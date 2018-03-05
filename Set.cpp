#include "Set.hpp"
Set *
Set::empty() {
  return _empty;
};

int
Set::label() const {
  return _label;
};

Set::~Set() {
#ifdef DEBUG2
  std::cerr << "Set::~Set() " << (void* ) this << std::endl;
  allSets.erase(this);
#endif
  --_numSets;
};

int
Set::numSets() {
  return _numSets;
};

Set::Set() : s(), _label(maxLabel++) , hash(0) {
#ifdef DEBUG2
  std::cerr << "Set::Set() " << (void* ) this << std::endl;
  allSets.insert(this);
#endif
  ++_numSets;
};

Set::Set(TagPhase p) : s(), _label(maxLabel++), hash(hashT(p.first)) {
    s.insert(p);
#ifdef DEBUG2
    std::cerr << "Set::Set(TagPhase) " << (void* ) this << std::endl;
    allSets.insert(this);
#endif
    ++_numSets;
};

Set *
Set::augment(TagPhase p) {
  // augment this set with p, unless this set is empty
  // in which case return a new set with just p;
  if(this == _empty)
    return new Set(p);
  for(auto e = s.equal_range(p.first); e.first != e.second; ++e.first) {
    if (e.first->second == p.second)
      throw std::runtime_error("Adding existing tagphase to tagphaseset");
  }
  s.insert(p);
  hash ^= hashT(p.first);
  return this;
};

Set *
Set::reduce(Tag * t) {
  // remove t from set; return pointer to empty set if
  // reduction leads to that
  if(this == _empty)
    throw std::runtime_error("Reducing empty set");
  erase(t);
  hash ^= hashT(t);
  if (s.size() == 0) {
    delete this;
    return _empty;
  }
  return this;
};

void
Set::erase(Tag * t) {
    // erase specific element t from multimap; i.e. match
    // by both key and value
  s.erase(t);
};

int
Set::count(TagID id) const {
  return s.count(id);
};

int
Set::count(TagPhase p) const {
  // count specific element p from map; i.e. match
  // by both key and value; returns 0 or 1
  auto r = s.equal_range(p.first);
  for (auto i = r.first; i != r.second; ++i)
    if (i->second == p.second)
      return 1;
  return 0;
};

Set *
Set::cloneAugment(TagPhase p) {
  // return pointer to clone of this Set, augmented by p
  if (this == _empty)
    return new Set(p);
  for(auto e = s.equal_range(p.first); e.first != e.second; ++e.first) {
    if (e.first->second == p.second)
      throw std::runtime_error("Adding existing tagphase to tagphaseset");
  }
  Set * ns = new Set();
  ns->s = s;
  ns->s.insert(p);
  ns->hash = hash ^ hashT(p.first);
  return ns;
};


Set *
Set::cloneReduce(Tag * t) {
  // return pointer to clone of this Set, reduced by p
  if (count(t) == 0)
    throw std::runtime_error("Set::cloneReduce(t) called with t not in set");
  Set * ns = new Set();
  ns->s = s;
  ns->erase(t);
  if (ns->s.size() == 0) {
    delete ns;
    return empty();
  }
  ns->hash = hash ^ hashT(t);
  return ns;
};

#ifdef DEBUG2
void
Set::dumpAll() {
  for (auto i = allSets.begin(); i != allSets.end(); ++i) {
    std::cout << "Set " << (*i)->_label << " has " << (*i)->s.size() << " elements:\n";
    for (auto j = (*i)->s.begin(); j != (*i)->s.end(); ++j) {
      std::cout << "   TagPhase " << *j << std::endl;
    }
  }
};
#endif

void
Set::dump() const {
  if (s.size() == 0) {
    std::cout << "(Empty)";
  } else {
    for (auto j = s.begin(); j != s.end(); ++j) {
      std::cout << *j << " ";
    }
  }
};

void
Set::init() {
  _empty = new Set();

#ifdef DEBUG2
  allSets = std::set < Set * > ();
  allSets.insert(_empty);
#endif
};

bool
Set::unique() const {
  if (this == _empty || s.size() != 1)
    return false;
  return true;
};

bool
Set::operator== (const Set & s) const {
  return hash == s.hash && this->s == s.s;
};

TagPhaseSetHash
Set::hashT (Tag * t) {
  return reinterpret_cast < long long > (t);
};

Set * Set::_empty = 0;
#ifdef DEBUG2
std::set < Set * > Set::allSets = std::set < Set * > ();
#endif
int Set::_numSets = 0;
int Set::maxLabel = 0;
