// class representing a receiver frequency history

#include "Freq_History.hpp"

Freq_History::Freq_History(Frequency_MHz freq, Timestamp ts) :
  is_constant(true),
  curr(freq, ts)
{};

Freq_History::Freq_History(const string filename) :
  is_constant(false),
  filename(filename)
{
  history_file.open(filename.c_str(), std::ios_base::in);
  if (history_file.fail())
    throw std::runtime_error(string("Couldn't open frequency settings file ") + filename);
  // get initial frequency setting, which must be present
  get_next_setting(true);
  
  // try get the subsequent setting, so we know when the frequency switches
  // this need not exist, as the frequency might only be set once
  get_next_setting(false);
};

Freq_Setting Freq_History::get(Timestamp ts) {
  // return the frequency at time ts
  for (;;) {
    if (is_constant || (ts >= curr.ts && ts < next.ts ))
      return curr;
    if (ts < curr.ts) {
      std::cerr << "Warning: pulse time " << ts << " is before antenna frequency setting time " << curr.ts << "; using most recent frequency setting." << std::endl;
      return curr;
    }
    get_next_setting();
  }
};


void Freq_History::get_next_setting(bool must_exist) {
  Timestamp tmp_ts = 0;
  Frequency_MHz tmp_freq = 0;
  char sep;
  if (!(history_file >> tmp_ts >> sep >> tmp_freq)) {
    if (must_exist)
      throw std::runtime_error(string("Couldn't read frequency settings from file ") + filename);
    else
      // no more settings in the file, so from now on it is constant
      is_constant = true;
  }
  curr = next;
  next = Freq_Setting(tmp_freq, tmp_ts);
};
