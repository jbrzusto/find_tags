#ifndef EVENT_HPP
#define EVENT_HPP

#include "find_tags_common.hpp"

struct Event {

  // represents an event for a tag; 1 = activation; 0 = deactivation

  static const short E_ACTIVATE = 1;
  static const short E_DEACTIVATE = 0; 
  Tag	  * tag;  // pointer to known tag motus tag ID
  short     code; // code for event; one of this class's E_* constants

  Event(){};
  Event(Tag * tag, short code) : tag(tag), code(code) {};
};

#endif // EVENT_HPP
