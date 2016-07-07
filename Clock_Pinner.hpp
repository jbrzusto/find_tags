#ifndef CLOCK_PINNER_HPP
#define CLOCK_PINNER_HPP

#include "find_tags_common.hpp"

class Clock_Pinner {

  //!<   Clock_Pinner - pin an invalid clock to a valid clock, maintaining an
  //   estimate of the error.
  //
  //   A Clock_Pinner accepts a sequence of timestamps from two clocks,
  //   one valid and one not.  The invalid clock is assumed to differ
  //   from the valid clock by an unknown but constant offset, and to
  //   differ only negligably in rate.  This class tries to pin the
  //   invalid clock to the valid one, by maintaining a best estimate of
  //   the offset between them.

public:

  typedef enum {NONE, VALID, INVALID} Timestamp_Type;

  Clock_Pinner(); //!< ctor

  //!< accept a timestamp; return true iff a new estimate of offset is available
  bool accept(Timestamp ts, Timestamp_Type type);

  //!< is there an estimate of the offset?
  bool have_offset() { return haveOffset;};

  //!< return the offset which must be added to the invalid clock to correct it
  Timestamp offset() { return estOffset;};

  //!< upper bound on the magnitude of error in the offset
  Timestamp max_error() { return maxError;};


protected:

  Timestamp      lastValidTS;    //!< most recent valid timestamp
  Timestamp      invalidTSlo;    //!< earliest invalid timestamp in run
  Timestamp      invalidTShi;    //!< latest invalid timestamp in run; when zero, indicates no invalid timestamps in run yet
  bool           haveOffset;     //!< have we estimated the offset?
  Timestamp      estOffset;      //!< estimated offset
  Timestamp      maxError;       //!< max error in estimated offset

public:

  // public serialize function.

  template<class Archive>
  void serialize(Archive & ar, const unsigned int version)
  {
    ar & BOOST_SERIALIZATION_NVP( lastValidTS );
    ar & BOOST_SERIALIZATION_NVP( invalidTSlo );
    ar & BOOST_SERIALIZATION_NVP( invalidTShi );
    ar & BOOST_SERIALIZATION_NVP( haveOffset );
    ar & BOOST_SERIALIZATION_NVP( estOffset );
    ar & BOOST_SERIALIZATION_NVP( maxError );
  };
};

#endif // CLOCK_PINNER
