#ifndef CLOCK_REPAIR_HPP
#define CLOCK_REPAIR_HPP

#include "find_tags_common.hpp"
#include "SG_Record.hpp"
#include "DB_Filer.hpp"
#include "Clock_Pinner.hpp"
#include "GPS_Validator.hpp"
#include "Data_Source.hpp"

class Clock_Repair {

// ## Clock_Repair - a filter that repairs timestamps in SG records
//
//   This class accepts a sequence of records from raw SG data files, and tries
//   to correct faulty timestamps.  When a reasonably good correction is possible,
//   the data source is rewound and timestamps are corrected before the
//   records are returned to the instance's user.
//
//   This class would not be necessary if the SG on-board software and the GPS
//   were working correctly.
//
//   ## Timeline for timestamps:
//
//    Era:   |  MONOTONIC  |  PRE_GPS           |  VALID
//           +-------------+--------------------+------------->
//           |             |                    |
//    Value: 0             946684800            1262304000
//    Name:                TS_BEAGLEBONE_BOOT   TS_SG_EPOCH
//    Date:                2000-01-01           2010-01-01
//
//   ## Offset added to timestamp to correct, by Era:
//
//     Era       Offset
//   ============================
//   MONOTONIC   OFFSET_MONOTONIC
//   PRE_GPS     OFFSET_PRE_GPS
//   VALID       0
//
//   i.e. timestamps in the VALID era don't need correcting.
//
//   And note also, that OFFSET_MONOTONIC = OFFSET_PRE_GPS + TS_BEAGLEBONE_BOOT
//
//   ## Sources of Timestamps
//
//   - pulse records; these use the timestamp obtained from ALSA, which
//   in some releases uses CLOCK_MONOTONIC rather than CLOCK_REALTIME.
//   These timestamps need to be corrected by adding a fixed value, since
//   both these clocks are adjusted in the same way by chronyd, except
//   that CLOCK_REALTIME uses the unix epoch, while CLOCK_MONOTONIC uses
//   the reboot epoch.  CLOCK_REALTIME is incorrect (see below) until
//   stepped from a GPS fix, and this might not happen for minutes, hours,
//   or even days (with poor GPS sky view).
//
//   - GPS records: the timestamp in these records is that obtained from
//   the GPS.  Normally, this timestamp is written to the raw data files
//   every 5 minutes, but if a user connects to the web interface and
//   manually asks to refresh the GPS fix, the result is also recorded to
//   raw files.  Sometimes, a GPS gets stuck (goes to sleep) and this
//   record does not vary.
//
//   - parameter settings records: these are rare, and usually only at
//   the start of a boot session, when funcubes are first recognized and
//   set to their listening frequency.  The timestamp is from
//   CLOCK_REALTIME.
//
//   ## Beaglebone Clock
//
//   - the beaglebone CLOCK_REALTIME begins at 1 Jan 2000 00:00:00 GMT
//   when the unit boots.  This is 946684800 = TS_BEAGLEBONE_BOOT
//   Timestamps earlier than this are assumed to be from a monotonic
//   clock.
//
//   - no SGs existed before 2010, so 1 Jan 2010 is a lower bound on valid
//   timestamps from raw SG files.  This is 1262304000 = TS_SG_EPOCH
//
//   - we run chronyd to try sync the clock to GPS time, by stepping and
//   slewing.
//
//   - correction to within a few seconds is done by a step from the GPS
//   at boot time, when possible.
//
// What needs doing:
//
// - for most receivers, look for the first jump in time from the non-VALID to
//   the VALID era, corresponding to the clock having been set from the GPS.
//   This jump is used as an estimate of OFFSET_PRE_GPS.  It should be corret
//   to within 5 minutes, the time between GPS records.
//
// - for receivers where pulse timestamps are taken from CLOCK_MONOTONIC
//   instead of CLOCK_REALTIME, wait until timestamps on non-pulse records are
//   valid, then try to estimate the offset between CLOCK_REALTIME and
//   CLOCK_MONTONIC to within 1s.  This can be used to back-correct two kinds
//   of timestamps:
//
//      - pulse timestamps on CLOCK_MONOTONIC: t <- t + OFFSET_MONTONIC
//
//      - param setting timestamps on CLOCK_REALTIME: t <- t - TS_BEAGLEBONE_BOOT + OFFSET_MONTONIC
//
// Design: two possibilities:
//
// - Filter: buffer SG records until we have a good clock, then pass them through to Tag_Foray
//     Pro:
//       - minimal change to Tag_Foray
//       - all clock correction logic stays in Clock_Repair class
//       - no need to code "restart" on sources
//     Con:
//       - it's possible that the entire batch will be buffered; e.g. if no GPS clock
//         setting happens at all.
//
// - Standalone: wait until we have a good clock, then restart the batch with corrections
//      Pro:
//        - no buffering required
//      Con:
//        - won't work with input coming from a pipe
//        - splits up clock fixing logic between Clock_Repair and Tag_Foray
//        - need to implement restarts on data sources
//
// To avoid arbitrarily large buffer sizes, we went with a slightly hybrid
// version of the two choices above.  We use rewinds() but keep all
// clock fixing logic in this class.

public:

