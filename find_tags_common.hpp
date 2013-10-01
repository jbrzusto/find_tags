#ifndef FIND_TAGS_COMMON_HPP
#define FIND_TAGS_COMMON_HPP

#include <set>

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

// type representing a tag ID

typedef int Tag_ID;
static const Tag_ID BOGUS_TAG_ID = -1;

// a set of tag IDs;
typedef std::set< Tag_ID > Tag_ID_Set;
// an iterator for the set
typedef Tag_ID_Set::iterator Tag_ID_Iter;

// The type for interpulse gaps this should be able to represent a
// difference between two nearby Timestamp values We use float, which
// has sufficient precision here.

typedef float Gap;

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

#endif // FIND_TAGS_COMMON_HPP
