#ifndef TICKER_HPP
#define TICKER_HPP

#include "find_tags_common.hpp"
#include "History.hpp"

//<! iterates through a history

class Ticker {
  
protected:
  History * h;
  History::marker m;

public:
  Ticker();
  Ticker(History * h, History::marker begin);
  Timestamp ts(); //!< return the timestamp for the next event; if no events left, return +Inf
  Event get(); //!< return the event at marker, and increment marker; throws if no events left

  template<class Archive>
  void serialize(Archive & ar, const unsigned int version)
  {
    ar & h;
    ar & m;
  };

};

#endif // TICKER_HPP
