#include "SG_Record.hpp"

#include <string.h>
#include <sstream>

SG_Record::SG_Record() :
  v()
{
};

void
SG_Record::from_buf(char * buf) {
  // assume invalid record
  type = BAD;
  switch (buf[0]) {
  case 'p':
    /* a pulse record line like:
       p1,14332651182.1235,3.234,-55.44,-77.33
       port    ts          dfreq  sig    noise
    */
    if (5 == sscanf(buf+1, "%hd,%lf,%f,%f,%f", &port, &ts, &v.dfreq, &v.sig, &v.noise)) {
      type = PULSE;
    }
    break;

  case 'G':
    /* a GPS fix line like:
       G,1458001712,44.34021,-66.118733333,21.6
       ts        lat        lon        alt
    */
    if (buf[1] != '\0' && 4 == sscanf(buf+2, "%lf,%lf,%lf,%lf", &ts, &v.lat, &v.lon, &v.alt)) {
      type = GPS;
    }
    break;

  case 'S':
    /* a parameter-setting line like:
       S,1366227448.192,5,-m,166.376,0,
       is S, timestamp, port_num, param flag, value, return code, other error
    */
    if (buf[1] != '\0' && 5 == sscanf(buf+2, "%lf,%hd,-w,%[^,],%lf,%d,", &ts, &port, (char *) &v.param_flag, &v.param_value, &v.return_code)) {
      /* this case required by stupid formatting of "-w REG VAL"
         parameters to fcd, which ends up in files as
         e.g. "S,12345678,1,-w,0x07,1,0," */
        type = PARAM;
    } else if (buf[1] != '\0' && 5 == sscanf(buf+2, "%lf,%hd,%[^,],%lf,%d,", &ts, &port, (char *) &v.param_flag, &v.param_value, &v.return_code)) {
      type = PARAM;
    } else if (buf[1] != '\0' && (4 == sscanf(buf+2, "%lf,%hd,%[^,],undefined,%d,", &ts, &port, (char *) &v.param_flag, &v.return_code)
                || 4 == sscanf(buf+2, "%lf,%hd,%[^,],null,%d,", &ts, &port, (char *) &v.param_flag, &v.return_code))) {
      /* this case required to report errors in parameter setting that leave value as 'undefined' or 'null' */
      v.param_value = nan("0");
      type = PARAM;
    }
    v.error[0] = '\0';
    break;

  case 'C':
    /* a clock-setting line like:
       C,1466715518.311,6,0.00000196
       which gives the new timestamp, level of correction (roughly 10^-X), residual correction
       Mainly of interest because it provides a timestamp.
    */
    if (buf[1] != '\0' && 3 == sscanf(buf+2, "%lf,%d,%lf", &ts, &v.clock_level, &v.clock_remaining)) {
      type = CLOCK;
    }
    break;
  case 'F':
    /* a synthetic file timestamp line like:
       F,1466715518.311
       which is used to convey the timestamp encoded in an SG filename; this can
       be used to repair CLOCK_MONOTONIC timestamps where there's no other source
       of CLOCK_REALTIME timestamps (e.g. on an SG without a GPS and using NTP for
       clock sync)
    */
    if (buf[1] != '\0' && 1 == sscanf(buf+2, "%lf", &ts)) {
      type = SG_Record::FILE;
    }
    break;

  default:
    break;
  };
};
