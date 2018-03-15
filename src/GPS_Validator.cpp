#include "GPS_Validator.hpp"

GPS_Validator::GPS_Validator(Timestamp thresh) :
  thresh(thresh),
  lastGPSTS(0),
  pulseTSlo(0),
  pulseTShi(0),
  stuck(false)
{
};

bool
GPS_Validator::accept(Timestamp ts, bool isPulse) {

  // We keep track of the pulse clock between GPS fixes:
  //
  //                      |<---- elapsed pulse clock--------->|
  //                      |                                   |
  // ---+-----------------+-------------+---------------...---+--------------+----->
  //    |                 |             |                     |              |
  //    |                 |             |                     |              |
  //    lastGPSTS         pulseTS[1]    pulseTS[2]      ...   pulseTS[n]     GPSts
  //                      pulseTSlo                           pulseTShi
  //
  // If two consecutive GPS timestamps are identical, but separated by a
  // elapsed pulse clock of at least thresh seconds, then the GPS is deemed
  // 'stuck'.  Although the diagram shows pulses timestamps arriving in
  // temporal order, we don't assume this, and instead adjust the lo and hi
  // endpoints of the range with each new pulse timestamp. (Signal from
  // different antennas is processed in chunks which leads to interleaved
  // pulse timestamps.)

  if (isPulse) {
    if (lastGPSTS) {
      // we've seen a GPS timestamp, so we are in an intervening run of pulse timestamps;
      // broaden the pulse interval appropriately
      if (! pulseTShi) {
        pulseTShi = pulseTSlo = ts;
      } else {
        pulseTSlo = std::min(ts, pulseTSlo);
        pulseTShi = std::max(ts, pulseTShi);
      }
    }
  } else {
    // this is a GPS timestamp; if we've already seen one, check for an intervening
    // run of pulse timestamps of suitable width
    if (pulseTShi && pulseTShi - pulseTSlo >= thresh) {
      // We've seen suitably-spaced pulse timestamps since the last GPS timestamp,
      // so can (re)assess whether the GPS is stuck.
      stuck = (ts == lastGPSTS);
    }
    // reset the pulse timestamp run
    pulseTShi = 0;
    lastGPSTS = ts;
  }
  return stuck;
};
