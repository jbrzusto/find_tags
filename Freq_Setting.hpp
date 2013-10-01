#ifndef FREQ_SETTING_HPP
#define FREQ_SETTING_HPP

#include "find_tags_common.hpp"

#include <set>

// A set of nominal receiver frequences
typedef std::set < Nominal_Frequency_kHz > Freq_Set;

class Freq_Setting {

 public:
  Frequency_MHz		f_MHz;
  Nominal_Frequency_kHz f_kHz;
  Timestamp		ts;

 private:
  static Freq_Set      nominal_freqs;

 public:
  Freq_Setting(Frequency_MHz f_MHz=0, Timestamp ts=0);

  static Nominal_Frequency_kHz as_Nominal_Frequency_kHz(Frequency_MHz x);
  static Frequency_MHz as_Frequency_MHz(Nominal_Frequency_kHz x);
  static Nominal_Frequency_kHz get_closest_nominal_freq(Frequency_MHz freq);
  static void set_nominal_freqs(const Freq_Set & nominal_freqs);
};

#endif // FREQ_SETTING_HPP
