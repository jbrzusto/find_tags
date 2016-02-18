#include <set>
#include <map>
#include <iostream>
#include <stdexcept>
#include <cmath>
#include <memory>
#include <unordered_map>

class Node;
typedef double Point;
typedef int ID;
class Set;
typedef size_t SetHash;

class Graph;

class Set {
  friend class Node;
  friend class Graph;
  friend class hashSet;
  friend class SetEqual;
  std::set < ID > s;
  int _label;
  SetHash hash;
  static int numSets;
  static Set * _empty;
  static std::set < Set * > allSets;

public:
  static Set * empty() { return _empty;};
  int label() { return _label;};

  Set(const Set &s) {    // std::cout << "Called Set copy ctor with " << s._label << std::endl;
    allSets.insert(this);
  };

  Set() : _label(numSets++) {
    allSets.insert(this);
    //    std::cout << "Called Set()\n";
  };

  Set(ID p) : _label(numSets++) {
    s.insert(p);
    hash ^= p; 
    allSets.insert(this); 
    //    std::cout << "Called Set(p) with p = " << p << std::endl;
  };

  bool exists( std::set < ID > s ) {
    for (auto i = allSets.begin(); i != allSets.end(); ++i)
      if ((*i) -> s == s)
        return true;
    return false;
  };

  Set * augment(ID p) { 
    // augment this set with p, unless this set is empty
    // in which case return a new set with just p;
    if(this == _empty) 
      return new Set(p);
    s.insert(p);
    hash ^= p;
    return this;
  };

  Set * reduce(ID p) { 
    // remove p from set; return pointer to empty set if 
    // reduction leads to that
    if(this == _empty) 
      throw std::runtime_error("Reducing empty set"); 
    s.erase(p);
    hash ^= p;
    if (s.size() == 0) {
      delete this;
      return _empty;
    }
    return this;
  };

  Set * clone() {
    if (this == _empty) 
      return this; 
    Set * ns = new Set(); 
    ns->s = s; 
    return ns;
  };

  Set * cloneAugment(ID p) {
    // return pointer to clone of this Set, augmented by p
    if (this == _empty)
      return new Set(p);
    Set * ns = new Set(); 
    ns->s = s; 
    ns->s.insert(p);
    return ns;
  };
      

  Set * cloneReduce(ID p) {
    // return pointer to clone of this Set, reduced by p
    if (s.count(p) == 0)
      throw std::runtime_error("Set::cloneReduce(p) called with p not in set");
    Set * ns = new Set(); 
    ns->s = s; 
    ns->s.erase(p);
    return ns;
  };
      
  static void dumpAll() {
    for (auto i = allSets.begin(); i != allSets.end(); ++i) {
      std::cout << "Set " << (*i)->_label << " has " << (*i)->s.size() << " elements:\n";
      for (auto j = (*i)->s.begin(); j != (*i)->s.end(); ++j) {
        std::cout << "   Node " << *j << std::endl;
      }
    }
  };

  void dump() {
    if (s.size() == 0) {
      std::cout << "(Empty)";
    } else {
      for (auto j = s.begin(); j != s.end(); ++j) {
        std::cout << *j << " ";
      }
    }
  }
    
  static void init() {
    allSets = std::set < Set * > ();
    _empty = new Set();
    allSets.insert(_empty);
  };

  bool operator== (const Set & s) const {
    return hash == s.hash && this->s == s.s;
  };
};

Set * Set::_empty = 0;
std::set < Set * > Set::allSets = std::set < Set * > ();

struct hashSet {
  // hashing function used in DFA::setToNode
  size_t operator() (const Set * x) const {
    return x->hash;
  };
};

class Node {
  friend class Graph;
  std::map < double, Node * > e;
  Set * s;
  int useCount;

  int label;
  static int numNodes;

  void link() {
    ++ useCount;
  };

  void unlink() {
    if (-- useCount == 0)
      delete this;
  };

  Node * clone() { 
    Node * n = new Node();
    n->s = s->clone(); 
    return n;
  };

  Node * augment (ID p) {
    if (this == empty)
      return new Node(p);
    if (s == Set::empty())
      s = new Set(p);
    else
      s->augment(p);
    return this;
  };

  Node * reduce (ID p) {
    if (this == empty || s == Set::empty())
      throw std::runtime_error("called reduce on Node::empty");

    s = s->reduce(p);
    if (s == Set::empty())
      return empty;

    return this;
  };
      
  Node * cloneAugment (ID p) {
    // if (this == empty)
    //   throw std::runtime_error("called cloneAugment on Node::empty");
    std::cout << "calling cloneAugment on " << label << " for " << s->label() << " with " << p << std::endl;
    Node * n = clone();
    if (n->s == Set::empty())
      n->s = new Set(p);
    else
      n->s->augment(p);
    return n;
  };


public:

