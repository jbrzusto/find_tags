#include "History.hpp"

History::History() : q() {};

void
History:: push(Timestamp t, Event e) {
  q[t] = e;
};

Event
History::pop() {
  auto i = q.begin();
  Event e = i->second;
  q.erase(i);
  return e;
};

Timestamp
History::ts() {
  auto i = q.begin();
  if (i == q.end())
    return + 1.0 / 0.0;
  return i->first;
};
