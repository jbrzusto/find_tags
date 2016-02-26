#ifndef HISTORY_HPP
#define HISTORY_HPP

#include "find_tags_common.hpp"
#include "Event.hpp"

class Ticker;

class History {
  friend class Ticker;

public:
  typedef std::set < Event > timeline; //!< ordered sequence of events
  typedef timeline::iterator marker; //!< pointer to a location in history

  History();

  Ticker getTicker(); //!< get iterator to history
  void push(Event e); //!< add an event to the end of the history

protected:
  // represent a time-mapped sequence of events
  timeline q;
};

#endif // HISTORY_HPP
