// Freq_Setting.cpp

#include "Freq_Setting.hpp"
#include <cmath>

Freq_Setting::Freq_Setting(Frequency_MHz f_MHz, Timestamp ts) :
  f_MHz(f_MHz),
  f_kHz(get_closest_nominal_freq(f_MHz)),
  ts(ts)
{};

Nominal_Frequency_kHz Freq_Setting::as_Nominal_Frequency_kHz(Frequency_MHz x) {
  return (Nominal_Frequency_kHz) roundf(x * 1000);
};

Frequency_MHz Freq_Setting::as_Frequency_MHz(Nominal_Frequency_kHz x) {
  return (Frequency_MHz) x / 1000.0;
};

Nominal_Frequency_kHz Freq_Setting::get_closest_nominal_freq(Frequency_MHz freq) {
  // easy failsafe
  if (nominal_freqs.size() == 0)
    return as_Nominal_Frequency_kHz(freq);

  // return the nominal frequency closest to the specified one
  // shortcut, in case freq *is* a nominal freq
  if (nominal_freqs.count(freq))
    return freq;

  Nominal_Frequency_kHz best = 0.0;
  double best_fit = 1e9;	// something stupidly large

  for (Freq_Set :: iterator it = nominal_freqs.begin(); it != nominal_freqs.end(); ++it) {
    double fit = fabs(freq - as_Frequency_MHz(*it));
    if ( fit < best_fit ) {
      best_fit = fit;
      best = *it;
    }
  }
  return best;
};


void Freq_Setting::set_nominal_freqs(const Freq_Set &nominal_freqs) {
  Freq_Setting::nominal_freqs = nominal_freqs;
};

Freq_Set Freq_Setting::nominal_freqs;

