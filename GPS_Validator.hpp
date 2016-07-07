#ifndef GPS_VALIDATOR_HPP
#define GPS_VALIDATOR_HPP

#include "find_tags_common.hpp"

class GPS_Validator {

//!<   GPS_Validator - check whether the GPS is stuck
//
//   A GPS_Validator accepts a sequence of timestamps from the GPS
//   clock and from the pulse clock.  If two consecutive GPS fixes
//   have the same timestamp, but show at least 10 minutes of elapsed
//   time by the pulse clock, then the GPS is deemed stuck.

public:

  GPS_Validator(Timestamp thresh=10 * 60); //!< ctor

  //!< accept a timestamp, from either pulse or GPS clock 
  // return true iff the GPS is known to be stuck
  bool accept(Timestamp ts, bool isPulse);

protected:

  Timestamp      thresh;       //!< minimum seconds between identical GPS timestamps to conclude GPS is stuck;
  Timestamp      lastGPSTS;    //!< most recent GPS timestamp
  Timestamp      pulseTSlo;    //!< earliest pulse timestamp in run
  Timestamp      pulseTShi;    //!< latest pulse timestamp in run; when zero, indicates no invalid timestamps in run yet
  bool           stuck;        //!< is the most recent assessment that the GPS is stuck?

public:

  // public serialize function.

  template<class Archive>
  void serialize(Archive & ar, const unsigned int version)
  {
    ar & BOOST_SERIALIZATION_NVP( thresh );
    ar & BOOST_SERIALIZATION_NVP( lastGPSTS );
    ar & BOOST_SERIALIZATION_NVP( pulseTSlo );
    ar & BOOST_SERIALIZATION_NVP( pulseTShi );
    ar & BOOST_SERIALIZATION_NVP( stuck );
  };
};


#endif // GPS_VALIDATOR
