#include "Pulse.hpp"

Pulse::Pulse() :
  ts(0),
  dfreq(0),
  ant_freq(0),
  sig(0),
  noise(0),
  seq_no(0),
  line_no(0)
{};

Pulse::Pulse(double ts, Frequency_Offset_kHz dfreq, float sig, float noise, Frequency_MHz ant_freq, Seq_No line_no):
  ts(ts),
  dfreq(dfreq),
  ant_freq(ant_freq),
  sig(sig),
  noise(noise),
  line_no(line_no)
{
  this->seq_no = ++count;
};

Pulse
Pulse::make(double ts, Frequency_Offset_kHz dfreq, float sig, float noise, Frequency_MHz ant_freq, Seq_No line_no) {
  return Pulse(ts, dfreq, sig, noise, ant_freq, line_no);
};

void
Pulse::dump(bool with_line_no) {
  // 14 digits in timestamp output yields 0.1 ms precision
  std::cout << std::setprecision(14) << ts << std::setprecision(3) << ',' << dfreq << ',' << sig << ',' << noise;
  if (with_line_no)
    std::cout << ',' << line_no;
  std::cout << endl;
};

Seq_No
Pulse::count = 0;
