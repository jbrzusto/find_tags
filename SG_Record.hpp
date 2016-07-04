#ifndef SG_RECORD_HPP
#define SG_RECORD_HPP

#include "find_tags_common.hpp"

//!< SG_Record - struct to represent a record from a raw SG file
// handled as a tagged union

struct SG_Record {
  typedef enum {BAD, PULSE, GPS, PARAM, CLOCK, EXTENSION} Type;
  Type type;  //!< type of record represented

  Timestamp ts;  //!< timestamp from file line; common to all record types
  Port_Num port; //!< port from file line; common to most record types
  union {
    struct {
      // Pulse record
      Frequency_Offset_kHz dfreq;
      SignaldB             sig;
      SignaldB             noise;
    };

    struct {
      // GPS record
      double lat;
      double lon;
      double alt;
    };

    struct {
      // Parameter setting record
      char     param_flag[16];
      double   param_value;
      int      return_code;
      char     error[256];
    };

    struct {
      // Clock setting record
      int      clock_level;
      double   clock_remaining;
    };

  } v; 
   
  SG_Record(char * buf);        //!< construct from buffer
};

#endif // SG_RECORD
