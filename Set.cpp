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
#ifdef DEBUG
  allSets.erase(this);
#endif
  --_numSets;
};

int 
Set::numSets() {
  return _numSets;
};
  
Set::Set() : s(), _label(maxLabel++) , hash(0) {
#ifdef DEBUG
  allSets.insert(this);
#endif
  ++_numSets;
};

Set::Set(TagPhase p) : s(), _label(maxLabel++), hash(hashTP(p)) {
    s.insert(p);
#ifdef DEBUG
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
  s.insert(p);
  hash ^= hashTP(p);
  return this;
};

Set * 
Set::reduce(TagPhase p) { 
  // remove p from set; return pointer to empty set if 
  // reduction leads to that
  if(this == _empty) 
    throw std::runtime_error("Reducing empty set"); 
  erase(p);
  hash ^= hashTP(p);
  if (s.size() == 0) {
    delete this;
    return _empty;
  }
  return this;
};

void
Set::erase(TagPhase p) {
    // erase specific element p from multimap; i.e. match
    // by both key and value
  auto r = s.equal_range(p.first);
  for (auto i = r.first; i != r.second; ++i) {
    if (i->second == p.second) {
      s.erase(i);
      return;
    }
  }
};

int 
Set::count(TagID id) const {
  return s.count(id);
};

int
Set::count(TagPhase p) const {
  // count specific element p from multimap; i.e. match
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
  Set * ns = new Set(); 
  ns->s = s; 
  ns->s.insert(p);
  ns->hash = hash ^ hashTP(p);
  return ns;
};
      

Set *
Set::cloneReduce(TagPhase p) {
  // return pointer to clone of this Set, reduced by p
  if (count(p) == 0)
    throw std::runtime_error("Set::cloneReduce(p) called with p not in set");
  Set * ns = new Set(); 
  ns->s = s; 
  ns->erase(p);
  if (ns->s.size() == 0) {
    delete ns;
    return empty();
  }
  ns->hash = hash ^ hashTP(p);
  return ns;
};

#ifdef DEBUG
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
  ++_numSets;

#ifdef DEBUG
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
Set::hashTP (TagPhase tp) {
  return reinterpret_cast < long long > (tp.first) * tp.second;
};

Set * Set::_empty = 0;
#ifdef DEBUG
std::set < Set * > Set::allSets = std::set < Set * > ();
#endif
int Set::_numSets = 0;
int Set::maxLabel = 0;
