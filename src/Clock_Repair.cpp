#include "Clock_Repair.hpp"

Clock_Repair::Clock_Repair() {
  init();
};

Clock_Repair::Clock_Repair(Data_Source *data, unsigned long long *line_no, DB_Filer * filer, Timestamp tol) :
  data(data),
  line_no(line_no),
  filer(filer),
  tol(tol),
  cp(),
  gpsv(),
  GPSstuck(false),
  correcting(false),
  offset(0.0),
  offsetError(0.0),
  buf()
{
  init();
};

void
Clock_Repair::init() {
  // set the max valid timestamp, allowing for 5 minutes of slop
  max_ts = time_now() + 300;
};

//!< handle a record from an SG file; return TRUE if any
// required clock repairs can now be made.

bool
Clock_Repair::handle( SG_Record & r) {

  // run it past the GPS validator, to look for a stuck GPS

  if (r.type == SG_Record::PULSE || r.type == SG_Record::GPS) {
    GPSstuck = gpsv.accept(r.ts, r.type == SG_Record::PULSE);

    // skip stuck GPS records
    if (GPSstuck && r.type == SG_Record::GPS)
      return false;
  }

  // correct all monotonic timestamps to pre-GPS timestamps
  if (isMonotonic(r.ts))
    r.ts += TS_BEAGLEBONE_BOOT;

  // do clock correction estimation logic

  if (cp.accept(r.ts, isValid(r.ts) ? Clock_Pinner::VALID : Clock_Pinner::INVALID)
      && cp.max_error() <= tol) {
    // we have a good enough estimate for correcting the monotonic clock
    got_estimate();
  }

  // as soon as a pulse timestamp is valid, there will be no further
  // monotonic or pre-GPS timestamps, so we have to use whatever
  // correction is available to this point

  if (r.type == SG_Record::PULSE && isValid(r.ts)) {
    cp.force_estimate();
    got_estimate();
  }
  return correcting;
};

//!< indicate there are no more input records in the current batch
void
Clock_Repair::got_estimate() {
  // record time jump

  offset = cp.offset();
  offsetError = cp.max_error();
  correcting = true;
};

//!< get the next record available for processing, and return true.
// if no records are available, return false.
bool
Clock_Repair::read_record(SG_Record & r) {
  while (data->getline(buf, MAX_LINE_SIZE)) {
    ++ *line_no;
    r.from_buf(buf);
    if (r.type == SG_Record::BAD) {
      if (++num_bad_line_warnings <= MAX_BAD_LINE_WARNINGS ) {
        std::cerr << "Warning: malformed line in input\n  at line " << * line_no << ":\n" << (string("") + buf) << std::endl;
        if (num_bad_line_warnings == MAX_BAD_LINE_WARNINGS)
          std::cerr << "(skipping further warnings about this)" << std::endl;
      }
      continue;
    }
    if (r.ts > max_ts
        || (isMonotonic(r.ts) && r.ts + TS_BEAGLEBONE_BOOT > max_ts)
        || (isPreGPS(r.ts) && offset > 0.0 && r.ts + offset > max_ts)) {
      // timestamp too large!
      continue;
    }
    return true;
  }
  return false;
};

bool
Clock_Repair::get(SG_Record &r) {
//!< get the next record available for processing, correct its
// timestamp if necessary, and return true.  if no records are
// available, return false.

  if (! correcting) {
  // process records until we can correct
    while (!correcting) {
      if (! read_record(r)) {
        // no more records left so force a correction estimate
        cp.force_estimate();
        got_estimate();
        break;
      }
      handle(r);
    }
    filer->add_time_fix(TS_BEAGLEBONE_BOOT, TS_SG_EPOCH, offset, offsetError, 'S');
    data->rewind();
    GPSstuck = false;  // on next round, unstick GPS so we get initial run of non-stuck records
  }
  if (! read_record(r))
    return false;
  if (isMonotonic(r.ts))
    // always correct monotonic timestamps to pre-GPS
    r.ts += TS_BEAGLEBONE_BOOT;

  if (isPreGPS(r.ts))
    // correct the pre-GPS timestamps
    r.ts += offset;
  return true;
};

Timestamp
Clock_Repair::max_ts = 0;

int
Clock_Repair::num_bad_line_warnings = 0;

double
time_now() {
  struct timespec tsp;
  clock_gettime(CLOCK_REALTIME, & tsp);
  return  tsp.tv_sec + 1e-9 * tsp.tv_nsec;
};
