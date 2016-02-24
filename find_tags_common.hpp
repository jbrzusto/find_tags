#ifndef FIND_TAGS_COMMON_HPP
#define FIND_TAGS_COMMON_HPP

#include <set>
#include <unordered_set>
#include <vector>
#include <map>
#include <unordered_map>
#include <cmath>

const static unsigned int PULSES_PER_BURST = 4;	// pulses in a burst (Lotek VHF tags)
const static unsigned int MAX_LINE_SIZE = 512;	// characters in a .CSV file line

// type representing a timestamp, in seconds since the Epoch (1 Jan 1970 GMT)

typedef double Timestamp;
const static Timestamp BOGUS_TIMESTAMP = -1; // timestamp representing not-a-timestamp

// type representing a VHF frequency, in MHz

typedef double Frequency_MHz;

// type representing a frequency offset, in kHz

typedef float Frequency_Offset_kHz;

// type representing a nominal frequency

typedef int Nominal_Frequency_kHz;

// Tag IDs: we use integer primary keys into the Motus master tag database
// These are obtained by the harness code when it queries that database
// for tags to be sought.

typedef int Motus_Tag_ID;
static const Motus_Tag_ID BOGUS_MOTUS_TAG_ID = -1;

// type representing an internal tag ID; each entry in the database
// receives its own internal tag ID

class Tag;
typedef Tag * TagID;
static const TagID BOGUS_TAG = 0;

typedef std::unordered_set < TagID > TagSet; 

typedef short Phase;
static const Phase BOGUS_PHASE = -1;

typedef std::pair < TagID, Phase > TagPhase;
typedef std::unordered_multimap < TagID, Phase > TagPhaseSet;

// The type for interpulse gaps this should be able to represent a
// difference between two nearby timestamp values. We use float, which
// has sufficient precision here.

typedef float Gap;
typedef std::pair < Gap, Gap > GapRange;
typedef std::vector < GapRange > GapRanges;

// common standard stuff

#include <string>
using std::string;

#include <iostream>
using std::endl;
using std::ostream;

#include <iomanip>

#include <fstream>
using std::ifstream;

#include <stdexcept>

template<class CharType, class CharTraits>
std::basic_ostream<CharType, CharTraits> &operator<<
(std::basic_ostream<CharType, CharTraits> &stream, TagPhase const& value)
{
  if (value.second >= 0)
    return stream << "# " << value.first << " (" << value.second << ") ";
  return stream;
};

template<class CharType, class CharTraits>
std::basic_ostream<CharType, CharTraits> &operator<<
(std::basic_ostream<CharType, CharTraits> &stream, TagPhaseSet const& value)
{
  for (auto i = value.begin(); i != value.end(); ++i)
    stream << "\\n" << (*i);
  return stream;
};

#endif // FIND_TAGS_COMMON_HPP
