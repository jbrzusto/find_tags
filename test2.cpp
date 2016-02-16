// FIXME: use (tlo, thi) instead of (t-dt, t+dt) to allow for
// multiplicative, rather than additive tolerance.

#include <iostream>
#include <boost/icl/interval_set.hpp>
#include <boost/icl/separate_interval_set.hpp>
#include <boost/icl/split_interval_set.hpp>
#include <boost/icl/split_interval_map.hpp>
#include <stdexcept>
#include <fstream>
#include <unordered_map>

using namespace std;
using namespace boost::icl;

typedef int TagID;
typedef short Phase;

typedef std::pair < TagID, Phase > TagPhase;
typedef size_t TagPhaseMapHash;

struct TagPhaseMap {
  unordered_multimap < TagID, Phase > s; // a map of tagID to phase; each tagID can be present in multiple phases
  TagPhaseMapHash hash; // simple hash for set whose value is maintained as set insert, erase methods are called
  TagPhaseMap() : hash(0) {
    cout << "created empty TagPhaseMap\n";
  };

  void info() const {
    //    std::cout << " TPM " << label << " (" << this << ") ";
    std::cout << " TPM " << " (" << this << ") ";
  };


  TagPhaseMap(const TagPhase & tp) {
    insert (tp);
    cout << "created singleton TagPhaseMap with ";
    cout << tp.first << "," << tp.second;
    cout << "\n";
  };

  bool has (const TagPhase & tp) {
    auto r = s.equal_range(tp.first);
    for( auto i = r.first; i != r.second; ++i)
      if (i->second == tp.second)
        return true;
    return false;
  };

  // Note: we need a symmetric (in permutations of element order) hash
  // function that can be computed incrementally by both insert() and
  // erase().  Does any associative, commutative binary function work?
  // Obvious candidates are '+' and '^'.  We use both tag ID and phase
  // in computing it.

  void insert( const TagPhase & tp) {
    s.insert(tp);
    hash = hash ^ (tp.first * tp.second);
  };

  void erase( const TagPhase & tp ) {
    auto r = s.equal_range(tp.first);
    for( auto i = r.first; i != r.second; ++i) {
      if (i->second == tp.second) {
        s.erase(i);
        hash = hash ^ (tp.first * tp.second);
        return;
      }
    }
  };

  TagPhaseMap ctor() {
    return TagPhaseMap();
  };
  
  bool operator== (const TagPhaseMap & tps) const {
    return hash == tps.hash && s == tps.s;
  };
};


struct HashTagPhaseMap {
  // hashing function used in DFA::setToNode
  size_t operator() (const TagPhaseMap & x) const {
    return x.hash;
  };
};

struct TagPhaseMapEqual {
  // comparison function used in DFA::setToNode
  bool operator() ( const TagPhaseMap & x1, const TagPhaseMap & x2 ) const {
    return x1 == x2;
  };
};

template<class Type> struct TagPhaseMapCombine {
  typedef Type first_argument_type;
  static const Type identity_element () { return TagPhaseMap();};
  Type operator() (Type n1, Type n2) {
    std::cout << "Called TPMCombine with ";
    n1.info();
    std::cout << "and";
    n2.info();
    std::cout << "\n";
    Type n = n1.ctor();
    n.s = n1.s;
    n.s.insert(n2.s.begin(), n2.s.end());
    return n;
  };
};

template<class Type> struct TagPhaseMapUncombine {
  static const Type identity_element () { return TagPhaseMap();};
  Type operator() (Type n1, Type n2) {
    std::cout << "Called TPMUnCombine with ";
    n1.info();
    std::cout << "and";
    n2.info();
    std::cout << "\n";
    Type n = n1.ctor();
    n.s = n1.s;
    n.s.erase(n2.s.begin(), n2.s.end());
    return n;
  };
};
  
template<class Type> 
struct boost::icl::inverse<TagPhaseMapCombine <Type> > { 
  typedef TagPhaseMapUncombine<Type> type; 
};

template<class Type> struct TagPhaseMapIntersect {
  Type operator() (Type n1, Type n2) {
    std::cout << "Called TPMIntersect with ";
    n1.info();
    std::cout << "and";
    n2.info();
    std::cout << "\n";
    auto i1 = n1.s.begin();
    auto e1 = n1.s.end();
    while (i1 != e1) {
      auto i2 = i1;
      ++i2;
      bool found = false;
      auto range = n2.s.equal_range(i1.first);
      for (auto i3=range.first; i3 != range.second; ++i3) {
        if (i3.second == i1.second) {
          found = true;
          break;
        }
      }
      if (! found)
        n1.s.erase(i1);
      i1 = i2;
    }
    return n1;
  };
};
  
