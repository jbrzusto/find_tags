#ifndef HISTORY_HPP
#define HISTORY_HPP

#include "find_tags_common.hpp"
#include "Event.hpp"

class Ticker;

class History {
  friend class Ticker;

public:
  typedef std::vector < Event > Timeline; //!< ordered sequence of events
  typedef int marker; //!< index in timeline

  History();

  Ticker getTicker(); //!< get iterator to history
  void push(Event e); //!< add an event to the end of the history
  Event get (marker m); //!< get event at index m
  size_t size(); //!< return size of timeline

protected:
  // represent a time-mapped sequence of events
  Timeline q;

public:

  template<class Archive>
  void serialize(Archive & ar, const unsigned int version) {
    ar & BOOST_SERIALIZATION_NVP( q );
  };

};

#endif // HISTORY_HPP
