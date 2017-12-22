#ifndef TAG_DATABASE_HPP
#define TAG_DATABASE_HPP

#include "find_tags_common.hpp"

#include "Freq_Setting.hpp"
#include "Tag.hpp"
#include "History.hpp"

#include <map>

class Tag_Database {

private:
  typedef std::map < Nominal_Frequency_kHz, TagSet > TagSetSet;

  TagSetSet tags;

  Freq_Set nominal_freqs;

  std::map < Motus_Tag_ID, Tag * > motusIDToPtr;

  History *h;

  std::string db_hash; // commit hash of metadatabase corresponding to tags and events tables when read in populate_from_sqlite_file

public:
  Tag_Database (); //!< default ctor for deserializing into

  Tag_Database (string filename, bool get_history = false);

  void populate_from_csv_file(string filename);

  void populate_from_sqlite_file(string filename, bool get_history);

  Motus_Tag_ID get_max_motusID();

  Freq_Set & get_nominal_freqs();

  TagSet * get_tags_at_freq(Nominal_Frequency_kHz freq);

  Tag * getTagForMotusID (Motus_Tag_ID mid);

  History * get_history();

  std::string & get_db_hash();

  template<class Archive>
  void serialize(Archive & ar, const unsigned int version)
  {
    ar & BOOST_SERIALIZATION_NVP( tags );
    ar & BOOST_SERIALIZATION_NVP( nominal_freqs );
    ar & BOOST_SERIALIZATION_NVP( motusIDToPtr );
    ar & BOOST_SERIALIZATION_NVP( h );
  };
};

#endif // TAG_DATABASE_HPP