bool operator < (const TagPhase& x1, const TagPhase& x2) { 
  return x1.first < x2.first || (x1.first == x2.first && x1.second < x2.second);
}

bool operator == (const TagPhase& x1, const TagPhase& x2) { 
  return x1.first == x2.first && x1.second == x2.second; 
}

bool operator <= (const TagPhase& x1, const TagPhase& x2) { 
  return x1.first <= x2.first ||  (x1.first == x2.first && x1.second <= x2.second);
}

template<class CharType, class CharTraits>
std::basic_ostream<CharType, CharTraits> &operator<<
(std::basic_ostream<CharType, CharTraits> &stream, TagPhase const& value)
{
  if (value.second >= 0)
    return stream << "# " << value.first << " (" << value.second << ") ";
  return stream;
}

template<class CharType, class CharTraits>
std::basic_ostream<CharType, CharTraits> &operator<<
(std::basic_ostream<CharType, CharTraits> &stream, TagPhaseMap const& value)
{
  for (auto i = value.s.begin(); i != value.s.end(); ++i)
    std::cout << TagPhase(i->first, i->second) <<",";
}


typedef interval_map < double, TagPhaseMap, boost::icl::partial_absorber, std::less, TagPhaseMapCombine, TagPhaseMapIntersect  > Edges;

class DFA_Graph;

class DFA_Node {
public:
  TagPhaseMap s;
  Edges e;
  int l;
private:
  DFA_Node() {};
  friend class DFA_Graph;
};

class DFA_Graph {
protected:
  int nodesAlloc;    // number of nodes allocated (and possibly later deleted)
  int nodesUsed; // number of nodes actually in tree.
  DFA_Node * root; // root of DFA_Graph

  typedef std::unordered_map < TagPhaseMap, DFA_Node, HashTagPhaseMap, TagPhaseMapEqual> SetToNode;
  SetToNode setToNode; // will own all DFA_Node nodes in this graph

  DFA_Node new_node() {
    DFA_Node n;
    n.l = ++nodesAlloc;
    return n;
  };

public:

  DFA_Graph() : nodesAlloc(0)
  {
    root = & setToNode.insert(make_pair(TagPhaseMap(), new_node())).first->second;
  };

  DFA_Node copy(DFA_Node &n) {
    DFA_Node rv = n;
    rv.l = ++nodesAlloc;
    return rv;
  }

  void add(TagPhase tag) {
    root->s.insert(tag);
  };

  void add(TagPhase tag, double tlo, double thi) {
    add(* root, tag, tlo, thi);
  };

  void add(DFA_Node &n, TagPhase tag, double tlo, double thi) {
    // add an edge from a node to one with tag for the interval [tlo,
    // thi)
    TagPhaseMap tmp;
    tmp.insert(tag);
    n.e.add(make_pair( interval < double > :: closed(tlo, thi), tmp));
    // iterate over segments, looking for those having the
    // just-added TagPhase; for each of these, build a new node.
    for(auto i = n.e.begin(); i != n.e.end(); ++i) {
      auto ss = i->second; // set of nodes
      if (ss.has(tag)) {
        if (ss.s.size() == 1) {
          // this segment maps only to TagPhase
          DFA_Node d = new_node();
          d.s.insert(tag);
          setToNode.insert(make_pair(d.s, d));
        } else {
          // we want to copy the node corresponding to the original
          // set, i.e. before this TagPhase was added
          auto sm = ss;
          auto rng = ss.s.equal_range(tag.first);
          for (auto is = rng.first; is != rng.second; ++is) {
            if (is->second == tag.second) {
              sm.s.erase(is);
              break;
            }
          }
          // if the original set still exists in the interval map,
          // then we must copy its node; otherwise, the newly-added
          // tagphase subsumed the original set, and we can augment
          // its node.  If the original set is still in the interval
          // map, it will be the set for either the preceding or the
          // following interval.
          auto nd = setToNode.find(sm);
          if (nd == setToNode.end()) {
            // original set no longer in interval map
            return;
          }
          bool keep_old = false;
          auto j = i;
          if (i != n.e.begin() && (--j)->second == sm) {
            keep_old = true;
          } else {
            j = i;
            ++j;
            if (j != n.e.end() && j->second == sm) {
              keep_old = true;
            }
          }
          DFA_Node d = copy(nd->second);
          if (d.s.s.count(tag.first))
            throw std::runtime_error("ID already in set");
          DFA_Node & n = (setToNode.insert(make_pair(ss, d)).first) -> second;
          n.s = ss;
          if (! keep_old) {
            setToNode.erase(sm);
          }
        }
      }
    }
  };

