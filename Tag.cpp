#include "Tag.hpp"

#include <sstream>
#include <math.h>

Tag::Tag(Motus_Tag_ID motusID, Frequency_MHz freq, Frequency_Offset_kHz dfreq, const std::vector < Gap > & gaps):
  motusID(motusID),
  freq(freq),
  dfreq(dfreq),
  gaps(gaps)
{
  period = 0;
  for (auto i = gaps.begin(); i != gaps.end(); ++i)
    period += *i;
};
