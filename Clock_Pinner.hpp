#ifndef CLOCK_PINNER_HPP
#define CLOCK_PINNER_HPP

#include "find_tags_common.hpp"

class Clock_Pinner {

  //!<   Clock_Pinner - pin an invalid clock to a valid clock, maintaining an
  //   estimate of the error.
  //
  //   A Clock_Pinner accepts a sequence of timestamps from two clocks,
  //   one valid and one not.  The invalid clock is assumed to differ
  //   from the valid clock by an unknown but constant offset, and so to
  //   differ only negligably in rate.  This class tries to pin the
  //   invalid clock to the valid one, by maintaining a best estimate of
  //   the offset between them.

public:

  typedef enum {NONE=-1, VALID=0, INVALID=1} Timestamp_Type;

  Clock_Pinner(); //!< ctor

  //!< accept a timestamp; return true iff a new estimate of offset is available
  bool accept(Timestamp ts, Timestamp_Type type);

  //!< is there an estimate of the offset?
  bool have_offset() { return haveOffset;};

  //!< return the offset which must be added to the invalid clock to correct it
  Timestamp offset() { return estOffset;};

  //!< upper bound on the magnitude of error in the offset
  Timestamp max_error() { return maxError;};

  //!< force an estimate of offset and error; e.g. if the caller knows the
  // remaining timestamps are all from the same clock.
  // Returns true unless no estimate is possible.
  // After a return of true, have_offset() returns true, and offset() and max_error()
  // return their best estimates.

  void force_estimate();

protected:

  Timestamp      lo[2];         //!< earliest timestamp in run of type VALID or INVALID
  Timestamp      hi[2];         //!< latest timestamp in run of type VALID or INVALID
  int            runLen[2];     //!< length of most recent (possibly current) run of type VALID or INVALID
                                //!< kept in case we need to estimate an offset and error from only
                                //!< a run of one type followed by a singleton of the other.
  Timestamp_Type runType;       //!< what type of timestamp run are we in?
  bool           haveOffset;    //!< have we estimated the offset?
  Timestamp      estOffset;     //!< estimated offset
  Timestamp      maxError;      //!< max error in estimated offset

public:

  // public serialize function.

  template<class Archive>
  void serialize(Archive & ar, const unsigned int version)
  {
    ar & BOOST_SERIALIZATION_NVP( lo );
    ar & BOOST_SERIALIZATION_NVP( hi );
    ar & BOOST_SERIALIZATION_NVP( runLen );
    ar & BOOST_SERIALIZATION_NVP( runType );
    ar & BOOST_SERIALIZATION_NVP( haveOffset );
    ar & BOOST_SERIALIZATION_NVP( estOffset );
    ar & BOOST_SERIALIZATION_NVP( maxError );
  };
};

#endif // CLOCK_PINNER