  void add_rec(TagPhase tag, TagPhase newtag, double tlo , double thi) {
    add_rec(* root, tag, newtag, tlo, thi);
  };

  void add_rec(DFA_Node & n, TagPhase tag, TagPhase newtag, double tlo , double thi) {
    // recursively search for nodes containing tag, and 
    // add add an edge to tag' where tag'.first = tag.first
    // and tag.second = newphase, with interval [t-dt, t+dt]
    // Tag to a DFA_Node.
    
    // iterate over edges, adding to each child that has some phase
    // of the given tag.
    if (interval_count(n.e) > 0) {
      for(auto i = n.e.begin(); i != n.e.end(); ++i) {
        if (i->second.s.count(tag.first)) {
          auto nn = setToNode.find(i->second);
          if (nn != setToNode.end())
            add_rec(nn->second, tag, newtag, tlo, thi);
        }
      }
      // possibly add edge from this node
    }
    if (n.s.has(tag))
      add(n, newtag, tlo, thi);
  };


  void validate() {
    nodesUsed = 0;
    validate(* root);
    if (nodesUsed != setToNode.size())
      throw std::runtime_error("Node count != size of setToNode.");
  };

  void validate(DFA_Node & n) {
    // recursively verify that each edge leads to a node
    
    // iterate over edges, adding to each child that has some phase
    // of the given tag.
    ++nodesUsed;
    if (interval_count(n.e) > 0) {
      for(auto i = n.e.begin(); i != n.e.end(); ++i) {
        auto nn = setToNode.find(i->second);
        if (nn == setToNode.end()) {
          cout << "No node for map:\n" << i->second;
          throw std::runtime_error("Edge with no corresponding node.");
        }
        validate(nn->second);
      }
    }
  };


  void del(TagPhase tag) {
    root->s.erase(tag);
  };

  void del_rec(TagPhase tp, double tlo, double thi) {
    del_rec(* root, tp, tlo, thi);
  };

  void del_rec(DFA_Node & n, TagPhase tp, double tlo, double thi) {
    // delete the entire subgraph for the tag with ID tp.first
    // tp.second should be -1.
    // we do this child-first, depth-first recursively.
    // When processing this node, we first correct the setToNode
    // mapping to reflect removal of tp, then correct the interval_map.
    if (interval_count(n.e) > 0) {
      bool usedHere = false;
      for(auto i = n.e.begin(); i != n.e.end(); ++i) {
        if (i->second.s.count(tp.first)) {
          auto nn = setToNode.find(i->second);
          if (nn != setToNode.end()) {
            del_rec(nn->second, tp, tlo, thi);
            if (nn->second.s.has(tp)) {
              usedHere = true;
              // correct the setToNode mapping to reflect that
              // it will be the reduced set now mapping to the 
              // node.  If there is already a node for the reduced
              // set, then just delete the node for the current set.
              auto sm = i->second;
              sm.erase(tp);
              auto rn = setToNode.find(sm);
              if (rn != setToNode.end()) {
                setToNode.erase(nn);
              } else {
                DFA_Node cp = nn->second;
                cp.s = sm;
                setToNode.erase(nn);
                setToNode.insert(make_pair(sm, cp));
              }
            }
          }
        }
      }
      if (usedHere) {
        auto period = interval < double > :: closed(tlo, thi);
        TagPhaseMap s1;
        s1.insert(tp);
        n.e.subtract(make_pair( period, s1));
      }
    }
    
  };
  
