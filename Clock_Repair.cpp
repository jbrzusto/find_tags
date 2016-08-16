#include "Clock_Repair.hpp"

Clock_Repair::Clock_Repair(DB_Filer * filer, Timestamp monoTol) :
  filer(filer),
  monoTol(monoTol),
  cp(),
  gpsv(),
  recBuf(),
  correcting(false),
  havePreGPSoffset(false),
  haveMonotonicOffset(false),
  pulseClock(CS_UNKNOWN),
  GPSstuck(false),
  preGPSTS (0)
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

  // do clock correction estimation logic, which depends on record type

  switch (r.type) {
  case SG_Record::PULSE:
    if (isMonotonic(r.ts)) {
      pulseClock = CS_MONOTONIC;
      cp.accept(r.ts, Clock_Pinner::INVALID);
    } else if (isValid(r.ts)) {
      pulseClock = CS_REALTIME;
      if (preGPSTS > 0) {
        // calculate the offset for correcting preGPS timestamps
        preGPSOffset = r.ts - preGPSTS;
        havePreGPSoffset = true;
      }
      correcting = true;  // we're ready to correct all timestamps
    } else {
      pulseClock = CS_REALTIME_PRE_GPS;
      preGPSTS = r.ts;
    }
    break;

    // timestamps for PARAM and GPS records are never MONOTONIC; for GPS, they
    // are VALID (or stuck), for PARAM they are pre-GPS or VALID.

  case SG_Record::PARAM:
  case SG_Record::GPS:
    if (isPreGPS(r.ts)) {
      preGPSTS = r.ts;
      break; // should only happen for PARAM records
    }
    // timestamp must be valid

    if (preGPSTS > 0) {
        // calculate the offset for correcting preGPS timestamps
        preGPSOffset = r.ts - preGPSTS;
        havePreGPSoffset = true;
        if (pulseClock == CS_REALTIME_PRE_GPS) {
          // pulse clock is known to not be using MONOTONIC clock, 
          // so we now know enough to correct all timestamps
          correcting = true;
          break;
        }
    }
    if(cp.accept(r.ts, Clock_Pinner::VALID)) {
          // we have a good enough estimate for correcting the monotonic clock
          haveMonotonicOffset = true;
          monotonicOffset = cp.offset();
          monotonicError = cp.max_error();
          correcting = true;
    }
    break;

  default:
    break;
  }
};

//!< indicate there are no more input records in the current batch
void
Clock_Repair::done() {
  // record time jumps, if any

  if (haveMonotonicOffset)
    filer->add_time_fix(0, TS_BEAGLEBONE_BOOT, monotonicOffset, monotonicError, 'M'); // indicate fix due to pinning MONOTONIC clock

  if (havePreGPSoffset)
    filer->add_time_fix(TS_BEAGLEBONE_BOOT, TS_SG_EPOCH, preGPSOffset, preGPSError, 'S'); // indicate fix due to setting clock from GPS
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
  if (isMonotonic(r.ts)) {
    // correct the monotonic timestamp
    r.ts += monotonicOffset;
  } else if (isPreGPS(r.ts)) {
    // correct the pre-GPS timestamp
    r.ts += preGPSOffset;
  }
  recBuf.pop();
  return true;
};

