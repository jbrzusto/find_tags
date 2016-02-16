#include <set>
#include <map>
#include <iostream>

struct Node;
typedef double Point;
typedef int ID;

struct Set;

struct Set {
  std::set < ID > s;
  int label;
  static int numSets;
  Set(const Set &s) {std::cout << "Called Set copy ctor with " << s.label << std::endl;};

  Set() : label(numSets++) {allSets.insert(this); std::cout << "Called Set()\n";};
  Set(ID p) : label(numSets++) {s.insert(p);allSets.insert(this); std::cout << "Called Set(p) with p = " << p << std::endl;};
  static Set * empty;
  static std::set < Set * > allSets;
  void augment(ID i) { s.insert(i);};
  Set * clone() { if (this == empty) return this; Set * ns = new Set(); ns->s = s; return ns;};
  static void dumpAll() {
    for (auto i = allSets.begin(); i != allSets.end(); ++i) {
      std::cout << "Set " << (*i)->label << " has " << (*i)->s.size() << " elements:\n";
      for (auto j = (*i)->s.begin(); j != (*i)->s.end(); ++j) {
        std::cout << "   Node " << *j << std::endl;
      }
    }
  };
  static void init() {
    allSets = std::set < Set * > ();
    empty = new Set();
  };
};

struct Node {
  std::map < double, Node * > e;
  Set * s;

  int label;
  static int numNodes;
  Node() : label(numNodes++) {s = Set::empty;};
  Node(const Node &n) { std::cout << "Called Node copy ctor with " << n.label << std::endl;};
  Node(ID p) : label(numNodes++) {s = new Set(p); std::cout << "Called Node (p) with p = " << p << " and label is " << label << std::endl;};
  Node * clone() { Node * n = new Node(); n->s = s->clone(); return n;};
  static Node * empty;
  void augment (ID p) {
    if (s == Set::empty)
      s = new Set(p);
    else
      s->augment(p);
  };

  Node * cloneAugment (ID p) {
    std::cout << "calling cloneAugment on " << label << " for " << s->label << " with " << p << std::endl;
    Node * n = clone();
    if (n->s == Set::empty)
      n->s == new Set(p);
    else
      n->s->augment(p);
    return n;
  };

  void add (Point b1, Point b2, ID p);
  void dump() {
    std::cout << "Node: " << label << " has " << e.size() << " entries in edge map:\n";
    for (auto i = e.begin(); i != e.end(); ++i) {
      std::cout << "   " << i->first << " -> Node for Set " << i->second->s->label << std::endl;
    };
  };
  static void init() {
    Set::init();
    empty = new Node();
  };
};

int Node::numNodes = 0;
int Set::numSets = 0;
Set * Set::empty = 0;
Node * Node::empty = 0;
std::set < Set * > Set::allSets = std::set < Set * > ();

void
Node::add (Point b1, Point b2, ID p) {
  auto i = e.lower_bound(b1);
  if (i == e.end()) {
    e.insert (std::make_pair(b1, new Node(p)));
    e.insert (std::make_pair(b2, Node::empty));
    return;
  }
  if (i->first >= b2) {
    e.insert (std::make_pair(b1, new Node(p)));
    if (i->first > b2)
      e.insert (std::make_pair(b2, Node::empty));
      
    return;
  }
  while (i->first < b2) {
    if (i->first == b1) {
      auto j = i;
      ++j;
      if (j != e.end() && b2 > j->first) {
        i->second->augment(p);
      } else {
        e.insert (std::make_pair(b2, i->second->clone()));
        i->second->augment(p);
        break;
      }
      b1 = std::min(b2, j->first);
      i = j;
    } else {
      // i->first > b1
      if (i == e.begin()) {
        e.insert (std::make_pair(b1, new Node(p)));
      } else {
        auto j = i;
        --j;
        auto nn = j->second->cloneAugment(p);
        e.insert (std::make_pair(b1, nn));
      }
      b1 = std::min(b2, i->first);
    }
  }
};


main (int argc, char * argv[] ) {
  Node::init();

  Node root;
  root.add(5, 10, 1000);
  root.dump();
  Set::dumpAll();

  root.add(7, 12, 1001);
  root.dump();
  Set::dumpAll();

  root.add(2, 8, 1002);
  root.dump();
  Set::dumpAll();
};

  



