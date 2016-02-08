// FIXME: use (tlo, thi) instead of (t-dt, t+dt) to allow for
// multiplicative, rather than additive tolerance.

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
typedef double Gap;

class TagPhase {
public:
  TagID tagID;

  int phase;

  TagPhase(TagID tagID, int phase) :
    tagID(tagID),
    phase(phase)
  {
    cout << "created TagPhase(" << tagID << "," << phase << ")\n";
  };

  TagPhase onlyID() {
    TagPhase rv = * this;
    rv.phase = -1;
    return(rv);
  }
};

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
    return stream << "#" << value.tagID << "." << value.phase;
  return stream;
}

// my own simple interval map class which has two features
// not in the boost version:
// - management of set creation / deletion
// - non-ownership of set
// so e.g. I want an interval_map < double, Node *> where the node owns
// the set of TagPhases, and has an interval_map as its edge structure.


// things we want to have it do:
// - when an <interval,set> pair is added: existing intervals
// are either left as is, split, or subsumed.
// - as is: do nothing
// - split: clone node, adding new subset to set of nodes
// - subsume: add new subset to set of nodes

// when interval is removed:
// - as is: do nothing
// - look for merge candidates on split.
// - subsumed: remove set; if result is empty, remove node and interval.
//
// set management: add set should compute union then look whether
// set node already exists and find node (hash code)
// remove set should compute difference then check for existing set.

typedef double Point;
typedef double Width;

typedef std::pair< Point, Width > Interval;

class IntervalMap {
  std::map < Interval, Node *> e; // edges
  std::set < TagPhase > s; // set

  void add (Point lo, Point hi, TagPhase t);
  void remove (Point lo, Point hi, TagPhase t);
  Node * advance (Point dt);
};

bool overlaps (Interval i1, Interval i2) {
  return ! (i1.first + i1.second <= i2.first || i2.first + i2.second <= i1.first);
};

struct TagPhaseSet {
  std::set < TagPhase > s;
  int hash;

  TagPhaseSet() : s(), hash(0) {};

  insert( TagPhase tp ) {
    s.insert(tp);
    hash = hash + tp.tagID + tp.phase;
  };
  remove( TagPhase tp ) {
    s.insert(tp);
    hash = hash - tp.tagID - tp.phase;
  };
};

