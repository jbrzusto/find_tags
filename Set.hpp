#ifndef SET_HPP
#define SET_HPP

#include "find_tags_common.hpp"
#include "Tag.hpp"

class Graph;
class Node;

typedef size_t TagPhaseSetHash;

class Set {
  friend class Node;
  friend class Graph;
  friend class hashSet;
  friend class SetEqual;
  friend class Tag_Foray;

protected:
  TagPhaseSet s;
  int _label;
  TagPhaseSetHash hash;

  static int _numSets;
  static int maxLabel;
  static Set * _empty;
  static std::set < Set * > allSets;

public:
  static Set * empty();
  int label() const;

  ~Set();

  static int numSets();
  
  Set();

  Set(TagPhase p);

  Set * augment(TagPhase p);

  Set * reduce(TagPhase p);
  void erase(TagPhase p);

  int count(TagID id) const;

  int count(TagPhase p) const;

  Set * cloneAugment(TagPhase p);
  Set * cloneReduce(TagPhase p);

  static void dumpAll();

  void dump() const;
    
  static void init();

  bool operator==(const Set & s) const;

  bool unique() const;

private:
  static TagPhaseSetHash hashTP (TagPhase tp);

public:
  template<class Archive>
  void serialize(Archive & ar, const unsigned int version) {
    ar & BOOST_SERIALIZATION_NVP( s );
    ar & BOOST_SERIALIZATION_NVP( _label );
    ar & BOOST_SERIALIZATION_NVP( hash );
  };

};

struct hashSet {
  // hashing function used in DFA::setToNode
  size_t operator() (const Set * x) const {
    return  x ? x->hash : 0;
  };
};

struct SetEqual {
  // comparison function used in DFA::setToNode
  bool operator() ( const Set * x1, const Set * x2 ) const {
    if (! x1 && ! x2) 
      return true;
    if (!x1 || ! x2)
      return false;
    return x1->s == x2->s; // fixme slow direct comparison
  };
};

#endif // SET_HPP
