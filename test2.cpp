// FIXME: use of unphased tagphase sentinel element in sets
// doesn't work: sets proliferate on subsequent adds.
// Need fastish way to determine whether a given tag ID is
// present in a set.

#include <iostream>
#include <boost/icl/interval_set.hpp>
#include <boost/icl/separate_interval_set.hpp>
#include <boost/icl/split_interval_set.hpp>
#include <boost/icl/split_interval_map.hpp>
#include <stdexcept>
#include <fstream>

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
  if (value.phase >= 0)
    return stream << "# " << value.tagID << " (" << value.phase << ") ";
  return stream;
}

typedef interval_map < double, TagPhaseSet > Edges;

class DFA {
public:
  typedef std::map < TagPhaseSet, DFA> SetToNode;
  static SetToNode setToNode; // will own all DFA nodes
  
  TagPhaseSet s;
  Edges e;
  string label;

private:
  DFA() : e(), s() {
    label = string("a") + std::to_string(++ncount);
  };

  // DFA(const DFA & x) : e(x.e), s(x.s) {
  //   label = string("a") + std::to_string(++ncount);
  // };

public:
  DFA copy() {
    DFA rv = *this;
    rv.label = string("a") + std::to_string(++ncount);
    return rv;
  }

      

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

  static int ncount;
  static DFA & make_root() {
    // make a root DFA node
    DFA d;
    return (setToNode.insert(make_pair(d.s, d)).first) -> second;
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

  void add(TagPhase tag, double t, double dt) {
    // add an edge from this node to one with tag for the interval [t-dt, t+dt)
    TagPhaseSet tmp;
    tmp.insert(tag);
    tmp.insert(onlyID(tag));
    e.add(make_pair( interval < double > :: closed(t-dt, t+dt), tmp));
    // iterate over segments, looking for those having the
    // just-added TagPhase; for each of these, build a new node.
    for(auto i = e.begin(); i != e.end(); ++i) {
      auto ss = i->second; // set of nodes
      if (ss.count(tag)) {
        if (ss.size() == 2) {
          // this TagPhase (and its unphased sentinel) just added to set
          DFA d;
          d.s.insert(tag);
          d.s.insert(onlyID(tag));
          setToNode.insert(make_pair(d.s, d));
        } else {
          // we want to copy the node corresponding to the original
          // set, i.e. before this TagPhase was added
          auto sm = ss;
          sm.erase(tag);
          sm.erase(onlyID(tag));
          // if the original set still exists in the interval map,
          // then we must copy its node; otherwise, the newly-added
          // tagphase subsumed the original set, and we can augment
          // its node.  If the original set is still in the interval
          // map, it will be the set for either the preceding or the
          // following interval.
          auto nd = setToNode.find(sm);
          if (nd == setToNode.end()) {
            return;
            cout << "reduced set is " << sm << endl;
            cout << "full set is " << ss << endl;
            throw runtime_error("couldn't find node for reduced set");
          }
          bool keep_old = false;
          auto j = i;
          if (i != e.begin() && (--j)->second == sm) {
            keep_old = true;
          } else {
            j = i;
            ++j;
            if (j != e.end() && j->second == sm) {
              keep_old = true;
            }
          }
          DFA d = nd->second.copy();
          if (d.s.count(onlyID(tag)))
            throw std::runtime_error("ID already in set");
          DFA & n = (setToNode.insert(make_pair(ss, d)).first) -> second;
          n.s = ss;
          if (! keep_old) {
            cout << "Working on node with set " << this->s << endl << "Erasing node for " << sm << "because overridden by addition of tag " << tag << endl;
            setToNode.erase(sm);
          }
        }
      }
    }
  };

  void add_rec(TagPhase tag, TagPhase newtag, double t , double dt) {
    // recursively search for nodes containing tag, and 
    // add add an edge to tag' where tag'.tagID = tag.tagID
    // and tag.phase = newphase, with interval [t-dt, t+dt]
    // Tag to a DFA.
    
    // iterate over edges, adding to each child that has some phase
    // of the given tag.
    auto id = onlyID(tag);
    for(auto i = e.begin(); i != e.end(); ++i) {
      if (i->second.count(id)) {
        //        auto n = setToNode.find(i->second);
        setToNode.find(i->second)->second.add_rec(tag, newtag, t, dt);
      }
    }
    // possibly add edge from this node
    if (s.count(tag))
      add(newtag, t, dt);
  };

  void del_rec(TagPhase tp, double t, double dt) {
    // delete the entire subgraph for the tag with ID tp.tagID
    // tp.phase should be -1.
    // we do this child-first, depth-first recursively.
    // When processing this node, we first correct the setToNode
    // mapping to reflect removal of tp, then correct the interval_map.
    if (interval_count(e) > 0) {
      auto id = onlyID(tp);
      for(auto i = e.begin(); i != e.end(); ++i) {
        if (i->second.count(id)) {
          auto n = setToNode.find(i->second);
          if (n != setToNode.end()) {
            n->second.del_rec(tp, t, dt);
            if (n->second.s.count(tp)) {
              // correct the setToNode mapping to reflect that
              // it will be the reduced set now mapping to the 
              // node.  If there is already a node for the reduced
              // set, then just delete the node for the current set.
              auto sm = i->second;
              sm.erase(tp);
              sm.erase(onlyID(tp));
              auto rn = setToNode.find(sm);
              if (rn != setToNode.end()) {
                setToNode.erase(n);
              } else {
                DFA cp = n->second;
                setToNode.erase(n);
                setToNode.insert(make_pair(sm, cp));
              }
            }
          }
        }
      }
      auto period = interval < double > :: closed(t - dt, t + dt);
      TagPhaseSet s1;
      s1.insert(tp);
      s1.insert(onlyID(tp));
      e.subtract(make_pair( period, s1));
    }
    
  };

  static void del (TagPhaseSet t) {
    // delete the node associated with the given TagPhaseSet
    auto i = setToNode.find(t);
    if (i != setToNode.end())
      setToNode.erase(i);
  };
  
  int nnum;
  static std::map < TagPhaseSet, string > labForSet;

  void viz(std::ostream &out) {
  // visualize
    out << "digraph TEST {\n";
    nnum = 1;
    // generate node symbols and labels
    for (auto i = setToNode.begin(); i != setToNode.end(); ++i) {
      out << i->second.label << "[label=\"" << i->second.label << "=" << i->first<< "\"];\n";
      for(auto j = i->second.e.begin(); j != i->second.e.end(); ++j) {
        auto n = setToNode.find(j->second);
        if (n != setToNode.end()) {
          out << i->second.label << " -> " << (n->second.label) << "[label = \""
              << j->first << "\"];\n";
        }
      }
    }
    out << "}\n";
  };
};

int DFA::ncount = 0;

DFA::SetToNode DFA::setToNode = DFA::SetToNode();
std::map < TagPhaseSet, string > DFA::labForSet = std::map < TagPhaseSet, string > ();
template<class CharType, class CharTraits>
std::basic_ostream<CharType, CharTraits> &operator<<
(std::basic_ostream<CharType, CharTraits> &stream, DFA const& value)
{
  stream << "Node with label " << value.label << "(" << (& value) << ") and set (" << & (value.s) << "):\n" << value.s << "\nand edges (" << & (value.e) << "):\n" << value.e;
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
  TagPhase tC0(3, 0);
  TagPhase tC1(3, 1);
  TagPhase tC2(3, 2);
  TagPhase tC3(3, 3);

  DFA & G = DFA::make_root();
#ifdef DEBUG2
  cout << "Before add G(" << &G << ") has setToNode:\n" << G.setToNode << endl;
#endif


  G.print(G.e);
  G.print(G.s);

  // Using del = 0.5:
  // add tag A with gaps 3,    5,    7
  // add tag B with gaps 2.75, 4.75, 8.1
  // add tag C with gaps 3.3,  5.1,  7.8

  int n = 0;

  double d = 0.5;
  G.add(tA1, 3.0, d);
  {
    std::ofstream of(string("./test") + std::to_string(++n) + ".gv");
    G.viz(of);
  }
  G.add_rec(tA1, tA2, 5.0, d);
  {
    std::ofstream of(string("./test") + std::to_string(++n) + ".gv");
    G.viz(of);
  }
  G.add_rec(tA2, tA3, 7.0, d);
  {
    std::ofstream of(string("./test") + std::to_string(++n) + ".gv");
    G.viz(of);
  }
  G.add(tB1, 2.75, d);
  {
    std::ofstream of(string("./test") + std::to_string(++n) + ".gv");
    G.viz(of);
  }
  G.add_rec(tB1, tB2, 4.75, d);
  {
    std::ofstream of(string("./test") + std::to_string(++n) + ".gv");
    G.viz(of);
  }
  G.add_rec(tB2, tB3, 8.1, d);
  {
    std::ofstream of(string("./test") + std::to_string(++n) + ".gv");
    G.viz(of);
  }
  G.add(tC1, 3.3, d);
  {
    std::ofstream of(string("./test") + std::to_string(++n) + ".gv");
    G.viz(of);
  }
  G.add_rec(tC1, tC2, 5.1, d);
  {
    std::ofstream of(string("./test") + std::to_string(++n) + ".gv");
    G.viz(of);
  }
  G.add_rec(tC2, tC3, 7.8, d);
  {
    std::ofstream of(string("./test") + std::to_string(++n) + ".gv");
    G.viz(of);
  }
  cout << "Before del\n" << G.setToNode << endl;
  G.del_rec(tA3, 7.0, d);
  {
    std::ofstream of(string("./test") + std::to_string(++n) + ".gv");
    G.viz(of);
  }
  cout << "After del\n" << G.setToNode << endl;
  G.del_rec(tA2, 5.0, d);
  {
    std::ofstream of(string("./test") + std::to_string(++n) + ".gv");
    G.viz(of);
  }
  G.del_rec(tA1, 3.0, d);
  {
    std::ofstream of(string("./test") + std::to_string(++n) + ".gv");
    G.viz(of);
  }
};

int main()
{
    cout << ">>dfa test <<\n";
    cout << "--------------------------------------------------------------\n";
    dfa_test();
    return 0;
}

