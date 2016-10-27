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
  short                 mfgID;                          // manufacturer ID; only used for Lotek input data
  short                 codeSet;                        // codeset the ID is from; either '3' or '4', if a Lotek tag.  0 if undefined.
  bool                  active;                         // is the tag transmitting?  This field is set by History events.

public:
  Tag(){};

  Tag(Motus_Tag_ID motusID, Frequency_MHz freq, Frequency_Offset_kHz dfreq, short mfgID, short codeSet, const std::vector < Gap > & gaps);

  template<class Archive>
  void serialize(Archive & ar, const unsigned int version) {
    ar & BOOST_SERIALIZATION_NVP( motusID );
    ar & BOOST_SERIALIZATION_NVP( freq );
    ar & BOOST_SERIALIZATION_NVP( dfreq );
    ar & BOOST_SERIALIZATION_NVP( gaps );
    ar & BOOST_SERIALIZATION_NVP( period );
    ar & BOOST_SERIALIZATION_NVP( count );
  };
};

#endif // TAG_HPP
