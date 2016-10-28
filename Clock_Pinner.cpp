#include "Clock_Pinner.hpp"

Clock_Pinner::Clock_Pinner() :
  runType(NONE),
  haveOffset(false),
  estOffset(0),
  maxError(0)
{
  hi[VALID] = hi[INVALID] = 0;
  lo[VALID] = lo[INVALID] = 0;
};

bool
Clock_Pinner::accept(Timestamp ts, Timestamp_Type type) {

  // if just extending the previous run, we don't estimate
  if (type == runType) {
    hi[type] = std::max(ts, hi[type]);
    lo[type] = std::min(ts, lo[type]);
    return false;
  }

  // mark the type of the new run
  runType = type;

  // if there isn't already a run of each type, we can't estimate,
  // so just record timestamp as both lo and hi of new run

  if (lo[type] == 0 || lo[1 - type] == 0) {
    lo[type] = hi[type] = ts;
    return false;
  }

  // there are previous runs of both types, so we can estimate
  // the clock offset by bracketing the latest run of the other
  // type between the current time and the previous run of this type.

  // We estimate by pinning the midpoints of the intervals:
  // [hi[type], ts] and [lo[1-type], hi[1-type]]

  Timestamp estOffset = (hi[type] + ts) / 2.0 - (lo[1 - type] + hi[1 - type]) / 2.0;

  // ensure the offset always represents VALID - INVALID
  if (type == INVALID)
    estOffset = - estOffset;

  // Max error is 1/2 the absolute difference of the sizes of the timespans
  // of the inner run and the outer one.

  // Note use of fabs since we're not guaranteed hi[type] < t, due to
  // the possibility of time reversals.

  Timestamp maxError = fabs(fabs(ts - hi[type]) - (hi[1-type] - lo[1 - type])) / 2.0;

  if (! haveOffset || maxError < this->maxError) {
    this->estOffset = estOffset;
    this->maxError = maxError;
    haveOffset = true;
  }

  // set lo, hi of new run to ts

  lo[type] = hi[type] = ts;

  return true;
};
