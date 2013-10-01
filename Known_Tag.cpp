#include "Known_Tag.hpp"

// Note: in the following, gaps points to an array of PULSES_PER_BURST gaps;
// the first PULSES_PER_BURST-1 gaps are intra burst gaps, but the last gap is the burst interval

Known_Tag::Known_Tag(Tag_ID id, string proj, Frequency_MHz freq, Frequency_MHz fcd_freq, Frequency_Offset_kHz dfreq, float *gaps):
  id(id),
  proj(proj),
  freq(freq),
  fcd_freq(fcd_freq),
  dfreq(dfreq),
  last_good(-1.0)
{
  // gaps are stored in the database as milliseconds
  float burst_length = 0.0;
  for (unsigned i = 0; i < PULSES_PER_BURST - 1; ++i) {
    burst_length += this->gaps[i] = gaps[i];
  }
  // calculate gap to start of next pulse, 
  this->gaps[PULSES_PER_BURST - 1] = gaps[PULSES_PER_BURST - 1] - burst_length;

  // store the burst interval
  this->gaps[PULSES_PER_BURST] = gaps[PULSES_PER_BURST - 1];
  if (burst_length > max_burst_length)
    max_burst_length = burst_length;
};

float 
Known_Tag::max_burst_length = 0.0;   // will be calculated as tags database is built
