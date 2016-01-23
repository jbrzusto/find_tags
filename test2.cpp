#include <iostream>
#include <boost/icl/interval_set.hpp>
#include <boost/icl/separate_interval_set.hpp>
#include <boost/icl/split_interval_set.hpp>
#include <boost/icl/split_interval_map.hpp>
#include <stdexcept>

using namespace std;
using namespace boost::icl;

typedef int TagID;

class TagPhase {
public:
  TagPhase(TagID tagID, int phase) :
    tagID(tagID),
    phase(phase)
  {};
  TagID tagID;
  int phase;
};

typedef set < TagPhase > TagPhaseSet;

bool operator < (const TagPhase& x1, const TagPhase& x2) { return x1.tagID < x2.tagID || (x1.tagID == x2.tagID && x1.phase < x2.phase);}
bool operator == (const TagPhase& x1, const TagPhase& x2) { return x1.tagID == x2.tagID && x1.phase == x2.phase; } 
bool operator <= (const TagPhase& x1, const TagPhase& x2) { return x1.tagID <= x2.tagID ||  (x1.tagID == x2.tagID && x1.phase <= x2.phase);}


template<class CharType, class CharTraits>
std::basic_ostream<CharType, CharTraits> &operator<<
  (std::basic_ostream<CharType, CharTraits> &stream, TagPhase const& value)
{
  return stream << "TagPhase: " << value.tagID << "," << value.phase;
}

typedef interval_map < double, TagPhaseSet > Edges;

class DFA {
public:
  typedef std::map < TagPhaseSet, DFA> SetToNode;
  static SetToNode nodeForSet; // will own all DFA nodes
  
  TagPhaseSet s;
  Edges e;


private:
  DFA() : e(), s() {};

public:
  static TagPhase onlyID(const TagPhase &t) {
    // return an "unphased" version of the tag
    // When a TagPhase is added to a TagPhaseSet, we add
    // the unphased version, so that we can quickly tell
    // whether a TagPhase with the same TagID and *any*
    // phase is in the set.  It is an error for a TagPhaseSet 
    // to contain more than one single normal TagPhase:  this
    // would imply error tolerance in gap sizes were too large.

    TagPhase t2 = t;
    t2.phase = -1;
    return t2;
  };

  static DFA & make_DFA() {
    // make a root DFA node
    DFA d;
    return (nodeForSet.insert(make_pair(d.s, d)).first) -> second;
  };

  static DFA & make_DFA(TagPhase t) {
    // make a new DFA node for a single TagPhase
    DFA d;
    d.s.insert(t);
    d.s.insert(onlyID(t));
    return (nodeForSet.insert(make_pair(d.s, d)).first) -> second;
  };

  static DFA & make_DFA(TagPhaseSet s, TagPhase t) {
    // make a new DFA node copying the existing one
    // for s \ {t}, but keeping t in the set.
#ifdef DEBUG
    cout << "make_DFA with set: " << s << "\nand TagPhase " << t << endl;
#endif
    auto sm = s;
    sm.erase(t);
    sm.erase(onlyID(t));
    auto nd = nodeForSet.find(sm);
    if (nd == nodeForSet.end())
      throw runtime_error("couldn't find node for reduced set");
    DFA d = nd->second;
#ifdef DEBUG
    cout << "original has set " << nd->second.s << " and edges " << nd->second.e << endl;
    cout << "copy has set " << d.s << " and edges " << d.e << endl;
#endif
    if (d.s.count(onlyID(t)))
      throw std::runtime_error("ID already in set");
    DFA & rv = (nodeForSet.insert(make_pair(s, d)).first) -> second;
    rv.s=s;
#ifdef DEBUG
    cout << "made node " << &rv << " with set " << rv.s << " and edges " << rv.e << endl;
#endif
    return rv;
  };
    

  static void print(TagPhaseSet s) {
    cout << s;
  };

  static void print(Edges e) {
    cout << e;
  };
  
  void printEdges() {
    cout << "Edges: " << e << endl;
  };

  void add(TagPhase tag) {
    // add tag to the set for this DFA.  Does not add any edges.
    s.insert(tag);
  };

