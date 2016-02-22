#include <set>
#include <map>
#include <iostream>
#include <stdexcept>
#include <cmath>
#include <memory>
#include <unordered_map>


class Node;
typedef double Point;
typedef int TagID;
typedef short Phase;
typedef std::pair < TagID, Phase > TagPhase;
typedef std::unordered_multimap < TagID, Phase > TagPhaseSet;

TagPhase TP(TagID t, Phase p) {
  return std::make_pair(t, p);
};


template<class CharType, class CharTraits>
std::basic_ostream<CharType, CharTraits> &operator<<
(std::basic_ostream<CharType, CharTraits> &stream, TagPhase const& value)
{
  if (value.second >= 0)
    return stream << "# " << value.first << " (" << value.second << ") ";
  return stream;
};


class Set;
typedef size_t TagPhaseSetHash;

class Graph;

class Set {
  friend class Node;
  friend class Graph;
  friend class hashSet;
  friend class SetEqual;
  TagPhaseSet s;
  int _label;
  TagPhaseSetHash hash;
  static int numSets;
  static Set * _empty;
  static std::set < Set * > allSets;

public:
  static Set * empty() { return _empty;};
  int label() { return _label;};

  ~Set() {
    allSets.erase(this);
  };

  Set(const Set &s) {    // std::cout << "Called Set copy ctor with " << s._label << std::endl;
    allSets.insert(this);
    this->s = s.s;
    this->hash = s.hash;
  };

  Set() : s(), _label(numSets++) , hash(0) {
    allSets.insert(this);
    //    std::cout << "Called Set()\n";
  };

  Set(TagPhase p) : s(), _label(numSets++), hash(hashTP(p)) {
    s.insert(p);
    allSets.insert(this); 
    //    std::cout << "Called Set(p) with p = " << p << std::endl;
  };

  Set * augment(TagPhase p) { 
    // augment this set with p, unless this set is empty
    // in which case return a new set with just p;
    if(this == _empty) 
      return new Set(p);
    s.insert(p);
    hash ^= hashTP(p);
    return this;
  };

