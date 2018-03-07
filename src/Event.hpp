#ifndef EVENT_HPP
#define EVENT_HPP

#include "find_tags_common.hpp"
#include "Tag.hpp"

struct Event {

  // represents an event for a tag; 1 = activation; 0 = deactivation

  static const short E_ACTIVATE = 1;
  static const short E_DEACTIVATE = 0; 
  Timestamp ts;   // time of event, in seconds since unix epoch
  Tag	  * tag;  // pointer to known tag motus tag ID
  short     code; // code for event; one of this class's E_* constants

  Event(){};
  Event(Timestamp ts, Tag * tag, short code) : ts(ts), tag(tag), code(code) {};

  bool operator< (Event const& right) const
  {
    return ts < right.ts || (ts == right.ts && tag->motusID < right.tag->motusID);
  };

  template<class Archive>
  void serialize(Archive & ar, const unsigned int version) {
    ar & BOOST_SERIALIZATION_NVP( ts );
    ar & BOOST_SERIALIZATION_NVP( tag );
    ar & BOOST_SERIALIZATION_NVP( code );
  };
};


#endif // EVENT_HPP
