#include <set>
#include <map>
#include <iostream>
#include <stdexcept>
#include <cmath>

struct Node;
typedef double Point;
typedef int ID;
typedef size_t Hash;
struct Set;

class Set {
  std::set < ID > s;
  Hash hash;
  int _label;
  static int numSets;
  static Set * _empty;
  static std::set < Set * > allSets;

public:
  static Set * empty() { return _empty;};
  int label() { return _label;};

  Set(const Set &s) {std::cout << "Called Set copy ctor with " << s._label << std::endl;};

  Set() : _label(numSets++) {allSets.insert(this); std::cout << "Called Set()\n";};

  Set(ID p) : _label(numSets++) {s.insert(p);allSets.insert(this); std::cout << "Called Set(p) with p = " << p << std::endl;};

  void augment(ID i) { if(this == _empty) throw std::runtime_error("Augmenting empty set"); s.insert(i);};

  Set * reduce(ID i) { 
    // remove i from set; return pointer to empty set if 
    // reduction leads to that
    if(this == _empty) 
      throw std::runtime_error("Reducing empty set"); 
    s.erase(i);
    if (s.size() == 0) {
      allSets.erase(this);
      delete this;
      return _empty;
    }
    return this;
  };

  Set * clone() { if (this == _empty) return this; Set * ns = new Set(); ns->s = s; return ns;};

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
  };

  bool operator==(const Set & s) {
    return (this->s == s.s);
  };
};

Set * Set::_empty = 0;
std::set < Set * > Set::allSets = std::set < Set * > ();

class Node {
  std::map < double, Node * > e;
  Set * s;

  int label;
  static int numNodes;

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
    s = Set::empty();
    if (edges) {
      e.insert(std::make_pair(-1.0 / 0.0, empty));
      e.insert(std::make_pair( 1.0 / 0.0, empty));
    }
  };

  Node(const Node &n) { std::cout << "Called Node copy ctor with " << n.label << std::endl;};

  Node(ID p) : label(numNodes++) {s = new Set(p); std::cout << "Called Node (p) with p = " << p << " and label is " << label << std::endl;};

  void add (Point b1, Point b2, ID p);
  void remove (Point b1, Point b2, ID p);

  void dump() {
    std::cout << "Node: " << label << " has " << e.size() << " entries in edge map:\n";
    for (auto i = e.begin(); i != e.end(); ++i) {
      std::cout << "   " << i->first << " -> Node for Set ";
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
  };
};

int Node::numNodes = 0;
int Set::numSets = 0;

void
Node::add (Point b1, Point b2, ID p) {
  // Get the largest endpoint not right of each new endpoint.  Because
  // of map entries for -Inf and +Inf, i, j are guaranteed be valid
  auto i = -- e.upper_bound(b1);
  auto j = -- e.upper_bound(b2);

  // If b1 is a new endpoint, map it to a Node including
  // what i maps to but augmented by {p}. (Otherwise, b1
  // is the same endpoint as i, so we just augment it
  // in the loop below.)

  if (i->first < b1) {
    newEdge( b1, i->second->cloneAugment(p));
    // advance to next existing endpoint; we increment twice
    // to skip over the newly added endpoint at b1, since
    // we have i->first < b1 < (++i)->first

    ++i;  ++i;  // Yes, twice. Read above
  }
  // augment all but the last endpoint overlapping the interval

  while (i->first < j->first) {
    i->second = i->second->augment(p);
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
    newEdge (b2,  j->second->clone());
    if (j->first >= b1)
      j->second = j->second->augment(p);
  }
};


void
Node::remove (Point b1, Point b2, ID p) {
  // iterate through endpoints in the range [b1, b2]
  // reducing them by {p}, and merging adjacent
  // endpoints that end up mapping to the same set

  auto i = e.lower_bound(b1);
  auto ip = i;
  --ip;

  while (i->first <= b2) {
    if (i->first < b2)
      i->second = i->second->reduce(p);
    if (* (i->second->s) == *(ip->second->s)) {
      auto idel = i;
      ++i;
      e.erase(idel);
    } else {
      ip = i;
      ++i;
    }
  }
};

Node * Node::empty = 0;

main (int argc, char * argv[] ) {
  Node::init();

  Node root;
  root.add(5, 10, 1000);
  root.dump();
  Set::dumpAll();

  root.add(7, 12, 1001);
  root.dump();
  
  root.add(4, 9, 1002);
  root.dump();
  
  root.add(3, 13, 1003);
  root.dump();
  
  root.add(7, 9, 1004);
  root.dump();
  
  root.add(1, 2, 1005);
  root.dump();
  
  root.add(0, 1, 1006);
  root.dump();
  
  root.add(14, 16, 1007);
  root.dump();
  
  root.add(14, 18, 1008);
  root.dump();
  
  root.add(12, 19, 1009);
  root.dump();

  Set::dumpAll();

  //-------------------------------------------------------------

  root.remove(5, 10, 1000);
  root.dump();

  root.remove(14, 16, 1007);
  root.dump();
  
  root.remove(3, 13, 1003);
  root.dump();
  
  root.remove(1, 2, 1005);
  root.dump();

  root.remove(7, 12, 1001);
  root.dump();
  
  root.remove(4, 9, 1002);
  root.dump();

  root.remove(7, 9, 1004);
  root.dump();
  
  root.remove(12, 19, 1009);
  root.dump();

  root.remove(0, 1, 1006);
  root.dump();
  
  root.remove(14, 18, 1008);
  root.dump();
  
};

  



