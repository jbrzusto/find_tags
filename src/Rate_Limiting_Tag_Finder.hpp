#ifndef RATE_LIMITING_TAG_FINDER_HPP
#define RATE_LIMITING_TAG_FINDER_HPP

#include <boost/serialization/export.hpp>
#include <boost/serialization/base_object.hpp>


// A tag finder with pre-filtering of periods with excessive pulse
// rate.

#include "find_tags_common.hpp"
#include "Pulse.hpp"
#include "Tag_Finder.hpp"
#include "Graph.hpp"
#include "Tag_Foray.hpp"

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
  Rate_Limiting_Tag_Finder() {}; // default ctor for serialization

  Rate_Limiting_Tag_Finder(Tag_Foray *owner);

  Rate_Limiting_Tag_Finder (Tag_Foray *owner, Nominal_Frequency_kHz nom_freq, TagSet * tags, Graph * g, Gap rate_window, float max_rate, Gap min_bogus_spacing, string prefix="");

  virtual ~Rate_Limiting_Tag_Finder();

  virtual void process(Pulse &p);

  template<class Archive>
  void serialize(Archive & ar, const unsigned int version)
    {
        // serialize base class information
        ar & boost::serialization::base_object<Tag_Finder>(*this);
        ar & pulses;
        ar & rate_window;
        ar & max_rate;
        ar & min_bogus_spacing;
        ar & last_bogus_emit_ts;
        ar & at_end;
    };
};

#endif // RATE_LIMITING_TAG_FINDER_HPP
