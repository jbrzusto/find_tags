#include "History.hpp"
#include "Ticker.hpp"

History::History() : q() {};

void
History::push(Event e) {
  q.push_back(e);
};

Ticker
History::getTicker() {
  return Ticker(this, 0);
};

Event
History::get (marker m) {
  return q[m];
};

size_t
History::size() {
  return q.size();
};
  