  void viz(std::ostream &out) {
    // visualize
    out << "digraph TEST {\n";
    // generate node symbols and labels
    for (auto i = setToNode.begin(); i != setToNode.end(); ++i) {
      out << "a" << i->second.l << "[label=\"a" << i->second.l << "=" << i->second.s<< "\"];\n";
      for(auto j = i->second.e.begin(); j != i->second.e.end(); ++j) {
        auto n = setToNode.find(j->second);
        if (n != setToNode.end()) {
          out << "a" << i->second.l << " -> a" << (n->second.l) << "[label = \""
              << j->first << "\"];\n";
        }
      }
    }
    out << "}\n";
  };

  void dumpNodes() {
    for (auto i = setToNode.begin(); i != setToNode.end(); ++i) {
      cout << "   Node (a" << i->second.l << ") and set ";
      for (auto j = i->first.s.begin(); j != i->first.s.end(); ++j) {
        cout << *j << " ";
      }
    }
  };

  void addTag(TagID id, vector < double > gaps, double tol, double maxTime) {
    // add the repeated sequence of gaps from a tag, with fractional
    // tolerance tol, starting at phase 0, until adding the next gap
    // would exceed a total elapsed time of maxTime.  The gap is set
    // to an interval given by gap * (1-tol), gap * (1 + tol)

    // NB: gap 0 take the tag from phase 0 to phase 1, etc.
    int n = gaps.size();
    int p = 0;
    double t = 0;
    TagPhase last(id, p);
    add(last);
    for(;;) {
      double g = gaps[p % n];
      t += g;
      if (t > maxTime)
        break;
      ++p;
      TagPhase next(id, p);
      add_rec(last, next, g * (1 - tol), g * (1 + tol));
      last = next;
    };
  }    

  void delTag(TagID id, vector < double > gaps, double tol, double maxTime) {
    // remove the tag which was added with given gaps, tol, and maxTime

    int n = gaps.size();
    int p = 0;
    double t = 0;
    for(;;) {
      t += gaps[p % n];
      if (t > maxTime)
        break;
      ++p;
    }      

    while(p > 0) {
      --p;
      double g = gaps[p % n];
      del_rec(TagPhase(id, p+1), g * (1 - tol), g * (1 + tol));
    }

    del(TagPhase(id, 0));
  }

};

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

  DFA_Graph * G = new DFA_Graph();

  // Using del = 0.5:
  // add tag A with gaps 5,    7,    3
  // add tag B with gaps 2.75, 8.1, 4.75
  // add tag C with gaps 5.1,  7.8,  3.1

  int n = 0;
  double d = 0.5;

#define SHOW \
  { \
    std::ofstream of(string("./test") + std::to_string(++n) + ".gv"); \
    G->viz(of); \
  }

  G->add(tA0);
  SHOW;

  G->add_rec(tA0, tA1, 5.0 - d, 5.0 + d);
  SHOW;

  G->add_rec(tA1, tA2, 7.0 - d, 7.0 + d);
  SHOW;

  G->add_rec(tA2, tA3, 3.0 - d, 3.0 + d);
  SHOW;

  G->add(tB0);
  SHOW;

  G->add_rec(tB0, tB1, 2.75 - d, 2.75 + d);
  SHOW;

  G->add_rec(tB1, tB2, 8.1 - d, 8.1 + d);
  SHOW;

  G->add_rec(tB2, tB3, 4.75 - d, 4.75 + d);
  SHOW;

  G->add(tC0);
  SHOW;

  G->add_rec(tC0, tC1, 5.1 - d, 5.1 + d);
  SHOW;

  G->add_rec(tC1, tC2, 7.8 - d, 7.8 + d);
  SHOW;

  G->add_rec(tC2, tC3, 3.1 - d, 3.1 + d);
  SHOW;

  G->del_rec(tA3, 3.0 - d, 3.0 + d);
  SHOW;

  G->del_rec(tA2, 7.0 - d, 7.0 + d);
  SHOW;

  G->del_rec(tA1, 5.0 - d, 5.0 + d);
  SHOW;

  G->del(tA0);
  SHOW;

  // New tag with ID 4 and gaps 6.7, 4.1, 4.3, BI=110

  vector < double > T2;
  T2.push_back(4.1);  
  T2.push_back(4.3);
  T2.push_back(6.7);
  T2.push_back(110);

  double tol = 0.25;
  G->addTag(4, T2, tol, 250);

  SHOW;

  G->delTag(4, T2, tol, 250);

  SHOW;

  G->validate();
};

int main()
{
  cout << ">>dfa test <<\n";
  cout << "--------------------------------------------------------------\n";
  dfa_test();
  return 0;
}