  Set * reduce(TagPhase p) { 
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

  void erase(TagPhase p) {
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

  int count(TagPhase p) {
    // count specific element p from multimap; i.e. match
    // by both key and value; returns 0 or 1
    auto r = s.equal_range(p.first);
    for (auto i = r.first; i != r.second; ++i)
      if (i->second == p.second)
        return 1;
    return 0;
  };
    

  Set * clone() {
    if (this == _empty) 
      return this; 
    Set * ns = new Set(); 
    ns->s = s; 
    ns->hash = hash;
    return ns;
  };

  Set * cloneAugment(TagPhase p) {
    // return pointer to clone of this Set, augmented by p
    if (this == _empty)
      return new Set(p);
    Set * ns = new Set(); 
    ns->s = s; 
    ns->s.insert(p);
    ns->hash = hash ^ hashTP(p);
    return ns;
  };
      

  Set * cloneReduce(TagPhase p) {
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
      
  static void dumpAll() {
    for (auto i = allSets.begin(); i != allSets.end(); ++i) {
      std::cout << "Set " << (*i)->_label << " has " << (*i)->s.size() << " elements:\n";
      for (auto j = (*i)->s.begin(); j != (*i)->s.end(); ++j) {
        std::cout << "   TagPhase " << *j << std::endl;
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

private:
  static TagPhaseSetHash hashTP (TagPhase tp) {
    return tp.first * tp.second;
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
  typedef std::map < double, Node * > Edges;
  Set * s;
  Edges e;
  int useCount;

  int label;
  static int numNodes;

  static Node * _empty;

  void link() {
    ++ useCount;
  };

  bool unlink() {
    return -- useCount == 0;
  };

  void drop() {
    if (s != Set::empty())
      delete s;
    delete this;
  };

  Node * clone() { 
    Node * n = new Node();
    n->s = s->clone(); 
    return n;
  };

  Node * augment (TagPhase p) {
    if (this == _empty)
      return new Node(p);
    if (s == Set::empty())
      s = new Set(p);
    else
      s->augment(p);
    return this;
  };

  Node * reduce (TagPhase p) {
    if (this == _empty || s == Set::empty())
      throw std::runtime_error("called reduce on Node::empty");

    s = s->reduce(p);
    if (s == Set::empty())
      return _empty;

    return this;
  };
      
  Node * cloneAugment (TagPhase p) {
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

protected:

  void _init() {
    useCount = 0;
    label = numNodes++;
  }

public:

  static void init() {
    Set::init();
    _empty = new Node();
  };

  static Node * empty() {
    return _empty;
  };
  
  Node() : s(Set::empty()), e() {
    _init();
    e.insert(std::make_pair(-1.0 / 0.0, _empty));
    e.insert(std::make_pair( 1.0 / 0.0, _empty));
  };

  Node(const Node &n) : s(n.s), e(n.e) { 
    _init();
    std::cout << "Called Node copy ctor with " << n.label << std::endl;
    useCount = 0;
  };

  Node(const Node *n) : s(n->s), e(n->e) {
    _init();
  };

  Node(TagPhase p) : s(new Set(p)), e() {
    _init();
  };

  
  void add (Point b1, Point b2, TagPhase p);
  void remove (Point b1, Point b2, TagPhase p);

  void dump(bool skipEdges=false) {
    std::cout << "Node: " << label << " has " << e.size() << " entries in edge map:\n";
    if (! skipEdges) {
      for (auto i = e.begin(); i != e.end(); ++i) {
        std::cout << "   " << i->first << " -> Node (" << i->second->label << ", uc=" << i->second->useCount << ") for Set ";
        i->second->s->dump();
        std::cout << std::endl;
      }
    }
  };

  void newEdge (Point b, Node * p) {
    e.insert (std::make_pair(b, p));
    p->link();
  };
};
int Node::numNodes = 0;
int Set::numSets = 0;

Node * Node::_empty = 0;

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

  // void insert (DFA_Node *n, const ID &t); // insert tagphase to non-root node
  // void erase (DFA_Node *n, const ID &t); // erase tagphase from non-root node
  void insert_rec (Node *n, Point lo, Point hi, TagPhase tFrom, TagPhase tTo); // recursively insert a transition from tFrom to tTo
  void erase_rec (Node *n, Point lo, Point hi, TagPhase tFrom, TagPhase tTo); // recursively erase all transitions from tFrom to tTo
  Node * advance (Node *n, Point dt);

  void removeEdge (Node * head, Point b, Node * tail); // remove edge at point b from head to tail
  void augmentEdge (Node * head, Point b, Node * tail, TagPhase p); // existing edge from head at point b will now go to node for set union(tail->s, {p})
                                                              // if there is no node yet for that set, create one by cloning tail.
  void reduceEdge (Node * head, Point b, Node * tail, TagPhase p);  // existing edge from head at point b will now go to node for set diff(tail->s, {p})
                                                              // use count for node tail will decrease by one. If no node that set, 
                                                              // create one by cloning tail and removing p from its set.
  
  void mapSet( Set * s, Node * n) {
    auto p = std::make_pair(s, n);
    setToNode.insert(p);
  };

  void unmapSet ( Set * s) {
    if (s != Set::empty())
      setToNode.erase(s);
  };

  Graph() : setToNode(100) {
    root = new Node();
    mapSet(Set::empty(), Node::empty());
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

  Node * nodeReduce (Node * n, TagPhase p) {
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


  Node * addEdge (Node * head, Point b, Set *s, TagPhase p) {
    // add edge from head at point b to node for set union(s, {p}); return pointer to tail node
  };

  void insert (const TagPhase &t) {
    root->s->augment(t);
    // note: we don't remap root in setToNode, since we
    // never try to lookup the root node from its set.
  };

  void erase (const TagPhase &t) {
    root->s->reduce(t);
    // note: we don't remap root in setToNode
  };

  void insert (Point b1, Point b2, TagPhase p) {
    insert (root, b1, b2, p);
  };

  void erase (Point b1, Point b2, TagPhase p) {
    erase(root, b1, b2, p);
  };

  Node::Edges::iterator ensureEdge ( Node *n, Point b) {
    // ensure there is an edge from node n at point b,
    // and returns an iterator to it

    // if it does not already exist, create an 
    // edge pointing to the same node already
    // implicitly pointed to for that point
    // (i.e. the node pointed to by the first endpoint
    // to the left of b)

    auto i = n->e.upper_bound(b);
    --i;  // i->first is now <= b
    if (i->first == b)
      // already have an edge at that point
      return i;
    n->e.insert(i, std::make_pair(b, i->second));
    // increase use count for that node
    i->second->link();
    // return iterator to the new edge, which
    // will be immediately to the right of i
    return ++i;
  };

  void unlinkNode (Node *n) {
    if (n->unlink() && n != Node::empty()) {
      unmapSet(n->s);
      n->drop();
    };
  };

  void augmentEdge(Node::Edges::iterator i, TagPhase p) {
    // given an existing edge, augment its tail node by p

    Node * n = i->second;
    Set * s = n->s->cloneAugment(p);
    auto j = setToNode.find(s);
    if (j != setToNode.end()) {
      // already have a node for this set
      unlinkNode(n);
      delete s;
      i->second = j->second;
      i->second->link();
      return;
    }
    if (n->useCount == 1) {
      // special case to save work: re-use this node
      unmapSet(n->s);
      n->s = n->s->augment(p);
      mapSet(n->s, n);
      delete s;
      return;
    }
    // create new node with augmented set, but preserving
    // its outgoing edges
    Node * nn = new Node(n);
    nn->s = s;
    mapSet(s, nn);
    // adjust incoming edge counts on old and new nodes
    unlinkNode(n);
    nn->link();
    i->second = nn;
  };


  void reduceEdge(Node::Edges::iterator i, TagPhase p) {
    // given an existing edge, reduce its tail node by p

    Node * n = i->second;
    if (n->s->count(p) == 0)
      return;
    Set * s = n->s->cloneReduce(p);
    auto j = setToNode.find(s);
    if (j != setToNode.end()) {
      // already have a node for this set
      if (s != Set::empty())
        delete s;
      i->second = j->second;
      i->second->link();
      unlinkNode(n);
      return;
    }
    if (n->useCount == 1) {
      // special case to save work: re-use this node
      unmapSet(n->s);
      n->s = n->s->reduce(p);
      mapSet(n->s, n);
      if (s != Set::empty())
        delete s;
      return;
    }
    // create new node with reduced set, but preserving
    // its outgoing edges
    Node * nn = new Node(n);
    nn->s = s;
    mapSet(s, nn);
    // adjust incoming edge counts on old and new nodes
    unlinkNode(n);
    nn->link();
    i->second = nn;
  };


  void dropEdgeIfExtra(Node * n, Node::Edges::iterator i) {
    // compare edge to its predecessor; if they map 
    // to the same nodes, remove this one and
    // adjust useCount
    // caller must guarantee i does not point to
    // first edge.

    auto j = i;
    --j;
    if (* (i->second->s) == * (j->second->s)) {
      unlinkNode(i->second);
      n->e.erase(i);
    };
  };

  void insert (Node *n, Point b1, Point b2, TagPhase p)
  {
    // From the node at n, add appropriate edges to other nodes
    // given that the segment [b1, b2] is being augmented by p.

    ensureEdge(n, b2);  // ensure there is an edge from n at b2 to the
                        // current node for that point

    auto i = ensureEdge(n, b1); // ensure there is an edge from n at
                                // b1 to the current node for that
                                // point

    // From the b1 to the last existing endpoint in [b1, b2), augment
    // each edge by p.

    while (i->first < b2) {
      augmentEdge(i, p);
      ++i;
    }
  };
    
  void erase (Node *n, Point b1, Point b2, TagPhase p) {
    // for any edges from n at points in the range [b1, b2]
    // remove p from the tail node.
    // If edges at b1 and/or b2 become redundant after 
    // this, remove them.

    auto i = n->e.lower_bound(b1);
    auto j = i; // save value for later
    while (i->first <= b2) {
      reduceEdge(i, p);
      ++i;
    }
    dropEdgeIfExtra(n, --i); // rightmost edge
    dropEdgeIfExtra(n, j);  // leftmost edge
  };

  void dumpSetToNode() {
    for (auto i = setToNode.begin(); i != setToNode.end(); ++i) {
      std::cout << "Set ";
      i->first->dump();
      std::cout << "Maps to Node ";
      if (i->second)
        i->second->dump(true);
      else
        std::cout << "(no Node)";
      std::cout << "\n";
    }
  };
  
  void validateSetToNode() {
    // verify that each set maps to a node that has it as a set!
    for (auto i = setToNode.begin(); i != setToNode.end(); ++i) {
      if (! i->second) {
        std::cout << "Null Node Ptr for set:" << std::endl;
        i->first->dump();
      } else if (i->second->s != i->first) {
        std::cout << "Invalid setToNode map:" << std::endl;
        dumpSetToNode();
        return;
      }
    }
    std::cout << "setToNode is okay" << std::endl;
  };
};

main (int argc, char * argv[] ) {
  Node::init();
  Graph g;

  g.insert(5, 10, TP(1000, 1));
  // g.dump();
  Set::dumpAll();

  g.insert(7, 12, TP(1001, 1));
  // g.dump();
  
  g.insert(4, 9, TP(1002, 1));
  // g.dump();
  
  g.insert(3, 13, TP(1003, 1));
  // g.dump();

  Set::dumpAll();

  g.insert(7, 9, TP(1004, 1));
  // g.dump();
  
  g.insert(1, 2, TP(1005, 1));
  // g.dump();
  
  g.insert(0, 1, TP(1006, 1));
  // g.dump();
  
  g.insert(14, 16, TP(1007, 1));
  // g.dump();
  
  g.insert(14, 18, TP(1008, 1));
  // g.dump();
  
  g.insert(12, 19, TP(1009, 1));
  g.dump();

  Set::dumpAll();
  g.validateSetToNode();

  //-------------------------------------------------------------

  g.erase(5, 10, TP(1000, 1));
  g.dump();
  Set::dumpAll();
  g.validateSetToNode();

  g.erase(14, 16, TP(1007, 1));
  g.dump();
  Set::dumpAll();
  g.validateSetToNode();
  
  g.erase(3, 13, TP(1003, 1));
  g.dump();
  Set::dumpAll();
  g.validateSetToNode();
  
  g.erase(1, 2, TP(1005, 1));
  g.dump();
  Set::dumpAll();
  g.validateSetToNode();

  g.erase(7, 12, TP(1001, 1));
  g.dump();
  Set::dumpAll();
  g.validateSetToNode();
  
  g.erase(4, 9, TP(1002, 1));
  g.dump();
  Set::dumpAll();
  g.validateSetToNode();

  g.erase(7, 9, TP(1004, 1));
  g.dump();
  Set::dumpAll();
  g.validateSetToNode();
  
  g.erase(12, 19, TP(1009, 1));
  g.dump();
  Set::dumpAll();
  g.validateSetToNode();

  g.erase(0, 1, TP(1006, 1));
  g.dump();
  Set::dumpAll();
  g.validateSetToNode();
  
  g.erase(14, 18, TP(1008, 1));
  g.dump();
  Set::dumpAll();
  g.validateSetToNode();

  Set::dumpAll();
  
};
