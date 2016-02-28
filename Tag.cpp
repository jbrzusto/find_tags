#include "Tag.hpp"

#include <sstream>
#include <math.h>

Tag::Tag(Motus_Tag_ID motusID, Frequency_MHz freq, Frequency_Offset_kHz dfreq, const std::vector < Gap > & gaps):
  motusID(motusID),
  freq(freq),
  dfreq(dfreq),
  gaps(gaps),
  count(0),
  cb(0),
  cbData(0)
{
  period = 0;
  for (auto i = gaps.begin(); i != gaps.end(); ++i)
    period += *i;
};

void
Tag::setCallback (Callback cb, void * cbData) {
  this->cb = cb;
  this->cbData = cbData;
};

