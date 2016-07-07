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
      //      char     error[256];  // omit: too large when buffering
    };

    struct {
      // Clock setting record
      int      clock_level;
      double   clock_remaining;
    };

  } v; 
   
  static SG_Record from_buf(char * buf);        //!< construct from buffer


  template<class Archive>
  void serialize(Archive & ar, const unsigned int version)
  {
    ar & BOOST_SERIALIZATION_NVP( type );
    ar & BOOST_SERIALIZATION_NVP( ts );
    ar & BOOST_SERIALIZATION_NVP( port );
    switch(type) {
    case PULSE:
      ar & BOOST_SERIALIZATION_NVP( v.dfreq );
      ar & BOOST_SERIALIZATION_NVP( v.sig );
      ar & BOOST_SERIALIZATION_NVP( v.noise );
      break;
      
    case GPS:
      ar & BOOST_SERIALIZATION_NVP( v.lat );
      ar & BOOST_SERIALIZATION_NVP( v.lon );
      ar & BOOST_SERIALIZATION_NVP( v.alt );
      break;

    case PARAM:
      ar & BOOST_SERIALIZATION_NVP( v.param_flag );
      ar & BOOST_SERIALIZATION_NVP( v.param_value );
      ar & BOOST_SERIALIZATION_NVP( v.return_code );
      break;

    default:
      break;
    } 
  };  
};

#endif // SG_RECORD