  Node(bool edges = true) : label(numNodes++) {
    std::cout << "Called Node(edges) ctor with edges = " << edges << " label: " <<  label << std::endl;
    s = Set::empty();
    if (edges) {
      e.insert(std::make_pair(-1.0 / 0.0, empty));
      e.insert(std::make_pair( 1.0 / 0.0, empty));
    }
  };

  Node(const Node &n) { 
    std::cout << "Called Node copy ctor with " << n.label << std::endl;
  };

  Node(ID p) : label(numNodes++) {
    s = new Set(p); 
    std::cout << "Called Node (p) with p = " << p << " and label is " << label << std::endl;
  };

  void add (Point b1, Point b2, ID p);
  void remove (Point b1, Point b2, ID p);

  void dump() {
    std::cout << "Node: " << label << " has " << e.size() << " entries in edge map:\n";
    for (auto i = e.begin(); i != e.end(); ++i) {
      std::cout << "   " << i->first << " -> Node (" << i->second->label << ", uc=" << i->second->useCount << ") for Set ";
      i->second->s->dump();
      std::cout << std::endl;
    };
  };

  static Node * empty;
  static void init() {
    Set::init();
    empty = new Node(false);
  };

  void newEdge (Point b, Node * p) {
    e.insert (std::make_pair(b, p));
    p->link();
  };
};

int Node::numNodes = 0;
int Set::numSets = 0;

Node * Node::empty = 0;

struct SetEqual {
  // comparison function used in DFA::setToNode
  bool operator() ( const Set * x1, const Set * x2 ) const {
    return x1->s == x2->s; // fixme slow direct comparison
  };
};


class Graph {
  Node * root;

public:
  // map from sets to nodes
  std::unordered_map < Set *, Node *, hashSet, SetEqual > setToNode; 

  void insert (const ID &t); // insert tagphase to root node
  void erase (const ID &t); // erase tagphase from root node
  // void insert (DFA_Node *n, const ID &t); // insert tagphase to non-root node
  // void erase (DFA_Node *n, const ID &t); // erase tagphase from non-root node
  void insert (Point b1, Point b2, ID p); // insert transitions from n for t
  void erase (Point b1, Point b2, ID p); // erase transitions from n for t
  void insert (Node *n, Point b1, Point b2, ID p); // insert transitions from n for t
  void erase (Node *n, Point b1, Point b2, ID p); // erase transitions from n for t
  void insert_rec (Node *n, Point lo, Point hi, ID tFrom, ID tTo); // recursively insert a transition from tFrom to tTo
  void erase_rec (Node *n, Point lo, Point hi, ID tFrom, ID tTo); // recursively erase all transitions from tFrom to tTo
  Node * advance (Node *n, Point dt);

  Node * addEdge (Node * head, Point b, Set *s, ID p); // add edge from head at point b to node for set union(s, {p}); return pointer to tail node
  void removeEdge (Node * head, Point b, Node * tail); // remove edge at point b from head to tail
  void augmentEdge (Node * head, Point b, Node * tail, ID p); // existing edge from head at point b will now go to node for set union(tail->s, {p})
                                                              // if there is no node yet for that set, create one by cloning tail.
  void reduceEdge (Node * head, Point b, Node * tail, ID p);  // existing edge from head at point b will now go to node for set diff(tail->s, {p})
                                                              // use count for node tail will decrease by one. If no node that set, 
                                                              // create one by cloning tail and removing p from its set.
  
  void mapSet( Set * s, Node * n) {
    setToNode.insert(std::make_pair(s, n));
  };

  void unmapSet ( Set * s) {
    setToNode.erase(s);
  };

  Graph() {
    root = new Node;
    mapSet(root->s, root);
  };

  Node * nodeForSet (Set * s) {
    // find the node for set s.  If found, return the node
    // and delete this copy of s.  If not found, create a new
    // node for this set, which becomes the owner.
    auto i = setToNode.find(s);
    if (i != setToNode.end()) {
      delete s;
      return i->second;
    }
    Node * nn = new Node();
    nn->s = s;
    mapSet(s, nn);
    return nn;
  };

  Node * nodeAugment (Node * n, ID p, bool doLinks = true) {
    // return the node for the set:  n->s U {p}
    // If it doesn't already exist, clone n then
    // augment it by p and return that.
    Set * s = n->s->cloneAugment(p);
    auto i = setToNode.find(s);
    if (i != setToNode.end()) {
      delete s;
      if (doLinks) {
        // transfer one usage from original node to augmented one
        i->second->link();
        n->unlink();
      }
      return i->second;
    }
    if (n->useCount == 1) {
      // special case to save work
      unmapSet(n->s);
      n->s->augment(p);
      mapSet(n->s, n);
      delete s;
      return n;
    }
    Node * nn = new Node();
    nn->s = s;
    mapSet(s, nn);
    if (doLinks) {
      nn->link();
      n->unlink();
    }
    return nn;
  };