  Clock_Repair();

  Clock_Repair(Data_Source *data, unsigned long long *line_no, DB_Filer * filer, Timestamp tol = 1); //!< ctor, with tolerance for bracketing correction to CLOCK_MONOTONIC

  void init();

  //!< get the next record available for processing, and return true.
  // if no (corrected) records are available, return false.
  bool get(SG_Record &r);

protected:

  //!< indicate there are no more input records
  void got_estimate();

  //!< handle a record from an SG file
  bool handle( SG_Record & r);

  //!< try read a record from the data source
  bool read_record( SG_Record & r);

  typedef enum {  // sources of timestamps (i.e. what kind of record in the raw file)
    TSS_PULSE = 0,  // pulse record
    TSS_GPS   = 1,  // GPS record
    TSS_PARAM = 2   // parameter setting record
  } TimestampSource;

  typedef enum {  // clock type from which timestamp was obtained
    CS_UNKNOWN          = -1, // not (yet?) determined
    CS_MONOTONIC        =  0, // monotonic SG system clock
    CS_REALTIME         =  1, // realtime SG system clock, after GPS sync
    CS_REALTIME_PRE_GPS =  2, // realtime SG system clock, before GPS sync
    CS_GPS              =  3  // GPS clock
  } ClockSource;

  static constexpr Timestamp TS_BEAGLEBONE_BOOT = 946684800;  // !<  2000-01-01 00:00:00
  static constexpr Timestamp TS_SG_EPOCH        = 1262304000; // !<  2010-01-01 00:00:00

  bool isValid(Timestamp ts) { return ts >= TS_SG_EPOCH; }; //!< is timestamp in VALID era?
  bool isMonotonic(Timestamp ts) { return ts < TS_BEAGLEBONE_BOOT; }; //!< is timestamp in MONOTONIC era?
  bool isPreGPS(Timestamp ts) { return ts >= TS_BEAGLEBONE_BOOT && ts < TS_SG_EPOCH; } //!< is timestamp in PRE_GPS era?

  Data_Source *data; //!< data source we get records from
  unsigned long long *line_no; //!< pointer to line number so we can update it
  DB_Filer * filer;   //!< for filing time corrections
  Timestamp tol;  //!< maximum allowed error (seconds) in correcting timestamps
  Clock_Pinner cp;    //!< for pinning CLOCK_PRE_GPS to CLOCK_REALTIME
  GPS_Validator gpsv; //!< for detecting a stuck GPS

  bool GPSstuck; // true iff we see the GPS is stuck, as determined by the GPS_Validator class
  bool correcting; //!< true if we're able to correct records

  Timestamp offset;
  Timestamp offsetError;

  char buf[MAX_LINE_SIZE + 1]; //!< input buffer

  //!< are pulses using CLOCK_MONOTONIC?
  bool clock_monotonic();

  static Timestamp max_ts; //!< maximum valid timestamp; records with larger timestamps are ignored.

  static constexpr int MAX_BAD_LINE_WARNINGS = 5; // maximum number of bad line warnings to issue
  static int num_bad_line_warnings; // number of bad line warnings issued

public:

  // public serialize function.

  template<class Archive>
  void serialize(Archive & ar, const unsigned int version)
  {
    ar & BOOST_SERIALIZATION_NVP( tol );
    ar & BOOST_SERIALIZATION_NVP( cp );
    ar & BOOST_SERIALIZATION_NVP( gpsv );
    ar & BOOST_SERIALIZATION_NVP( correcting );
    ar & BOOST_SERIALIZATION_NVP( offset );
    ar & BOOST_SERIALIZATION_NVP( offsetError );
  };
};


#endif // CLOCK_REPAIR
