#include "Clock_Pinner.hpp"

Clock_Pinner::Clock_Pinner() :
  runType(NONE),
  haveOffset(false),
  estOffset(0),
  maxError(0)
{
  hi[VALID] = hi[INVALID] = 0;
  lo[VALID] = lo[INVALID] = 0;
  runLen[VALID] = runLen[INVALID ] = 0;
};

bool
Clock_Pinner::accept(Timestamp ts, Timestamp_Type type) {

  // if just extending the previous run, we don't estimate
  if (type == runType) {
    ++runLen[type];
    // extend run endpoints, allowing for inversions
    hi[type] = std::max(ts, hi[type]);
    lo[type] = std::min(ts, lo[type]);
    return false;
  }

  // mark the type and length of the new run
  runType = type;
  runLen[type] = 1;

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

  // Note use of fabs(ts - hi[type]) since we're not guaranteed
  // hi[type] < t, due to the possibility of time reversals.  The
  // outer fabs() shouldn't be necessary, but might be, due to
  // reversals that could make the inner interval appear longer than
  // the outer one.

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

void
Clock_Pinner::force_estimate() {
  // there will be no more invalid timestamps, so force an estimate.
  // returns true if an estimate has been made

  // nothing to do if we already have an offset estimate
  if (haveOffset)
    return;

  // if there have been no invalid timestamps, we correct with 0 offsets
  if (runLen[INVALID] == 0) {
    haveOffset = true;
    estOffset = 0;
    maxError = 0;
    return;
  }

  // We've had a run of timestamps from one clock type, and are in a
  // run from the other clock type, so there is no bracketing.
  //
  // As a crude but simple approach, we estimate the rate at which
  // timestamps are being emitted overall, by looking at total
  // elapsed time in the two runs, and dividing by (n1 + n2 - 2).
  // Provided n1 + n2 > 2, this gives us a rate (r) of timestamps
  // per second.  We estimate the error as 1/2r, and pin the clocks
  // using the equation abs(t[1,n] - t[2,1]) = 1 / r
  // where t[1,n] is the last timestamp of the first run, and t[2,1]
  // is the first timestamp of the second run.

  // Thus we assume the elapsed time between the changeover timestamps
  // is 1 / r; i.e. the average inter-timestamp interval.

  // In the exceptional case where we have only a single timestamp
  // from each clock, we pin the two values to each other, and
  // flag the impossibility of error estimation by setting the
  // error to -1.

  int n = runLen[VALID] + runLen[INVALID] - 2;
  if (n > 0) {
    double r = ((hi[VALID] - lo[VALID]) + (hi[INVALID] - lo[INVALID])) / n;
    estOffset = lo[runType] - hi[1 - runType] + 1.0 / r;
    if (runType == INVALID)
      estOffset = - estOffset;
    maxError = 1.0 / (2.0 * r);
  } else {
    estOffset = lo[runType] - hi[1 - runType];
    maxError = -1.0;
  }
}
