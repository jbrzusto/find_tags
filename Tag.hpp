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

  Callback              cb;                             // when not null, a function to call each time this tag is detected
                                                        // The only use so far is to let the Ambiguity object know that a particular
                                                        // proxy tag has been detected, so that its clone information should be recorded.

  void *                cbData;                         // additional pointer to private data for this tag; typically used by the callback function

public:
  Tag(){};

  Tag(Motus_Tag_ID motusID, Frequency_MHz freq, Frequency_Offset_kHz dfreq, const std::vector < Gap > & gaps);

  void setCallback (Callback cb, void * cbData);
};

#endif // TAG_HPP
