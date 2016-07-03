#include "SG_Record.hpp"

#include <string.h>
#include <sstream>

SG_Record::SG_Record(char * buf) {
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
    if (4 == sscanf(buf+2, "%lf,%lf,%lf,%lf", &ts, &v.lat, &v.lon, &v.alt)) {
      type = GPS;
    }
    break;

  case 'S':
    /* a parameter-setting line like:
       S,1366227448.192,5,-m,166.376,0,
       is S, timestamp, port_num, param flag, value, return code, other error
    */
    if (5 <= sscanf(buf+2, "%lf,%hd,%[^,],%lf,%d,%[^\n]", &ts, &port, (char *) &v.param_flag, &v.param_value, &v.return_code, (char *) &v.error)) {
      // silently ignore malformed line (note:  %[^\n] field doesn't increase count if remainder of line is empty, so sscanf here can return 5 or 6
      type = PARAM;
    }
    break;

  default:
    break;
  };
};
