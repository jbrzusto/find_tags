#ifndef FREQ_HISTORY_HPP
#define FREQ_HISTORY_HPP

#include "find_tags_common.hpp"
#include "Freq_Setting.hpp"

class Freq_History {
 private:
  bool		is_constant;
  Freq_Setting	curr;
  Freq_Setting	next;
  std::ifstream	history_file;
  string	filename;
  
 public:
  Freq_History(Frequency_MHz freq, Timestamp ts=0);
  Freq_History(const string filename);
  Freq_Setting get(Timestamp ts);
  
 private:
  void get_next_setting(bool must_exist=false);
};

#endif // FREQ_HISTORY_HPP
