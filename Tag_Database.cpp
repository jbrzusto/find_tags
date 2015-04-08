#include "Tag_Database.hpp"
#include <sqlite3.h>

Tag_Database::Tag_Database(string filename) {
  if (filename.substr(filename.length() - 4) == ".csv")
    populate_from_csv_file(filename);
  else if (filename.substr(filename.length() - 7) == ".sqlite")
    populate_from_sqlite_file(filename);
  else
    throw std::runtime_error("Tag_Database: unrecognized file type; name must end in '.csv' or '.sqlite'");
};

void
Tag_Database::populate_from_sqlite_file(string filename) {
  
  sqlite3 * db; //<! handle to sqlite connection
  
  if (SQLITE_OK != sqlite3_open_v2(filename.c_str(),
                                   & db,
                                   SQLITE_OPEN_READONLY,
                                   0))
    throw std::runtime_error("Couldn't open tag database file");

  sqlite3_stmt * st; //!< pre-compiled statement for recording raw pulses

  if (SQLITE_OK != sqlite3_prepare_v2(db, "select proj, id, tagFreq, fcdFreq, dfreq, g1/1000.0, g2/1000.0, g3/1000.0, bi from tags order by tagFreq, id",
                                      -1, &st, 0)) 
    throw std::runtime_error("Sqlite tag database does not have the required columns: proj, id, tagFreq, fcdFreq, dfreq, g1, g2, g3, bi");

  while (SQLITE_DONE != sqlite3_step(st)) {
    int id;
    float freq_MHz, fcd_freq, gaps[4], dfreq;
    const char * proj = (const char *) sqlite3_column_text(st, 0);
    id = sqlite3_column_int(st, 1);
    freq_MHz = sqlite3_column_double(st, 2);
    fcd_freq = sqlite3_column_double(st, 3);
    dfreq = sqlite3_column_double(st, 4);
    for (int i = 0; i < 4; ++i)
      gaps[i] = sqlite3_column_double(st, 5 + i);
    Nominal_Frequency_kHz nom_freq = Freq_Setting::as_Nominal_Frequency_kHz(freq_MHz);
    if (nominal_freqs.count(nom_freq) == 0) {
      // we haven't seen this nominal frequency before
      // add it to the list and create a place to hold stuff
      nominal_freqs.insert(nom_freq);
      tags[nom_freq] = Tag_Set();
    }
    tags[nom_freq].insert (new Known_Tag (id, string(proj), freq_MHz, fcd_freq, dfreq, &gaps[0]));
  };
  sqlite3_finalize(st);
  sqlite3_close(db);
  if (tags.size() == 0)
    throw std::runtime_error("No tags in database.");
}

void
Tag_Database::populate_from_csv_file(string filename) {

  ifstream inf(filename.c_str(), ifstream::in);
  char buf[MAX_LINE_SIZE + 1];

  inf.getline(buf, MAX_LINE_SIZE);
  if (string(buf) != "\"proj\",\"id\",\"tagFreq\",\"fcdFreq\",\"g1\",\"g2\",\"g3\",\"bi\",\"dfreq\",\"g1.sd\",\"g2.sd\",\"g3.sd\",\"bi.sd\",\"dfreq.sd\",\"filename\"")
    throw std::runtime_error("Tag database file does not have required header with these columns: proj,id,tagFreq,fcdFreq,g1,g2,g3,bi,dfreq,g1.sd,g2.sd,g3.sd,bi.sd,dfreq.sd,filename");

  int num_lines = 1;
  while (! inf.eof()) {
    inf.getline(buf, MAX_LINE_SIZE);
    if (inf.gcount() == 0)
      break;
    ++num_lines;

    char proj[MAX_LINE_SIZE+1], filename[MAX_LINE_SIZE];
    int id;
    float freq_MHz, fcd_freq, gaps[4], dfreq, g1sd, g2sd, g3sd, bisd, dfreqsd;
    int num_par = sscanf(buf, "\"%[^\"]\",%d,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,\"%[^\"]\"", proj, &id, &freq_MHz, &fcd_freq, &gaps[0], &gaps[1], &gaps[2], &gaps[3], &dfreq, &g1sd, &g2sd, &g3sd, &bisd, &dfreqsd, filename);
    if (num_par < 15) {
      std::cerr << "Ignoring faulty line " << num_lines << " of tag database file:\n   " << buf << std::endl;
      continue;
    }
    Nominal_Frequency_kHz nom_freq = Freq_Setting::as_Nominal_Frequency_kHz(freq_MHz);
    // convert gaps to seconds
    for (int i=0; i < 3; ++i)
      gaps[i] /= 1000.0;

    if (nominal_freqs.count(nom_freq) == 0) {
      // we haven't seen this nominal frequency before
      // add it to the list and create a place to hold stuff
      nominal_freqs.insert(nom_freq);
      tags[nom_freq] = Tag_Set();
    }
    tags[nom_freq].insert (new Known_Tag (id, string(proj), freq_MHz, fcd_freq, dfreq, &gaps[0]));
  };
  if (tags.size() == 0)
    throw std::runtime_error("No tags registered.");
};

Freq_Set & Tag_Database::get_nominal_freqs() {
  return nominal_freqs;
};

Tag_Set *
Tag_Database::get_tags_at_freq(Nominal_Frequency_kHz freq) {
  return & tags[freq];
};

Known_Tag *
Tag_Database::get_tag(Tag_ID id) {
  return id;
};


