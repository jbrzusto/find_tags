#ifndef RATE_LIMITING_TAG_FINDER_HPP
#define RATE_LIMITING_TAG_FINDER_HPP

// A tag finder with pre-filtering of periods with excessive pulse
// rate.

#include "find_tags_common.hpp"
#include "Pulse.hpp"
#include "Tag_Finder.hpp"

#include <list>

class Rate_Limiting_Tag_Finder : public Tag_Finder {

private:
  typedef std::list < Pulse > Pulse_List;
  Pulse_List pulses;

  Gap rate_window;
  float max_rate;
  Gap min_bogus_spacing;
  Timestamp last_bogus_emit_ts;
  bool at_end;

public:
  Rate_Limiting_Tag_Finder() {};

  Rate_Limiting_Tag_Finder (Nominal_Frequency_kHz nom_freq, Tag_Set * tags, Gap rate_window, float max_rate, Gap min_bogus_spacing, string prefix="");

  virtual void process(Pulse &p);

  virtual void end_processing();

};

#endif // RATE_LIMITING_TAG_FINDER_HPP
