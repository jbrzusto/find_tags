#include "Ticker.hpp"

Ticker::Ticker(){};

Ticker::Ticker(History::marker begin, History::marker end) :
  m(begin),
  end(end)
{};


Timestamp
Ticker::ts() {
  if (m == end)
    return +1.0 / 0.0;
  else 
    return m->ts;
};

Event
Ticker::get() {
  if (m == end)
    throw std::runtime_error("Tried to get event past end of history");
  return *m++;
};