  Node * nodeReduce (Node * n, ID p) {
    // return the node for the set:  n->s \ {p}
    // If it doesn't already exist, clone n then
    // reduce it by p and return that.
    Set * s = n->s->cloneReduce(p);
    auto i = setToNode.find(s);
    if (i != setToNode.end()) {
      delete s;
      i->second->link();
      return i->second;
    }
    if (n->useCount == 1) {
      // special case to save work
      unmapSet(n->s);
      n->s->reduce(p);
      mapSet(n->s, n);
      delete s;
      return n;
    }
    Node * nn = new Node();
    nn->s = s;
    mapSet(s, nn);
    nn->link();
    return nn;
  };

  void dump () {
    root->dump();
  };

};


Node *
Graph::addEdge (Node * head, Point b, Set *s, ID p) {
  // add edge from head at point b to node for set union(s, {p}); return pointer to tail node
};

void
Graph:: insert (const ID &t) {
  root->s->augment(t);
  // note: we don't remap root in setToNode, since we
  // never try to lookup the root node from its set.
};

void
Graph:: erase (const ID &t) {
  root->s->reduce(t);
  // note: we don't remap root in setToNode
};

void 
Graph::insert (Point b1, Point b2, ID p) {
  insert (root, b1, b2, p);
};


void 
Graph::insert (Node *n, Point b1, Point b2, ID p)
{
  // From the node at n, add appropriate edges to other nodes
  // given that the segment [b1, b2] is being augmented by p.

  // Get the largest endpoint not right of each new endpoint.  Because
  // of map entries for -Inf and +Inf, i, j are guaranteed be valid
  auto i = -- (n->e.upper_bound(b1));
  auto j = -- (n->e.upper_bound(b2));

  // If b1 is a new endpoint, map it to a Node including
  // what i maps to but augmented by {p}. (Otherwise, b1
  // is the same endpoint as i, so we just augment it
  // in the loop below.)

  if (i->first < b1) {
    n->newEdge( b1, nodeAugment(i->second, p, false));
    // advance to next existing endpoint; we increment twice
    // to skip over the newly added endpoint at b1, since
    // we have i->first < b1 < (++i)->first

    ++i;  ++i;  // Yes, twice. Read above
  }
  // augment all but the last endpoint overlapping the interval

  while (i->first < j->first) {
    i->second = nodeAugment(i->second, p);
    ++i;
  }

  // j, the last endpoint, requires special treatment to correctly
  // "end" the new segment containing {p}.

  // We have j->first <= b2:

  //    If "=", there's nothing to do, as j already maps to a node
  //    without {p}.

  //    If "<", we must add an endpoint at b2 which maps to the
  //    original Node mapped to by j (i.e. without {p}).  Then, if j
  //    was in [b1, b2], we need to augment its node with {p} since the
  //    loop above deliberately stopped before doing so.

  if (j->first < b2) {
    n->newEdge (b2,  j->second);
    if (j->first >= b1)
      j->second = nodeAugment(j->second, p);
  }
};
 

void
Graph::erase (Point b1, Point b2, ID p) {
  erase(root, b1, b2, p);
};

void
Graph::erase (Node *n, Point b1, Point b2, ID p) {
  // iterate through endpoints in the range [b1, b2]
  // reducing them by {p}, and merging adjacent
  // endpoints that end up mapping to the same node

  auto i = n->e.lower_bound(b1);
  auto ip = i;
  --ip;

  while (i->first <= b2) {
    if (i->first < b2)
      i->second = nodeReduce(i->second, p);
    if (i->second == ip->second) {
      auto idel = i;
      ++i;
      idel->second->unlink();
      n->e.erase(idel);
    } else {
      ip = i;
      ++i;
    }
  }
};

main (int argc, char * argv[] ) {
  Node::init();
  Graph g;

  g.insert(5, 10, 1000);
  g.dump();
  Set::dumpAll();

  g.insert(7, 12, 1001);
  g.dump();
  
  g.insert(4, 9, 1002);
  g.dump();
  
  g.insert(3, 13, 1003);
  g.dump();

  Set::dumpAll();

  g.insert(7, 9, 1004);
  g.dump();
  
  g.insert(1, 2, 1005);
  g.dump();
  
  g.insert(0, 1, 1006);
  g.dump();
  
  g.insert(14, 16, 1007);
  g.dump();
  
  g.insert(14, 18, 1008);
  g.dump();
  
  g.insert(12, 19, 1009);
  g.dump();

  Set::dumpAll();

  //-------------------------------------------------------------

  g.erase(5, 10, 1000);
  g.dump();

  g.erase(14, 16, 1007);
  g.dump();
  
  g.erase(3, 13, 1003);
  g.dump();
  
  g.erase(1, 2, 1005);
  g.dump();

  g.erase(7, 12, 1001);
  g.dump();
  
  g.erase(4, 9, 1002);
  g.dump();

  g.erase(7, 9, 1004);
  g.dump();
  
  g.erase(12, 19, 1009);
  g.dump();

  g.erase(0, 1, 1006);
  g.dump();
  
  g.erase(14, 18, 1008);
  g.dump();

  Set::dumpAll();
  
};

  



