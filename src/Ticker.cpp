#include "Ticker.hpp"

Ticker::Ticker(){};

Ticker::Ticker(History * h, History::marker begin) :
  h(h),
  m(begin)
{};


Timestamp
Ticker::ts() {
  if (m >= h->size())
    return +1.0 / 0.0;
  else 
    return h->get(m).ts;
};

Event
Ticker::get() {
  if (m >= h->size())
    throw std::runtime_error("Tried to get event past end of history");
  return h->get(m++);
};
