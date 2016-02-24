#include "find_tags_common.hpp"
#include "Tag.hpp"
#include "Node.hpp"
#include "Set.hpp"
#include <cmath>

void
Node::link() {
  ++ useCount;
  ++ _numLinks;
};

bool
Node::unlink() {
  -- _numLinks;
  return -- useCount == 0;
};

void
Node::drop() {
  if (s != Set::empty())
    delete s;
  -- _numNodes;
  delete this;
};

Node * 
Node::advance (Gap dt) {
  // return the Node obtained by following the edge labelled "gap",
  // or NULL if no such edge exists.  i.e. move to the state representing
  // those tag IDs which are compatible with the current set of pulses and with
  // the specified gap to the next pulse.

  // get the point at or left of the given gap

  auto i = e.upper_bound(dt);
  --i;
  if (i->second != _empty)
    return i->second;
  return 0;
};

      
void
Node::ctorCommon() {
  useCount = 0;
  stamp = 0;
  label = maxLabel++;
  ++ _numNodes;
  if (_empty) {
    e.insert(std::make_pair(-1.0 / 0.0, _empty));
    e.insert(std::make_pair( 1.0 / 0.0, _empty));
  }
};

void
Node::init() {
  Set::init();
  _empty = new Node();
  ++ _numNodes;
  };

Node *
Node::empty() {
  return _empty;
};

Node::Node() : s(Set::empty()), e() {
  ctorCommon();
};

Node::Node(const Node *n) : s(n->s), e(n->e), useCount(0) {
  ctorCommon();
  for (auto i = e.begin(); i != e.end(); ++i)
    i->second->link();
};

int
Node::numNodes() {
  return _numNodes;
};

int
Node::numLinks() {
  return _numLinks;
};


bool
Node::Node::is_unique() {

  // does this DFA state represent a single Tag ID?

  return s->unique();
};


Gap 
Node::get_max_age() {
  auto i = e.rbegin();
  --i;
  if (std::isfinite(i->first))
    return i->first;
  return 0;
};

Tag * 
Node::get_tag() {
  if (s == Set::empty())
    return BOGUS_TAG;
  return s->s.begin()->first;
};

Phase
Node::get_phase() {
  if (s == Set::empty())
    return BOGUS_PHASE;
  return s->s.begin()->second;
};
  
void
Node::dump(bool skipEdges) {
  std::cout << "Node: " << label << " has " << e.size() << " entries in edge map:\n";
  if (! skipEdges) {
    for (auto i = e.begin(); i != e.end(); ++i) {
      std::cout << "   " << i->first << " -> Node (" << i->second->label << ", uc=" << i->second->useCount << ") for Set ";
      i->second->s->dump();
      std::cout << std::endl;
    }
  }
};

int Node::_numNodes = 0;
int Node::_numLinks = 0;
int Node::maxLabel = 0;
Node * Node::_empty = 0;
