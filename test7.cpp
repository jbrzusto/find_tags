#include <set>
#include <map>
#include <iostream>
#include <fstream>
#include <stdexcept>
#include <cmath>
#include <memory>
#include <vector>
#include <unordered_map>
#include <string>
#include <random>

#include "Tag_Database.hpp"

class Node;
typedef double Point;
typedef int TagID;
typedef short Phase;
typedef std::pair < TagID, Phase > TagPhase;
typedef std::unordered_multimap < TagID, Phase > TagPhaseSet;


template<class CharType, class CharTraits>
std::basic_ostream<CharType, CharTraits> &operator<<
(std::basic_ostream<CharType, CharTraits> &stream, TagPhase const& value)
{
  if (value.second >= 0)
    return stream << "# " << value.first << " (" << value.second << ") ";
  return stream;
};

template<class CharType, class CharTraits>
std::basic_ostream<CharType, CharTraits> &operator<<
(std::basic_ostream<CharType, CharTraits> &stream, TagPhaseSet const& value)
{
  for (auto i = value.begin(); i != value.end(); ++i)
    stream << "\\n" << (*i);
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
  static int maxLabel;
  static Set * _empty;
  static std::set < Set * > allSets;

public:
  static Set * empty() { return _empty;};
  int label() { return _label;};

  ~Set() {
    allSets.erase(this);
    --numSets;
  };

  static int getNumSets() {
    return numSets;
  };
  
  Set() : s(), _label(maxLabel++) , hash(0) {
    allSets.insert(this);
    ++numSets;
#ifdef DEBUG
    std::cout << "Called Set()\n";
#endif
  };

  Set(TagPhase p) : s(), _label(maxLabel++), hash(hashTP(p)) {
    s.insert(p);
    allSets.insert(this); 
    ++numSets;
#ifdef DEBUG
    std::cout << "Called Set(p) with p = " << p << std::endl;
#endif
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

  int count(TagID id) {
    return s.count(id);
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
    ++numSets;
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
    return  x ? x->hash : 0;
  };
};

int Set::numSets = 0;
int Set::maxLabel = 0;

class Node {
  friend class Graph;
  typedef std::map < double, Node * > Edges;
  Set * s;
  Edges e;
  int useCount;

  int label;
  static int numNodes;
  static int maxLabel;

  static Node * _empty;

  void link() {
    ++ useCount;
#ifdef DEBUG
    std::cout << "UC for Node " << label << " = " << useCount << std::endl;
#endif
  };

  bool unlink() {
    return -- useCount == 0;
  };

  void drop() {
    if (s != Set::empty())
      delete s;
    --numNodes;
    delete this;
  };
      
protected:

  void _init() {
    useCount = 0;
    label = maxLabel++;
    ++numNodes;
    if (_empty) {
      e.insert(std::make_pair(-1.0 / 0.0, _empty));
      e.insert(std::make_pair( 1.0 / 0.0, _empty));
    }
  }

public:

  static void init() {
    Set::init();
    _empty = new Node();
    ++numNodes;
  };

  static Node * empty() {
    return _empty;
  };
  
  Node() : s(Set::empty()), e() {
    _init();
  };

  Node(const Node *n) : s(n->s), e(n->e), useCount(0) {
    _init();
#ifdef DEBUG
    std::cout << "Called Node * copy ctor with " << n->label << std::endl;
#endif
    for (auto i = e.begin(); i != e.end(); ++i)
      i->second->link();
  };

  static int getNumNodes() {
    return numNodes;
  };

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

};
int Node::numNodes = 0;
int Node::maxLabel = 0;
Node * Node::_empty = 0;

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


class Graph {
  Node * root;
  std::string vizPrefix;
  int numViz;

public:

  Node * advance (Node *n, Point dt);
    
  Graph(std::string vizPrefix) : vizPrefix(vizPrefix), numViz(0), setToNode(100) {
    root = new Node();
    mapSet(Set::empty(), Node::empty());
    mapSet(0, root);
  };

  void dump () {
    root->dump();
  };

  // map from sets to nodes
  std::unordered_map < Set *, Node *, hashSet, SetEqual > setToNode; 

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

  void insert (const TagPhase &t) {
    root->s = root->s->augment(t);
    // note: we don't remap root in setToNode, since we
    // never try to lookup the root node from its set.
  };

  void erase (const TagPhase &t) {
    root->s = root->s->reduce(t);
    // note: we don't remap root in setToNode
  };

  void insert (Point lo, Point hi, TagPhase p) {
    insert (root, lo, hi, p);
  };

  void erase (Point lo, Point hi, TagPhase p) {
    erase(root, lo, hi, p);
  };

  bool hasEdge ( Node *n, Point b, TagPhase p) {
    // check whether there is already an edge from
    // node n at point b to a set with tagPhase p
    auto i = n->e.lower_bound(b);
    if (i->first != b)
      return false;
    if (i->second->s->count(p))
      return true;
    return false;
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
#ifdef DEBUG
      std::cout << "Unlink Node(" << n->label << ") for set " << n->s->s << std::endl;
#endif
      unmapSet(n->s);
      // unlink tail nodes
      for (auto i = n->e.begin(); i != n->e.end(); ) {
        auto j = i;
        ++j;
        if (i->second != Node::empty())
          unlinkNode(i->second);
        i = j;
      }
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

  void insert (Node *n, Point lo, Point hi, TagPhase p)
  {
    // From the node at n, add appropriate edges to other nodes
    // given that the segment [lo, hi] is being augmented by p.

    ensureEdge(n, hi);  // ensure there is an edge from n at hi to the
                        // current node for that point

    auto i = ensureEdge(n, lo); // ensure there is an edge from n at
                                // lo to the current node for that
                                // point

    // From the lo to the last existing endpoint in [lo, hi), augment
    // each edge by p.

    while (i->first < hi) {
      auto j = i;
      ++j;
      augmentEdge(i, p);
      i = j;
    }
  };
    
  void insertRec (Node *n, Point lo, Point hi, TagPhase tFrom, TagPhase tTo) {
    // recursively insert a transition from tFrom to tTo because this
    // is a DAG, rather than a tree, a given node might already have
    // been visited by depth-first search, so we don't continue the
    // recursion of the given node already has the transition
    auto id = tFrom.first;

    if (hasEdge(n, lo, tTo))
      return;
    for(auto i = n->e.begin(); i != n->e.end(); ) {
      auto j = i;
      ++j;
      if (i->second->s->count(id)) {
        insertRec(i->second, lo, hi, tFrom, tTo);
      }
      i = j;
    }
      // possibly add edge from this node
    if (n->s->count(tFrom))
      insert(n, lo, hi, tTo);
  };

  void erase (Node *n, Point lo, Point hi, TagPhase tp) {
    // for any edges from n at points in the range [lo, hi]
    // remove p from the tail node.
    // If edges at lo and/or hi become redundant after 
    // this, remove them.

    auto i = n->e.lower_bound(lo);
    auto j = i; // save value for later
    while (i->first <= hi) {
      auto j = i;
      ++j;
      reduceEdge(i, tp);
      i = j;
    }
    --i;
    if (std::isfinite(i->first))
      dropEdgeIfExtra(n, i); // rightmost edge
    if (i != j && std::isfinite(j->first))
      dropEdgeIfExtra(n, j);  // leftmost edge
  };

  void eraseRec (Node *n, Point lo, Point hi, TagPhase tpFrom, TagPhase tpTo) {
    // recursively erase all transitions between tpFrom and tpTo for
    // the range lo, hi
    auto id = tpFrom.first;
    bool here = n->s->count(tpFrom) > 0;
    bool usedHere = false;
    for(auto i = n->e.begin(); i != n->e.end(); ) {
      auto j = i;
      ++j;
      if (i->second->s->count(id)) {
        // recurse to any child node that has this tag ID in
        // its set, because we might eventually reach a node with tpFrom
        if (here && i->second->s->count(tpTo))
          usedHere = true;
        eraseRec(i->second, lo, hi, tpFrom, tpTo);
      }
      i = j;
    }
    if (usedHere)
      erase(n, lo, hi, tpTo);
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
    bool okay = true;
    for (auto i = setToNode.begin(); i != setToNode.end(); ++i) {
      if (! i->second) {
        std::cout << "Null Node Ptr for set:" << std::endl;
        i->first->dump();
        okay = false;
      } else if (! i->first) {
        if (i->second != root) {
          std::cout << "Non-root Node mapped by null set *: " << std::endl;
          i->second->dump();
          okay = false;
        }
      } else if (i->second->s != i->first) {
        std::cout << "Invalid setToNode map:" << std::endl;
        dumpSetToNode();
        okay = false;
      }
    }
    if (!okay)
      throw std::runtime_error("validateSetToNode() failed");
  };

  void  viz() {
    // dump graph in the "dot" format for the graphviz package
    // These .gv files can be converted to nice svg plots like so:
    // for x in *.gv; do dot -Tsvg $x > ${x/gv/svg}; done

    std::ofstream out(vizPrefix + std::to_string(++numViz) + ".gv");

    out << "digraph TEST {\n";

    // generate node symbols and labels
    for (auto i = setToNode.begin(); i != setToNode.end(); ++i) {
      // define the node, labelled with its TagPhase set
      out << "a" << i->second->label <<
        "[label=\"a" << i->second->label << i->second->s->s << "\"];\n";

      // define each edge, labelled by its interval
      for(auto j = i->second->e.begin(); j != i->second->e.end(); ++j) {
        Set * m = j->second->s;
        if (m != Set::empty()) {
          auto n = setToNode.find(m);
          if (n != setToNode.end()) {
            auto k = j;
            ++k;
            out << "a" << i->second->label << " -> a" << (n->second->label) << "[label = \"["
                << j->first << "," << k->first << "]\"];\n";
          }
        }
      }
    }
    out << "}\n";
  };

  void addTag(TagID id, std::vector < double > gaps, double tol, double timeFuzz, double maxTime) {
    // add the repeated sequence of gaps from a tag, with fractional
    // tolerance tol, starting at phase 0, until adding the next gap
    // would exceed a total elapsed time of maxTime.  The gap is set
    // to an interval given by gap * (1-tol), gap * (1 + tol)

    // NB: gap 0 takes the tag from phase 0 to phase 1, etc.
    int n = gaps.size();
    int p = 0;
    double t = 0;
    TagPhase last(id, p);
    insert(last);
    for(;;) {
      double g = gaps[p % n];
      t += g;
      if (t > maxTime)
        break;
      ++p;
      TagPhase next(id, p);
      insertRec(root, std::min(g - tol, g * (1 - timeFuzz)), std::max(g + tol, g * (1 + timeFuzz)), last, next);
#ifdef DEBUG
      validateSetToNode();
#endif
      last = next;
    };
  };    

  void delTag(TagID id, std::vector < double > gaps, double tol, double timeFuzz, double maxTime) {
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
      eraseRec(root, std::min(g - tol, g * (1 - timeFuzz)), std::max(g + tol, g * (1 + timeFuzz)), TagPhase(id, p), TagPhase(id, p+1));
#ifdef DEBUG
      validateSetToNode();
#endif
    }
    erase(TagPhase(id, 0));
#ifdef DEBUG
    validateSetToNode();
    viz();
#endif
  };
};

#include <stdlib.h>

main (int argc, char * argv[] ) {
  Node::init();
  Graph g("test");

  int n = 0;

  int maxnt = -1;
  if (argc > 1)
    maxnt = atoi(argv[1]);

  g.insert(5, 10, TagPhase(1000, 1));
  // g.dump();
  //  Set::dumpAll();

  g.insert(7, 12, TagPhase(1001, 1));
  // g.dump();
  
  g.insert(4, 9, TagPhase(1002, 1));
  // g.dump();
  
  g.insert(3, 13, TagPhase(1003, 1));
  // g.dump();

  //  Set::dumpAll();

  g.insert(7, 9, TagPhase(1004, 1));
  // g.dump();
  
  g.insert(1, 2, TagPhase(1005, 1));
  // g.dump();
  
  g.insert(0, 1, TagPhase(1006, 1));
  // g.dump();
  
  g.insert(14, 16, TagPhase(1007, 1));
  // g.dump();
  
  g.insert(14, 18, TagPhase(1008, 1));
  // g.dump();
  
  g.insert(12, 19, TagPhase(1009, 1));
  g.dump();

  Set::dumpAll();
  g.validateSetToNode();


  g.viz();

  // New tag with ID 4 and gaps 6.7, 4.1, 4.3, BI=110

  std::vector < double > T2;
  T2.push_back(4.1);  
  T2.push_back(4.3);
  T2.push_back(6.7);
  T2.push_back(110);

  double tol = 0.25;
  double timeFuzz = 0.05;

  g.addTag(4, T2, tol, timeFuzz, 500);

  g.viz();

  //-------------------------------------------------------------

  g.erase(5, 10, TagPhase(1000, 1));
  g.viz();
    g.validateSetToNode();

  g.erase(14, 16, TagPhase(1007, 1));
  g.viz();
    g.validateSetToNode();
  
  g.erase(3, 13, TagPhase(1003, 1));
  g.viz();
    g.validateSetToNode();
  
  g.erase(1, 2, TagPhase(1005, 1));
  g.viz();
    g.validateSetToNode();

  g.erase(7, 12, TagPhase(1001, 1));
  g.viz();
    g.validateSetToNode();
  
  g.erase(4, 9, TagPhase(1002, 1));
  g.viz();
    g.validateSetToNode();

  g.erase(7, 9, TagPhase(1004, 1));
  g.viz();
    g.validateSetToNode();
  
  g.erase(12, 19, TagPhase(1009, 1));
  g.viz();
    g.validateSetToNode();

  g.erase(0, 1, TagPhase(1006, 1));
  g.viz();
    g.validateSetToNode();
  
  g.erase(14, 18, TagPhase(1008, 1));
  g.viz();
  g.validateSetToNode();

  g.delTag(4, T2, tol, timeFuzz, 500);

  g.viz();

  g.validateSetToNode();

  Tag_Database T("/sgm/2015_normalized_full_motus_database.sqlite");

  auto ts = T.get_tags_at_freq(Nominal_Frequency_kHz(166380));

  int nt = ts->size();

  std::cout << "Got " << nt << " tags\n";

  // FIXME: just use the first 10 tags
  if (maxnt > 0)
    nt = maxnt;

  std::vector < Known_Tag > tags (nt);
  int i = 0;
  for (auto j = ts->begin(); j != ts->end() && i < nt; ++j) {
#ifdef DEBUG
    std::cout << (*j)->motusID << std::endl;
#endif
    tags[i++] = **j;
  }

  for (i = 1; i < nt; ++i) {
    for (auto ii = 0; ii < i; ++ii) {
      if (tags[i].gaps[0] == tags[ii].gaps[0] 
          &&
          tags[i].gaps[1] == tags[ii].gaps[1] 
          &&
          tags[i].gaps[2] == tags[ii].gaps[2] 
          &&
          tags[i].gaps[3] == tags[ii].gaps[3]
          )
        std::cout << "Tag dups: " << i << "," << ii << std::endl;
    }
  }

  std::vector < bool > inTree(nt);

  std::random_device rd;     // only used once to initialise (seed) engine
  std::mt19937 rng(rd());    // random-number engine used (Mersenne-Twister in this case)
  std::uniform_int_distribution<int> uni(0, nt - 1); // guaranteed unbiased
  
  tol = 0.0015;
  timeFuzz = 0;

  int numTags = 1;
  for(int numEvts = 0; /**/ ; ++ numEvts) {
    auto r = uni(rng);
    std::vector < double > T;
    for (i = 0; i < 4; ++i)
      T.push_back(tags[r].gaps[i]);
    if (inTree[r]) {
#ifdef DEBUG      
      std::cout << "-" << tags[r].motusID << std::endl;
#endif
      g.delTag(tags[r].motusID, T, tol, timeFuzz, 60);
      inTree[r] = false;
      --numTags;
    } else {
#ifdef DEBUG
      std::cout << "+" << tags[r].motusID << std::endl;
#endif
      g.addTag(tags[r].motusID, T, tol, timeFuzz, 60);
      inTree[r] = true;
      ++numTags;
    };
#ifdef DEBUG
    g.validateSetToNode();
    g.viz();
#endif
    if (numEvts % 100 == 0)
      std::cout << "After "  << numEvts << " events, # tags in tree is " << numTags << ", # Nodes = " << Node::getNumNodes() << ", # Sets = " << Set::getNumSets() << std::endl;
  }
  g.viz();
};
