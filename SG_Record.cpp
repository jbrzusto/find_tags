#include "SG_Record.hpp"

#include <string.h>
#include <sstream>

SG_Record
SG_Record::from_buf(char * buf) {
  // assume invalid record
  SG_Record rv;
  rv.type = BAD;
  switch (buf[0]) {
  case 'p':
    /* a pulse record line like:
       p1,14332651182.1235,3.234,-55.44,-77.33
       port    ts          dfreq  sig    noise
    */
    if (5 == sscanf(buf+1, "%hd,%lf,%f,%f,%f", &rv.port, &rv.ts, &rv.v.dfreq, &rv.v.sig, &rv.v.noise)) {
      rv.type = PULSE;
    }
    break;

  case 'G':
    /* a GPS fix line like:
       G,1458001712,44.34021,-66.118733333,21.6
       ts        lat        lon        alt
    */
    if (4 == sscanf(buf+2, "%lf,%lf,%lf,%lf", &rv.ts, &rv.v.lat, &rv.v.lon, &rv.v.alt)) {
      rv.type = GPS;
    }
    break;

  case 'S':
    /* a parameter-setting line like:
       S,1366227448.192,5,-m,166.376,0,
       is S, timestamp, port_num, param flag, value, return code, other error
    */
    if (5 == sscanf(buf+2, "%lf,%hd,%[^,],%lf,%d,", &rv.ts, &rv.port, (char *) &rv.v.param_flag, &rv.v.param_value, &rv.v.return_code)) {
      rv.type = PARAM;
    }
    break;

  case 'C':
    /* a clock-setting line like:
       C,1466715518.311,6,0.00000196
       which gives the new timestamp, level of correction (roughly 10^-X), residual correction
       Mainly of interest because it provides a timestamp.
    */
    if (3 == sscanf(buf+2, "%lf,%d,%lf", &rv.ts, &rv.v.clock_level, &rv.v.clock_remaining)) {
      rv.type = CLOCK;
    }
    break;

  default:
    break;
  };
  return rv;
};