void
IntervalMap::add (Point lo, Point hi, TagPhase t)
{
  // in general, the new interval can overlap zero or one
  // interval on its left, enclose 0 or more contiguous intervals, then
  // overlap 0 or one interval on its right.

  Interval ni(lo, hi-lo);
  Interval rt(hi, 0);
  auto i = e.lower_bound(rt);
  // i is the first interval whose start could overlap the new interval;
  // it's possible the previous interval could overlap the new interval
  // on the right.
  if (i == e.end() || ! overlaps(ni, i->first)) {
    // something like this, but done by the DFA which owns all nodes
    // and manages the sets e.g. setToNode map.
    Node n = new Node();
    n.s = TagPhaseSet(t);
    e.insert(std::pair(ni, n));
    return;
  }
  if (i != e.begin())
    --i;
  if (! 
  auto j = i;
  --j;
  if (j != 











class DAG;
class Node;
class NodePtr;

typedef interval_map < Gap, Node * > FastEdges;
typedef interval_map < Gap, NodePtr > Edges;

class NodePtr {
  // class to wrap pointers to nodes, so that we can overload 
  // the += and -= operators used by icl::interval_map.

private:
  Node * p;

public:
  NodePtr();
  NodePtr(Node *n); // wrap a node in a nodeptr
  NodePtr(const NodePtr &n); // invoke copy constructor of Node class
  ~NodePtr(); // allow usecounting for referred-to node
  NodePtr& operator += (const NodePtr & right);
  NodePtr& operator -= (const NodePtr & right);
  bool operator == (const NodePtr& right) const;
  
  friend class Node;
  friend class DAG;
};

class Node {
protected:
  const TagPhaseSet * s; // TagPhase elements in set; points to set which is the
  // key in DAG::setToNode

  Edges e; // transitions from this node using NodePtr; used in building / modifying the graph
  FastEdges fe; // transitions from this node using Node *
  int l; // label for node; indicates creation order
  int uc; // number of times this node referred to in graph.
  DAG * owner;

  friend class DAG;
  friend class NodePtr;
  
public:
  Node();
  //  Node(const Node & right);
  Node& operator += (const Node & right);
  Node& operator -= (const Node & right);
  void remove();
};


class DAG {
protected:
  int nodesAlloc;    // number of nodes allocated (and possibly later deleted)
  int nodesUsed; // number of nodes actually in tree.
  Node * root; // root of DAG

  typedef std::map < TagPhaseSet, Node * > SetToNode;
  SetToNode setToNode; // will own all TagPhaseSets

public:

  DAG();
  Node * makeNode();
  Node * makeNode(TagPhase & t);
  void augmentNode(Node * n1, const Node & n2);
  void reduceNode(Node * n1, const Node & n2);
  void copyNode(Node * n1, const Node &n2);
  void removeNode(Node * n1);

  Node copy(Node &n);

  void add(TagPhase tag);

  void add(TagPhase tag, double tlo, double thi);

  void add(Node &n, TagPhase tag, double tlo, double thi);

  void add_rec(TagPhase tag, TagPhase newtag, double tlo , double thi);

  void add_rec(Node & n, TagPhase tag, TagPhase newtag, double tlo , double thi);

  void validate();

  void validate(Node & n);

  void del(TagPhase tag);

  void del_rec(TagPhase tp, double tlo, double thi); 

  void del_rec(Node & n, TagPhase tp, double tlo, double thi);
  
  void viz(std::ostream &out);

  void dumpNodes();

  void addTag(TagID id, vector < double > gaps, double tol, double maxTime);

  void delTag(TagID id, vector < double > gaps, double tol, double maxTime);

};

NodePtr& NodePtr::operator += (const NodePtr & right)
{
  (*p) += *(right.p);  // invoke += operator of Node class
  return *this; 
};
  
NodePtr& NodePtr::operator -= (const NodePtr & right)
{
  (*p) -= *(right.p);  // invoke -= operator of Node class
  return *this; 
};

NodePtr::NodePtr() 
{
  // invoke empty constructor of Node class
  p = new Node();
};

NodePtr::NodePtr(Node *n) : p(n)
{
};

NodePtr::NodePtr(const NodePtr & right) 
{
  p = right.p;
  ++(p->uc);
  // // invoke copy constructor of Node class
  // p = new Node(* right.p);
  // cout << "Doing NodePtr copy ctor with p->s = "<< p->s << endl;
  // if (p->owner)
  //   p->owner->copyNode(p, * right.p);
};

NodePtr::~NodePtr() 
{
  // invoke remove method of Node class
  p->remove();
};

Node& Node::operator += (const Node & right)
{
  // either this node or the right node will have a DAG owner
  if (owner == 0)
    owner = right.owner;
  owner->augmentNode(this, right);
  return *this; 
};
  
Node& Node::operator -= (const Node & right)
{
  // either this node or the right node will have a DAG owner
  if (owner == 0)
    owner = right.owner;
  owner->reduceNode(this, right);
  return *this; 
};
  
Node::Node() {
  s = new TagPhaseSet();
  uc = 1;
};

// Node::Node(const Node & right) {
//   cout << "Got to Node(right) const with set ptr " << s << " and right.s ptr " << right.s << " and right.uc == " << right.uc << endl;
//   e = right.e;
//   s = right.s;
//   fe = right.fe;
//   l = ++
//   if (owner == 0)
//     owner = right.owner;
//   if (owner)
//     owner->copyNode(this, right);
// };  

void
Node::remove() {
  if (owner) {
    owner->removeNode(this);
  } else {
    delete s;
    delete this;
  }
};

bool NodePtr:: operator == (const NodePtr& right) const {
  return p->s == right.p->s;
}


//typedef interval_map < Gap, NodePtr > Edges;


Node * DAG::makeNode() {
  cout << "got to makeNode" << endl;
  Node * n = new Node();
  cout << "call to new Node() left s = " << n->s << endl;
  n->owner = this;
  n->l = ++nodesAlloc;
  return n;
};

Node * DAG::makeNode(TagPhase &t) {
  cout << "got to makeNode with " << t << endl;
  Node * n = makeNode();
  TagPhaseSet s;
  s.insert(t);
  s.insert(t.onlyID());
  setToNode.insert(make_pair(s, n));
  return(n);
};


DAG::DAG() : nodesAlloc(0), nodesUsed(0)
{
  root = new Node();
  auto n = setToNode.insert(make_pair(TagPhaseSet(), root));
  root->s = & n.first->first;
};

  // EVENTUALLY: protected:

Node DAG::copy(Node &n) {
    Node rv = n;
    rv.l = ++nodesAlloc;
    return rv;
  }

void DAG::add(TagPhase tag) {
  // add a tagphase to the root node's set
  TagPhaseSet ns;
  ns.insert(root->s->begin(), root->s->end());
  ns.insert(tag);
  ns.insert(tag.onlyID());
  setToNode.erase(* root->s);
  auto n = setToNode.insert(make_pair(ns, root));
  root->s = & n.first->first;
};

void  DAG::add(TagPhase tag, double tlo, double thi) {
  add(* root, tag, tlo, thi);
};

void  DAG::add(Node &n, TagPhase tag, double tlo, double thi) {
  // add an edge from a node to one with tag for the interval [tlo,
  // thi)
  n.e.add(make_pair( interval < double > :: closed(tlo, thi), 
                     NodePtr(makeNode(tag))));

  // the icl interval_map code will call other methods through
  // NodePtr
};

//   // iterate over segments, looking for those having the
//   // just-added TagPhase; for each of these, build a new node.
//   for(auto i = n.e.begin(); i != n.e.end(); ++i) {
//     auto ss = i->second; // set of nodes
//     if (ss.count(tag)) {
//       if (ss.size() == 2) {
//         // this TagPhase (and its unphased sentinel) just added to set
//         Node d = new_node();
//         d.s.insert(tag);
//         d.s.insert(tag.onlyID());
//         setToNode.insert(make_pair(d.s, d));
//       } else {
//         // we want to copy the node corresponding to the original
//         // set, i.e. before this TagPhase was added
//         auto sm = ss;
//         sm.erase(tag);
//         sm.erase(tag.onlyID());
//         // if the original set still exists in the interval map,
//         // then we must copy its node; otherwise, the newly-added
//         // tagphase subsumed the original set, and we can augment
//         // its node.  If the original set is still in the interval
//         // map, it will be the set for either the preceding or the
//         // following interval.
//         auto nd = setToNode.find(sm);
//         if (nd == setToNode.end()) {
//           // original set no longer in interval map
//           return;
//         }
//         bool keep_old = false;
//         auto j = i;
//         if (i != n.e.begin() && (--j)->second == sm) {
//           keep_old = true;
//         } else {
//           j = i;
//           ++j;
//           if (j != n.e.end() && j->second == sm) {
//             keep_old = true;
//           }
//         }
//         Node d = copy(nd->second);
//         if (d.s.count(tag.onlyID()))
//           throw std::runtime_error("ID already in set");
//         Node & n = (setToNode.insert(make_pair(ss, d)).first) -> second;
//         n.s = ss;
//         if (! keep_old) {
//           setToNode.erase(sm);
//         }
//       }
//     }
//   }
// };

void  DAG::add_rec(TagPhase tag, TagPhase newtag, double tlo , double thi) {
  add_rec(* root, tag, newtag, tlo, thi);
};

void  DAG::add_rec(Node & n, TagPhase tag, TagPhase newtag, double tlo , double thi) {
  // recursively search for nodes containing tag, and 
  // add add an edge to tag' where tag'.tagID = tag.tagID
  // and tag.phase = newphase, with interval [t-dt, t+dt]
  // Tag to a Node.
    
  // iterate over edges, adding to each child that has some phase
  // of the given tag.
  auto id = tag.onlyID();
  if (interval_count(n.e) > 0) {
    for(auto i = n.e.begin(); i != n.e.end(); ++i) {
      if (i->second.p->s->count(id)) {
        add_rec(* i->second.p, tag, newtag, tlo, thi);
      }
    }
    // possibly add edge from this node
  }
  if (n.s->count(tag))
    add(n, newtag, tlo, thi);
};


void  DAG::validate() {
  nodesUsed = 0;
  validate(* root);
  if (nodesUsed != setToNode.size())
    throw std::runtime_error("Node count != size of setToNode.");
};

void  DAG::validate(Node & n) {
  // recursively verify that each edge leads to a node
    
  // iterate over edges, adding to each child that has some phase
  // of the given tag.
  ++nodesUsed;
  if (interval_count(n.e) > 0) {
    for(auto i = n.e.begin(); i != n.e.end(); ++i) {
      auto nn = setToNode.find(* (i->second.p->s));
      if (nn == setToNode.end()) {
        cout << "No node for set:\n" << * (i->second.p->s);
        throw std::runtime_error("Edge with no corresponding node.");
      }
      validate(* (nn->second));
    }
  }
};


void  DAG::del(TagPhase tag) {
  TagPhaseSet ns;
  ns.insert(root->s->begin(), root->s->end());
  ns.erase(tag);
  ns.erase(tag.onlyID());
  setToNode.erase(* root->s);
  auto n = setToNode.insert(make_pair(ns, root));
  root->s = & n.first->first;
};

void  DAG::del_rec(TagPhase tp, double tlo, double thi) {
  del_rec(* root, tp, tlo, thi);
};

void  DAG::del_rec(Node & n, TagPhase tp, double tlo, double thi) {
  // delete the entire subgraph for the tag with ID tp.tagID
  // tp.phase should be -1.
  // we do this child-first, depth-first recursively.
  // When processing this node, we first correct the setToNode
  // mapping to reflect removal of tp, then correct the interval_map.
  if (interval_count(n.e) > 0) {
    auto id = tp.onlyID();
    //    bool usedHere = false;
    for(auto i = n.e.begin(); i != n.e.end(); ++i) {
      if (i->second.p->s->count(id)) {
        auto nn = setToNode.find(* (i->second.p->s));
        if (nn != setToNode.end()) {
          del_rec(* nn->second, tp, tlo, thi);
          // if (nn->second.s.count(tp)) {
          //   usedHere = true;
          //   // correct the setToNode mapping to reflect that
          //   // it will be the reduced set now mapping to the 
          //   // node.  If there is already a node for the reduced
          //   // set, then just delete the node for the current set.
          //   auto sm = i->second;
          //   sm.erase(tp);
          //   sm.erase(tp.onlyID());
          //   auto rn = setToNode.find(sm);
          //   if (rn != setToNode.end()) {
          //     setToNode.erase(nn);
          //   } else {
          //     Node cp = nn->second;
          //     cp.s = sm;
          //     setToNode.erase(nn);
          //     setToNode.insert(make_pair(sm, cp));
          //   }
          // }
        }
      }
    }
    //    if (usedHere) {
    auto period = interval < double > :: closed(tlo, thi);
    n.e.subtract(make_pair( period, NodePtr(makeNode(tp))));
    //}
  }
    
};
  
void  DAG::viz(std::ostream &out) {
  // dump graph in the "dot" format for the graphviz package
  // These .gv files can be converted to nice svg plots like so:
  // for x in *.gv; do dot -Tsvg $x > ${x/gv/svg}; done

  out << "digraph TEST {\n";

  // generate node symbols and labels
  for (auto i = setToNode.begin(); i != setToNode.end(); ++i) {
    // define the node, labelled with its TagPhase set
    out << "a" << i->second->l <<
      "[label=\"a" << i->second->l << "=" << i->second->s<< "\"];\n";

    // define each edge, labelled by its interval
    for(auto j = i->second->e.begin(); j != i->second->e.end(); ++j) {
      const TagPhaseSet * m = j->second.p->s;
      if (m) {
        auto n = setToNode.find(*m);
        if (n != setToNode.end()) {
          out << "a" << i->second->l << " -> a" << (n->second->l) << "[label = \""
              << j->first << "\"];\n";
        }
      }
    }
  }
  out << "}\n";
};

void  DAG::dumpNodes() {
  cout << "There are " << nodesUsed << " nodes used and " << nodesAlloc << " allocated.\n" << "and " << setToNode.size() << " sets in the map.\n";
  for (auto i = setToNode.begin(); i != setToNode.end(); ++i) {
    cout << "   Node (a" << i->second->l << ") and set ";
    for (auto j = i->first.begin(); j != i->first.end(); ++j) {
      cout << *j << " ";
    }
    cout << endl;
  }
};

void  DAG::addTag(TagID id, vector < double > gaps, double tol, double maxTime) {
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

void  DAG::delTag(TagID id, vector < double > gaps, double tol, double maxTime) {
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


void 
DAG::augmentNode(Node * n1, const Node & n2) {
  // augment n1 from n2
  cout << "Got to DAG augmentNode\nwith ";
  if (n1->s) 
    cout << * (n1->s);
  else 
    cout << " empty s1 ";
  cout << "\nand ";
  if (n2.s)
    cout << * n2.s;
  else 
    cout << " empty s2 ";
  cout << endl;
  
  TagPhaseSet ns = *n1->s;
  ns.insert(n2.s->begin(), n2.s->end());
  Node * n = new Node(* n1);
  n->e = n1->e; // copy
  n->fe = n1->fe; // copy
  auto i = setToNode.insert(make_pair(ns, n));
  n->s = & i.first->first;
};

void DAG::reduceNode(Node * n1, const Node & n2) {
  cout << "Got to DAG reduceNode\nwith ";
  if (n1->s) 
    cout << * (n1->s);
  else 
    cout << " empty s1 ";
  cout << "\nand ";
  if (n2.s)
    cout << * n2.s;
  else 
    cout << " empty s2 ";
  cout << endl;
};

void DAG::copyNode(Node * n1, const Node &n2) {
  cout << "Got to DAG copyNode\nwith ";
  if (n1->s) 
    cout << * (n1->s);
  else 
    cout << " empty s1 ";
  cout << "\nand ";
  if (n2.s)
    cout << * n2.s;
  else 
    cout << " empty s2 ";
  cout << endl;
};

void DAG::removeNode(Node * n1) {
  cout << "Got to DAG removeNode\nwith";
  if (n1->s)
    cout << * (n1->s);
  else
    cout << "empty set";
  cout << endl;
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

  DAG * G = new DAG();

  // Using del = 0.5:
  // add tag A with gaps 5,    7,    3
  // add tag B with gaps 2.75, 8.1, 4.75
  // add tag C with gaps 5.1,  7.8,  3.1

  int n = 0;
  double d = 0.5;

#define SHOW                                                            \
  {                                                                     \
    std::ofstream of(string("./test") + std::to_string(++n) + ".gv");   \
    G->viz(of);                                                         \
    G->dumpNodes();                                                     \
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

