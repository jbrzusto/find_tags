#ifndef TAG_HPP
#define TAG_HPP

#include "find_tags_common.hpp"

struct Tag {

  // represents a database of tags we have "registered" (i.e. estimated
  // gaps, burst interval, and frequency offset from raw receiver audio)

public:

  typedef  void (*Callback)(Tag *);                     // type of function which can be called on tag detection

  Motus_Tag_ID		motusID;			// motus tag ID
  Frequency_MHz		freq;				// nominal transmit frequency (MHz)
  Frequency_Offset_kHz	dfreq;				// offset from nominal frequency observed by funcube at registration (kHz)
  std::vector < Gap >	gaps;	                        // gaps between pulses; a tag is assumed to transmit a sequence of pulses with 
  // gaps given (cyclically) by this vector
  Gap                   period;                         // sum of the gaps

  long long             count;                          // number of times this tag has been detected during a run of the program;
                                                        // The only use so far is to keep track of whether an Ambiguity Clone can be
                                                        // augmented or reduced without creating a new one.
public:
  Tag(){};

  Tag(Motus_Tag_ID motusID, Frequency_MHz freq, Frequency_Offset_kHz dfreq, const std::vector < Gap > & gaps);

  template<class Archive>
  void serialize(Archive & ar, const unsigned int version) {
    ar & motusID;
    ar & freq;
    ar & dfreq;
    ar & gaps;
    ar & period;
    ar & count;
  };
};

#endif // TAG_HPP
