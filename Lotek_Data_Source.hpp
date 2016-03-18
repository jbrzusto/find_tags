#ifndef LOTEK_DATA_SOURCE_HPP
#define LOTEK_DATA_SOURCE_HPP

// NOTE: This data source converts Lotek tag detections back to
// sequences of 4 pulses, simulating what an SG would have seen.  When
// two detections are very close together in time, it's possible that
// some of the pulses from the second detection occur earlier than
// some of the pulses from the first detection.  Moreover, in some versions
// of Lotek receiver firmware, detection timestamps are not guaranteed to
// be monotonic (time can sometimes jump backwards when the clock is corrected
// by GPS, or due to other issues).
// 
// Therefore, we buffer output pulses in chronological order, and
// don't pass them to the tag finder until a fixed lead time has
// elapsed, as judged by the timestamps on incoming detections.
// e.g. once a detection arrives with a timestamp at least 10 seconds
// later than anything in the buffer, we judge that the buffer
// contents are now "final", and can be passed to the tag finder, on
// the assumption that backward time jumps are by less than 10 seconds.
//
// This presents a dilemma when pausing/resuming tag finding (e.g.
// when field data are received in batches, weeks apart):
// 
// Either
//
// A: we process all detections before a pause, then the first few
//    detections after a resume might have timestamps earlier than the
//    last pre-pause pulse timestamp, and these will be seen as time
//    reversals by the tag finder, and ignored.  This might result in
//    dropping the first few detections after a resume.
//
// or
//
// B: we save the buffered output pulses before a pause, and only
//    process them after a resume, once new detections have come in,
//    then the last few detections from before a pause might not
//    appear in the dataset until after processing resumes, which
//    might be weeks later.
//
// We opt for B, because then the detection dataset will ultimately
// be correct, even if it is not fully delivered on the most convenient
// schedule.


//!< Source for Lotek-format input data.

#include "find_tags_common.hpp"
#include "Data_Source.hpp"
#include "Tag_Database.hpp"
#include "Tag_Candidate.hpp"
#include <map>
#include <unordered_set>
#include <boost/serialization/map.hpp>
#include <boost/serialization/vector.hpp>
#include <boost/serialization/set.hpp>

class Lotek_Data_Source : public Data_Source { 

public:
  Lotek_Data_Source(std::istream * data, Tag_Database *tdb, Frequency_MHz defFreq);
  bool getline(char * buf, int maxLen);
  static const int MAX_LOTEK_LINE_SIZE = 100;
  static const int MAX_LEAD_SECONDS = 10;  //!< maximum number of
                                         //! seconds before we dump
                                         //! pulses from a tag; allows
                                         //! for small time reversals
                                         //! and interleaving of
                                         //! output pulses
  static const int MAX_ANTENNAS=10;      //!< maximum number of antennas


protected:
  std::istream *data;                                                     //!< pointer to true input stream
  std::map < std::pair < short , short > , std::vector < Gap > * > tcode; //!< map (codeset, ID) to pulse gaps from the tag DB
  char ltbuf[MAX_LOTEK_LINE_SIZE];                                        //!< buffer for reading true input stream
  bool done;                                                              //!< true if input stream is finished
  std::map < double, std::string > sgbuf;                                 //!< buffer of SG-format lines
  Timestamp latestInputTS;                                                //!< timestamp of most recent input line
  std::vector < Frequency_MHz > antFreq;                                  //!< most recent listen frequency on each antenna, in MHz
  std::set < std::pair < short, short > > warned;                         //!< sets of tag/codeset combos for which 'non-existent' warning has been issued

  // methods

  bool getInputLine(); //!< get line from true input stream; return
                       //! false on EOS

  bool translateLine(); //!< parse

  void serialize(boost::archive::binary_iarchive & ar, const unsigned int version);
  void serialize(boost::archive::binary_oarchive & ar, const unsigned int version);

};

#endif // LOTEK_DATA_SOURCE
