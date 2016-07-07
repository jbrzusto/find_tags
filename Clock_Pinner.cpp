#include "Clock_Pinner.hpp"

Clock_Pinner::Clock_Pinner() :
  lastValidTS(0),
  invalidTShi(0),
  haveOffset(false),
  estOffset(0),
  maxError(0)
{
};

bool
Clock_Pinner::accept(Timestamp ts, Timestamp_Type type) {
  // assume we won't have a new offset estimate
  bool rv = false;

  if (type == INVALID) {
    // if we've seen a valid timestamp, we are accumulating subsequent
    // invalid ones
    if (lastValidTS) {
      if (! invalidTShi) {
        invalidTShi = invalidTSlo = ts;
      } else {
        invalidTSlo = std::min(ts, invalidTSlo);
        invalidTShi = std::max(ts, invalidTShi);
      }
    }
  } else if (type == VALID) {
    if (invalidTShi > 0) {

      // We've accumulated invalid timestamps since the last valid timestamp,
      // so estimate offset as the difference between the midpoints (MP) of the
      // valid and invalid timestamp ranges.  The maximum magnitude of the
      // pinning error is at most (elapsedValid - elapsedInvalid) / 2.
      //
      //                                         validMP
      //                                         |
      //    |<------------------- elapsedValid --+------------------------------>|
      //
      //                      |<---- elapsedInvalid ------------->|
      //                      |                     invalidMP     |
      // ---+-----------------+-------------+-------+-------...---+--------------+----->
      //    |                 |             |                     |              |
      //    |                 |             |                     |              |
      //    lastValidTS       invalidTS[1]  invalidTS[2]    ...   invalidTS[n]   ts (valid)
      //                      invalidTSlo                         invalidTShi

      Timestamp estOffset = (ts + lastValidTS) / 2 - (invalidTSlo + invalidTShi) / 2;
      Timestamp elapsedValid = ts - lastValidTS;
      Timestamp elapsedInvalid = invalidTShi - invalidTSlo;

      // sanity check: make sure the elapsed time on the invalid clock is no
      // more than 10% larger than the elapsed time on the valid clock.
      // Timestamps are supposed to have been provided in real time order, so
      // the elapsed invalid time should be smaller than the elapsed valid
      // time, but there might be some slop.  The slop is also the reason we
      // explicitly look for the earliest and latest invalid timestamps, rather
      // than just assuming these are the first and the last, respectively
      // (which the diagram above does).  Pulse timestamps in particular can
      // have time inversions, e.g. when pulses are detected on multiple
      // antennas, since each antenna's data is processed in chunks.

      bool okay = elapsedValid > 0 && elapsedInvalid / elapsedValid < 1.1;

      // maximum error is the 1/2 difference between time elapsed on
      // the valid clock and on the invalid clock, since we're pinning
      // at the midpoints of the two intervals.

      Timestamp maxError = fabs(elapsedValid - elapsedInvalid) / 2.0;

      // if there is no existing estimate, or if the new maxError is smaller
      // than the previous one, record this estimate
      if (okay && (! haveOffset || maxError < this->maxError)) {
        this->estOffset = estOffset;
        this->maxError = maxError;
        haveOffset = true;
        rv = true;
      }
      // initialize a new run of invalid timestamps
      invalidTShi = 0;
    }
    lastValidTS = ts;
  }
  return rv;
};
