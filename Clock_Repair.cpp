#include "Clock_Repair.hpp"

Clock_Repair::Clock_Repair(DB_Filer * filer, Timestamp tol) :
  filer(filer),
  tol(tol),
  cp(),
  gpsv(),
  recBuf(),
  GPSstuck(false),
  correcting(false),
  offset(0.0),
  offsetError(0.0)
{

};

//!< accept a record from an SG file
void
Clock_Repair::put( SG_Record & r) {

  // run it past the GPS validator, to look for a stuck GPS

  if (r.type == SG_Record::PULSE || r.type == SG_Record::GPS) {
    GPSstuck = gpsv.accept(r.ts, r.type == SG_Record::PULSE);

    // skip stuck GPS records
    if (GPSstuck && r.type == SG_Record::GPS)
      return;
  }

  recBuf.push(r);

  if (correcting)
    return; // we already know how to correct all timestamps

  // correct all monotonic timestamps to pre-GPS timestamps
  if (isMonotonic(r.ts))
    r.ts += TS_BEAGLEBONE_BOOT;

  // do clock correction estimation logic

  if (cp.accept(r.ts, isValid(r.ts) ? Clock_Pinner::VALID : Clock_Pinner::INVALID)
      && cp.max_error() <= tol) {
          // we have a good enough estimate for correcting the monotonic clock
          offset = cp.offset();
          offsetError = cp.max_error();
          correcting = true;
  }

  // as soon as a pulse timestamp is valid, there will be no further
  // monotonic or pre-GPS timestamps, so we have to use whatever
  // correction is available to this point

  if (r.type == SG_Record::PULSE && isValid(r.ts)) {
    cp.force_estimate();
    offset = cp.offset();
    offsetError = cp.max_error();
    correcting = true;
  }
};

//!< indicate there are no more input records in the current batch
void
Clock_Repair::done() {
  // record time jumps, if any

  if (correcting)
    filer->add_time_fix(TS_BEAGLEBONE_BOOT, TS_SG_EPOCH, offset, offsetError, 'S');
};

//!< get the next record available for processing, and return true.
// if no records are available, return false.
bool
Clock_Repair::get(SG_Record &r) {
  if (recBuf.size() == 0 || ! correcting)
    return false;

  // copy from buffer
  r = recBuf.front();

  // correct the timestamp

  if (isMonotonic(r.ts))
    // always correct monotonic timestamps to pre-GPS
    r.ts += TS_BEAGLEBONE_BOOT;

  if (isPreGPS(r.ts))
    // correct the pre-GPS timestamps
    r.ts += offset;

  recBuf.pop();
  return true;
};
