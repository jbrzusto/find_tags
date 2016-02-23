// FIXME: use (tlo, thi) instead of (t-dt, t+dt) to allow for
// multiplicative, rather than additive tolerance.

#include <iostream>
#include <boost/icl/interval_set.hpp>
#include <boost/icl/separate_interval_set.hpp>
#include <boost/icl/split_interval_set.hpp>
#include <boost/icl/split_interval_map.hpp>
#include <stdexcept>
#include <fstream>
#include <vector>

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
  TagPhase onlyID() {
    TagPhase rv = * this;
    rv.phase = -1;
    return(rv);
  }
};

typedef set < TagPhase > TagPhaseSet;

bool operator < (const TagPhase& x1, const TagPhase& x2) { 
  return x1.tagID < x2.tagID || (x1.tagID == x2.tagID && x1.phase < x2.phase);
}

bool operator == (const TagPhase& x1, const TagPhase& x2) { 
  return x1.tagID == x2.tagID && x1.phase == x2.phase; 
}

bool operator <= (const TagPhase& x1, const TagPhase& x2) { 
  return x1.tagID <= x2.tagID ||  (x1.tagID == x2.tagID && x1.phase <= x2.phase);
}

template<class CharType, class CharTraits>
std::basic_ostream<CharType, CharTraits> &operator<<
(std::basic_ostream<CharType, CharTraits> &stream, TagPhase const& value)
{
  if (value.phase >= 0)
    return stream << "# " << value.tagID << " (" << value.phase << ") ";
  return stream;
}

typedef interval_map < double, TagPhaseSet > Edges;

class DFA_Graph;

class DFA_Node {
public:
  TagPhaseSet s;
  Edges e;
  string label;
private:
  DFA_Node() {};
  friend class DFA_Graph;
};

class DFA_Graph {
protected:
  int ncount;
  int graphSize;
  DFA_Node * root;
  typedef std::map < TagPhaseSet, DFA_Node> SetToNode;
  SetToNode setToNode; // will own all DFA_Node nodes in this graph

public:
  DFA_Node new_node() {
    DFA_Node n;
    n.label = string("a") + std::to_string(++ncount);
    return n;
  };

  DFA_Graph() : ncount(0)
  {
    root = & setToNode.insert(make_pair(TagPhaseSet(), new_node())).first->second;
  };

  DFA_Node copy(DFA_Node &n) {
    DFA_Node rv = n;
    rv.label = string("a") + std::to_string(++ncount);
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
    TagPhaseSet tmp;
    tmp.insert(tag);
    tmp.insert(tag.onlyID());
    n.e.add(make_pair( interval < double > :: closed(tlo, thi), tmp));
    // iterate over segments, looking for those having the
    // just-added TagPhase; for each of these, build a new node.
    for(auto i = n.e.begin(); i != n.e.end(); ++i) {
      auto ss = i->second; // set of nodes
      if (ss.count(tag)) {
        if (ss.size() == 2) {
          // this TagPhase (and its unphased sentinel) just added to set
          DFA_Node d = new_node();
          d.s.insert(tag);
          d.s.insert(tag.onlyID());
          setToNode.insert(make_pair(d.s, d));
        } else {
          // we want to copy the node corresponding to the original
          // set, i.e. before this TagPhase was added
          auto sm = ss;
          sm.erase(tag);
          sm.erase(tag.onlyID());
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
          if (d.s.count(tag.onlyID()))
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
    // add add an edge to tag' where tag'.tagID = tag.tagID
    // and tag.phase = newphase, with interval [t-dt, t+dt]
    // Tag to a DFA_Node.
    
    // iterate over edges, adding to each child that has some phase
    // of the given tag.
    auto id = tag.onlyID();
    if (interval_count(n.e) > 0) {
      for(auto i = n.e.begin(); i != n.e.end(); ++i) {
        if (i->second.count(id)) {
          auto nn = setToNode.find(i->second);
          if (nn != setToNode.end())
            add_rec(nn->second, tag, newtag, tlo, thi);
        }
      }
      // possibly add edge from this node
    }
    if (n.s.count(tag))
      add(n, newtag, tlo, thi);
  };


  void validate() {
    graphSize = 0;
    validate(* root);
    if (graphSize != setToNode.size())
      throw std::runtime_error("Node count != size of setToNode.");
  };

  void validate(DFA_Node & n) {
    // recursively verify that each edge leads to a node
    
    // iterate over edges, adding to each child that has some phase
    // of the given tag.
    ++graphSize;
    if (interval_count(n.e) > 0) {
      for(auto i = n.e.begin(); i != n.e.end(); ++i) {
        auto nn = setToNode.find(i->second);
        if (nn == setToNode.end()) {
          cout << "No node for set:\n" << i->second;
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
    // delete the entire subgraph for the tag with ID tp.tagID
    // tp.phase should be -1.
    // we do this child-first, depth-first recursively.
    // When processing this node, we first correct the setToNode
    // mapping to reflect removal of tp, then correct the interval_map.
    if (interval_count(n.e) > 0) {
      auto id = tp.onlyID();
      bool usedHere = false;
      for(auto i = n.e.begin(); i != n.e.end(); ++i) {
        if (i->second.count(id)) {
          auto nn = setToNode.find(i->second);
          if (nn != setToNode.end()) {
            del_rec(nn->second, tp, tlo, thi);
            if (nn->second.s.count(tp)) {
              usedHere = true;
              // correct the setToNode mapping to reflect that
              // it will be the reduced set now mapping to the 
              // node.  If there is already a node for the reduced
              // set, then just delete the node for the current set.
              auto sm = i->second;
              sm.erase(tp);
              sm.erase(tp.onlyID());
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
        TagPhaseSet s1;
        s1.insert(tp);
        s1.insert(id);
        n.e.subtract(make_pair( period, s1));
      }
    }
    
  };
  
  int nnum;

  void viz(std::ostream &out) {
    // visualize
    out << "digraph TEST {\n";
    nnum = 1;
    // generate node symbols and labels
    for (auto i = setToNode.begin(); i != setToNode.end(); ++i) {
      out << i->second.label << "[label=\"" << i->second.label << "=" << i->second.s<< "\"];\n";
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

  void dumpNodes() {
    for (auto i = setToNode.begin(); i != setToNode.end(); ++i) {
      cout << "   Node (" << i->second.label << ") and set ";
      for (auto j = i->first.begin(); j != i->first.end(); ++j) {
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
      t += gaps[p % n];
      if (t > maxTime)
        break;
      TagPhase next(id, p+1);
      double g = gaps[p % n];
      add_rec(last, next, g * (1 - tol), g * (1 + tol));
      ++p;
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

    int N = 20;
 
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

