#include "Tag_Database.hpp"
#include <sqlite3.h>
#include <math.h>

Tag_Database::Tag_Database (string filename, bool get_history)
  : h(0)
{
  if (filename.substr(filename.length() - 7) == ".sqlite")
    populate_from_sqlite_file(filename, get_history);
  else
    throw std::runtime_error("Tag_Database: unrecognized file type; name must end in '.sqlite'");
};

void
Tag_Database::populate_from_sqlite_file(string filename, bool get_history) {
  
  sqlite3 * db; //<! handle to sqlite connection
  
  if (SQLITE_OK != sqlite3_open_v2(filename.c_str(),
                                   & db,
                                   SQLITE_OPEN_READONLY,
                                   0))
    throw std::runtime_error("Couldn't open tag database file");

  sqlite3_stmt * st; //!< pre-compiled statement for recording raw pulses

  if (SQLITE_OK != sqlite3_prepare_v2(db, "select tagID, nomFreq, offsetFreq, round(4*param1)/4000.0, round(4*param2)/4000.0, round(4*param3)/4000.0, round(4000 * period) / 4000 from tags order by nomFreq, tagID",
                                      -1, &st, 0)) 
    throw std::runtime_error("Sqlite tag database does not have the required columns: tagID, nomFreq, offsetFreq, param1, param2, param3, period");

  while (SQLITE_DONE != sqlite3_step(st)) {
    Motus_Tag_ID  motusID = (Motus_Tag_ID) sqlite3_column_int(st, 0);
    float freq_MHz = sqlite3_column_double(st, 1);
    float dfreq = sqlite3_column_double(st, 2);
    std::vector < Gap > gaps;
    double totalBurst = 0.0;
    for (int i = 0; i < 3; ++i) {
      double gap = sqlite3_column_double(st, 3 + i);
      gaps.push_back(gap);
      totalBurst += gap;
    }
    // subtract burst total from period to get final gap
    double gap = sqlite3_column_double(st, 6) - totalBurst;
    gaps.push_back(gap);
      
    Nominal_Frequency_kHz nom_freq = Freq_Setting::as_Nominal_Frequency_kHz(freq_MHz);
    if (nominal_freqs.count(nom_freq) == 0) {
      // we haven't seen this nominal frequency before
      // add it to the list and create a place to hold stuff
      nominal_freqs.insert(nom_freq);
      tags[nom_freq] = TagSet();
    }
    Tag * t = new Tag (motusID, freq_MHz, dfreq, gaps);
    tags[nom_freq].insert (t);
    if (get_history)
      motusIDToPtr[motusID] = t;
  };
  sqlite3_finalize(st);

  h = new History();  // empty history
  if (get_history) {
    if (SQLITE_OK != sqlite3_prepare_v2(db, "select ts, tagID, event from events order by ts",
                                        -1, &st, 0))
      throw std::runtime_error("You asked to use events but there is no 'events' table in the tag database.");
    while (SQLITE_DONE != sqlite3_step(st)) {
      Timestamp ts = (Timestamp) sqlite3_column_double(st, 0);
      Motus_Tag_ID  motusID = (Motus_Tag_ID) sqlite3_column_int(st, 1);
      int event = sqlite3_column_int(st, 2);
      auto p = motusIDToPtr.find(motusID);
      if (p == motusIDToPtr.end())
        throw std::runtime_error(std::string("Event refers to non-existent motus tag ID ") + std::to_string(motusID));
      h->push(ts, Event(p->second, event));
    }
    sqlite3_finalize(st);
  }
  
  sqlite3_close(db);
  if (tags.size() == 0)
    throw std::runtime_error("No tags in database.");
}

Freq_Set & Tag_Database::get_nominal_freqs() {
  return nominal_freqs;
};

TagSet *
Tag_Database::get_tags_at_freq(Nominal_Frequency_kHz freq) {
  return & tags[freq];
};

History *
Tag_Database::get_history() {
  return h;
};
