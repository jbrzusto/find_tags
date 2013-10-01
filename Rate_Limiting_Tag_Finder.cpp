#include "Rate_Limiting_Tag_Finder.hpp"

Rate_Limiting_Tag_Finder::Rate_Limiting_Tag_Finder (Nominal_Frequency_kHz nom_freq, Tag_Set * tags, Gap rate_window, float max_rate, Gap min_bogus_spacing, string prefix) :
  Tag_Finder(nom_freq, tags, prefix),
  rate_window(rate_window),
  max_rate(max_rate),
  min_bogus_spacing(min_bogus_spacing),
  last_bogus_emit_ts(-min_bogus_spacing),
  at_end(false)
{
};


void Rate_Limiting_Tag_Finder::process(Pulse &p) {

  // simple algorithm: wait until we have pulses spanning at least
  // rate_window seconds.  If the per-second rate in the buffer is
  // less than or equal to the max_rate, process the first pulse.
  // Otherwise, if at least min_bogus_spacing time has elapsed, emit a
  // bogus tag and discard the first pulse.  repeat this loop so long
  // as pulses in the buffer span at least rate_window time.

  if (! at_end)
    pulses.push_back(p);

  while(pulses.size() > 1) {
    Gap elapsed = pulses.back().ts - pulses.front().ts;
    unsigned int n = pulses.size();

    if ((elapsed < rate_window || n < 2) && !at_end) {
	break;
      }
    float r = (n - 1) / elapsed;
    Pulse &first_pulse = pulses.front();
    if (r <= max_rate) {
      Tag_Finder::process(first_pulse);
    } else {
      if ((! at_end) && (first_pulse.ts - last_bogus_emit_ts) >= min_bogus_spacing) {
	last_bogus_emit_ts = first_pulse.ts;
	dump_bogus_burst(first_pulse, 0);
      }
    }
    pulses.pop_front();
  }
};

void Rate_Limiting_Tag_Finder::end_processing() {
  Pulse p;
  at_end = true;
  process(p);
  Tag_Finder::end_processing();
};
