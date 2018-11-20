#include "Pulse.hpp"

Pulse::Pulse(double ts, Frequency_Offset_kHz dfreq, float sig, float noise, Frequency_MHz ant_freq):
  ts(ts),
  dfreq(dfreq),
  ant_freq(ant_freq),
  sig(sig),
  noise(noise)
{ 
  static Seq_No seq_no = 0;

  this->seq_no = ++seq_no;
};

Pulse Pulse::make(double ts, Frequency_Offset_kHz dfreq, float sig, float noise, Frequency_MHz ant_freq) {
  return Pulse(ts, dfreq, sig, noise, ant_freq);
};

void Pulse::dump() {
  // 14 digits in timestamp output yields 0.1 ms precision
  std::cout << std::setprecision(14) << ts << std::setprecision(3) << ',' << dfreq << ',' << sig << ',' << noise << endl;
};
