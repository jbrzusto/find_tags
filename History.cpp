#include "History.hpp"
#include "Ticker.hpp"

History::History() : q() {};

void
History::push(Event e) {
  q.insert(q.end(), e); // hinted insert since built in chrono order
};

Ticker
History::getTicker() {
  return Ticker(q.begin(), q.end());
};