  void add(TagPhase tag, double t1, double t2) {
    TagPhaseSet tmp;
    tmp.insert(tag);
    tmp.insert(onlyID(tag));
#ifdef DEBUG    
    cout << "At add for DFA (" << this << ") before add of " << tag << " with [" << t1 << "," << t2 << "] E is " << this->e << endl;
#endif
    e.add(make_pair( interval < double > :: closed(t1, t2), tmp));
    // iterate over segments, looking for those having the
    // just-added TagPhase; for each of these, build a new node.
#ifdef DEBUG
    cout << "after add, E (" << (&this->e) << ") is " << this->e << " and length is " << this->e.size() << endl;
#endif
    for(auto i = this->e.begin(); i != e.end(); ++i) {
      auto ss = i->second; // set of nodes
      if (ss.count(tag)) {
        if (ss.size() == 2) {
          // this TagPhase (and its unphased sentinel) just added to set
          make_DFA(tag);
        } else {
          // we want to copy the node corresponding to the original
          // set, i.e. before this TagPhase was added
          make_DFA(ss, tag);
        }
      }
    }
  };

  void add_rec(TagPhase tag, TagPhase newtag, double t1, double t2) {
    // recursively search for nodes containing tag, and 
    // add add an edge to tag' where tag'.tagID = tag.tagID
    // and tag.phase = newphase, with interval [t1, t2]
    // Tag to a DFA.
    
    // iterate over edges, adding to each child that has some phase
    // of the given tag.
    auto id = onlyID(tag);
    for(auto i = e.begin(); i != e.end(); ++i) {
      if (i->second.count(id)) {
        auto n = nodeForSet.find(i->second);
        nodeForSet.find(i->second)->second.add_rec(tag, newtag, t1, t2);
      }
    }
    // possibly add edge to current node
    if (s.count(tag))
      add(newtag, t1, t2);
  };

  void del_rec(TagPhase tag) {
  };

  static void del (TagPhaseSet t) {
    // delete the node associated with the given TagPhaseSet
    auto i = nodeForSet.find(t);
    if (i != nodeForSet.end())
      nodeForSet.erase(i);
  };

};

DFA::SetToNode DFA::nodeForSet = DFA::SetToNode();

template<class CharType, class CharTraits>
std::basic_ostream<CharType, CharTraits> &operator<<
(std::basic_ostream<CharType, CharTraits> &stream, DFA const& value)
{
  stream << "Node with address " << (& value) << " and set (" << & (value.s) << "):\n" << value.s << "\nand edges (" << & (value.e) << "):\n" << value.e;
};

template<class CharType, class CharTraits>
std::basic_ostream<CharType, CharTraits> &operator<<
(std::basic_ostream<CharType, CharTraits> &stream, DFA::SetToNode const& value)
{
  int j = 1;
  for (auto i = value.begin(); i != value.end(); ++i) {
    stream << "   Node (" << j++ << ") " << (i->second) << endl;
  }
 return stream;
}


void dfa_test()
{
  
  TagPhase tA0(1, 0);
  TagPhase tA1(1, 1);
  TagPhase tA2(1, 2);
  TagPhase tA3(1, 3);
  TagPhase tB0(2, 0);
  TagPhase tB1(2, 1);
  TagPhase tB2(2, 2);
  TagPhase tB3(2, 3);

  DFA & G = DFA::make_DFA();
#ifdef DEBUG
  cout << "Before add G(" << &G << ") has nodeForSet:\n" << G.nodeForSet << endl;
#endif

  // add tag A with gaps 3, 5, 7, using del 0.5

  double d = 0.5;
  G.add(tA1, 3.0 - d, 3.0 + d);
  cout << "After add\n" << G.nodeForSet << endl;
  G.add_rec(tA1, tA2, 5.0 - d, 5.0 + d);
  cout << "After add\n" << G.nodeForSet << endl;
  G.add_rec(tA2, tA3, 7.0 - d, 7.0 + d);
  cout << "After add\n" << G.nodeForSet << endl;
  G.add(tB1, 2.75 - d, 2.75 + d);
  cout << "After add\n" << G.nodeForSet << endl;
  G.add_rec(tB1, tB2, 4.75 - d, 4.75 + d);
  cout << "After add\n" << G.nodeForSet << endl;
  G.add_rec(tB2, tB3, 8.1 - d, 8.1 + d);
  cout << "After add\n" << G.nodeForSet << endl;
  
  
};

int main()
{
    cout << ">>dfa test <<\n";
    cout << "--------------------------------------------------------------\n";
    dfa_test();
    return 0;
}

