#ifndef TAG_DATABASE_HPP
#define TAG_DATABASE_HPP

#include "find_tags_common.hpp"

#include "Freq_Setting.hpp"
#include "Known_Tag.hpp"

#include <map>

class Tag_Database {

 private:
  typedef std::map < Nominal_Frequency_kHz, Tag_Set > Tag_Set_Set;
  
  Tag_Set_Set tags;

  Freq_Set nominal_freqs;

public:
  Tag_Database (string filename);

  Freq_Set & get_nominal_freqs();

  Tag_Set * get_tags_at_freq(Nominal_Frequency_kHz freq);
};

#endif // TAG_DATABASE_HPP
