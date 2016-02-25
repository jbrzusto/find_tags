#ifndef HISTORY_HPP
#define HISTORY_HPP

#include "find_tags_common.hpp"
#include "Event.hpp"
#include <queue>

class History {
public:
  History();
  void push(Timestamp ts, Event e); //!< add an event to the end of the history
  Event pop(); //!< remove and return the event at the start of the history
  Timestamp ts(); //!< return the timestamp for the next event
  
protected:
  // represent a time-mapped sequence of events

  std::map < Timestamp, Event > q; //!< ordered sequence of events
};

#endif // HISTORY_HPP
