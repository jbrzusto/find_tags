#include "History.hpp"
#include "Ticker.hpp"
#include <set>

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

void
History::prune_deceased(Timestamp ts) {
  // remove all pairs of activate/deactivate events for a given tag
  // which occurred before ts.  After this, the history will not
  // contain any de-activation events from before ts, and it will
  // contain only those activation events which did not have a
  // subsequent de-activation event before ts.

  Timeline q2; // new timeline, which will include only unpruned events
  // q2 builds in reverse order, but its contents will be copied backward
  // into q to fix this.
  // NB: it would have made more sense to use a std::list for the Timeline,
  // but we don't want to force an invalidation saved findtags state
  // in receiver DBs.

  // work back through event buffer to ts

  int i;
  for(i = size() - 1; i >= 0; --i) {
    if (q[i].ts < ts)
      break;
    q2.push_back(q[i]); // copy this
  }
  if (i < 0)
    return; // all events are at ts or later

  std::set < Motus_Tag_ID > killed;
  while(i >= 0) {
    if (q[i].code == Event::E_DEACTIVATE) {
      // mark this tag as one for which we'll ignore
      // activations, and don't keep this event
      killed.insert(q[i].tag->motusID);
    } else {
      auto id = killed.find(q[i].tag->motusID);
      if (id != killed.end()) {
        // this is an activation event for a tag that dies before ts,
        // so don't keep this event.

        // We could also remove this tag from `killed`, to optimize
        // the average performance of find() above, but this would
        // fail to cancel double activations.  Those shouldn't occur,
        // but it's more robust to not depend on that.

        // killed.remove(id);

      } else {
        // this is an activation event which doesn't have a corresponding
        // deactivation event before ts, so keep it
        q2.push_back(q[i]); // q2 builds in reverse order; fixed below
      }
    }
    --i;
  }

  // q2 now contains the events we want, but in reverse order.
  // feed them back into q

#ifdef DEBUG
  std::cerr << "Dropping " << q.size() - q2.size() << " irrelevant events before " << ts << std::endl;
#endif
  q.clear();

  for(auto p = q2.rbegin(); p != q2.rend(); ++p)
    q.push_back(*p);
};
