#ifndef FIND_TAGS_COMMON_HPP
#define FIND_TAGS_COMMON_HPP

#include <set>
#include <unordered_set>

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

// Tag IDs: so far, Lotek tags have 3 digit IDs.  But we want to be
// able to distinguish among tags with the same Lotek ID but
// (sufficiently) different burst intervals.   Also, we 
// want to be able to give those tags a simple ID which a user in
// the field can use to distinguish among tags.  Using the BI
// is not helpful as it is hard to label a tag with that.
// Instead, tags can be given additional indelible marks, and
// the count of such marks distinguishes between tags with the
// same freq and ID label in the hand.
//
// So normally, there are no marks and the tag ID is an integer.
// e.g. 123 which can be thought of as 123.0
// When a project has a second tag with ID 123, it is given an
// indelible mark, and that tag ID is given as 123.1
// If a third tag has ID 123, it is given two marks and called
// 123.2, and so on.

typedef float Lotek_Tag_ID;
static const Lotek_Tag_ID BOGUS_LOTEK_TAG_ID = -1;

// type representing an internal tag ID; each entry in the database
// receives its own internal tag ID

class Known_Tag;
typedef Known_Tag * Tag_ID;
static const Tag_ID BOGUS_TAG_ID = 0;

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
