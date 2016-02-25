#ifndef TICKER_HPP
#define TICKER_HPP

#include "find_tags_common.hpp"
#include "History.hpp"

//<! iterates through a history

class Ticker {
  
protected:
  History::marker m;
  History::marker end;

public:
  Ticker();
  Ticker(History::marker begin, History::marker end);
  Timestamp ts(); //!< return the timestamp for the next event; if no events left, return +Inf
  Event get(); //!< return the event at marker, and increment marker; throws if no events left
};

#endif // TICKER_HPP
