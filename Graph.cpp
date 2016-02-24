#include "find_tags_common.hpp"
#include "Graph.hpp"
#include <cmath>

Graph::Graph(std::string vizPrefix) : 
  vizPrefix(vizPrefix), 
  numViz(0), 
  setToNode(100),
  stamp(1)
{
  _root = new Node();
  mapSet(Set::empty(), Node::empty());
  mapSet(0, _root);
};

void 
Graph::newStamp() {
  if (! ++stamp) {
    resetAllStamps();
    stamp = 1;
  }
};

void 
Graph::resetAllStamps() {
  for (auto i = setToNode.begin(); i != setToNode.end(); ++i)
    i->second->stamp = 0;
};  

Node * 
Graph::root() {
  return _root;
};
    
void 
Graph::addTag(Tag *tag, double tol, double timeFuzz, double maxTime) {
  // add the repeated sequence of gaps from a tag, with fractional
  // tolerance tol, starting at phase 0, until adding the next gap
  // would exceed a total elapsed time of maxTime.  The gap is set
  // to an interval given by gap * (1-tol), gap * (1 + tol)

  // NB: gap 0 takes the tag from phase 0 to phase 1, etc.

  int n = tag->gaps.size();
  insert(TagPhase(tag, 0));
  // add two cycles of tag.  After the first cycle, the tag should be unique
  // and so we'll add new edges for skipped bursts
  Gap g;
  for(int i = 0; i < 2 * n - 1; ++i) {
    g = tag->gaps[i % n];
    GapRange gr(std::min(g - tol, g * (1 - timeFuzz)), std::max(g + tol, g * (1 + timeFuzz)));
    GapRanges grs;
    grs.push_back(gr);
    insertRec(grs, TagPhase(tag, i), TagPhase(tag, i+1));
#ifdef DEBUG
    validateSetToNode();
#endif
  };

  // We want to add two kinds of edges:
  // - "back" edges from the node at phase 2 * n - 1 to the node at phase n
  // corresponding to repetition of bursts.  There should be an edge for gap[n-1]
  // plus multiples of the period.
  // - "skip" edges from the node at phase n - 1 to the node at phase n corresponding
  // to missed bursts
  
  // back edges 
  GapRanges grs;
  for(g = tag->gaps[n - 1]; g < maxTime; g += tag->period)
    grs.push_back(GapRange(std::min(g - tol, g * (1 - timeFuzz)), std::max(g + tol, g * (1 + timeFuzz))));

  insertRec(grs, TagPhase(tag, 2 * n - 1), TagPhase(tag, n));

  // skip edges
  grs.clear();
  for(g = tag->gaps[n - 1] + tag->period; g < maxTime; g += tag->period)
    grs.push_back(GapRange(std::min(g - tol, g * (1 - timeFuzz)), std::max(g + tol, g * (1 + timeFuzz))));
  insertRec(grs, TagPhase(tag, n - 1), TagPhase(tag, n));
  
};    

void 
Graph::delTag(Tag *tag, double tol, double timeFuzz, double maxTime) {
  // remove the tag which was added with given gaps, tol, and maxTime

  int n = tag->gaps.size();
  Gap g;

  // remove skip edges
  GapRanges grs;
  for(g = tag->gaps[n - 1] + tag->period; g < maxTime; g += tag->period)
    grs.push_back(GapRange(std::min(g - tol, g * (1 - timeFuzz)), std::max(g + tol, g * (1 + timeFuzz))));
  eraseRec(grs, TagPhase(tag, n - 1), TagPhase(tag, n));

  // remove back edges 
  grs.clear();
  for(g = tag->gaps[n - 1]; g < maxTime; g += tag->period)
    grs.push_back(GapRange(std::min(g - tol, g * (1 - timeFuzz)), std::max(g + tol, g * (1 + timeFuzz))));
  eraseRec(grs, TagPhase(tag, 2 * n - 1), TagPhase(tag, n));

  for(int i = 2 * n - 2; i >= 0; --i) {
    g = tag->gaps[i % n];
    grs.clear();
    grs.push_back(GapRange(std::min(g - tol, g * (1 - timeFuzz)), std::max(g + tol, g * (1 + timeFuzz))));
    eraseRec(grs, TagPhase(tag, i), TagPhase(tag, i + 1));
#ifdef DEBUG
    validateSetToNode();
#endif
  };
  
  erase(TagPhase(tag, 0));
#ifdef DEBUG
  validateSetToNode();
  viz();
#endif
};

void
Graph::viz() {
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
  
void
Graph::dumpSetToNode() {
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

void 
Graph::validateSetToNode() {
  // verify that each set maps to a node that has it as a set!
  bool okay = true;
  for (auto i = setToNode.begin(); i != setToNode.end(); ++i) {
    if (! i->second) {
      std::cout << "Null Node Ptr for set:" << std::endl;
      i->first->dump();
      okay = false;
    } else if (! i->first) {
      if (i->second != _root) {
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
  
void
Graph::mapSet( Set * s, Node * n) {
  auto p = std::make_pair(s, n);
  setToNode.insert(p);
};

void
Graph::unmapSet ( Set * s) {
  if (s != Set::empty())
    setToNode.erase(s);
};

void
Graph::insert (const TagPhase &t) {
    _root->s = _root->s->augment(t);
    // note: we don't remap root in setToNode, since we
    // never try to lookup the root node from its set.
};

void 
Graph::erase (const TagPhase &t) {
    _root->s = _root->s->reduce(t);
    // note: we don't remap root in setToNode
};

void 
Graph::insert (GapRanges & grs, TagPhase p) {
  insert (_root, grs, p);
};

void 
Graph::erase (GapRanges & grs, TagPhase p) {
  erase(_root, grs, p);
};

bool 
Graph::hasEdge ( Node *n, Gap b, TagPhase p) {
  // check whether there is already an edge from
  // node n at point b to a set with tagPhase p
  auto i = n->e.lower_bound(b);
  if (i->first != b)
    return false;
  if (i->second->s->count(p))
    return true;
  return false;
};

Node::Edges::iterator 
Graph::ensureEdge ( Node *n, Gap b) {
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

void 
Graph::unlinkNode (Node *n) {
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

void
Graph::augmentEdge(Node::Edges::iterator i, TagPhase p) {
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


void
Graph::reduceEdge(Node::Edges::iterator i, TagPhase p) {
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


void
Graph::dropEdgeIfExtra(Node * n, Node::Edges::iterator i) 
{
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

void
Graph::insert (Node *n, GapRanges & grs, TagPhase p)
{
  for (auto gr = grs.begin(); gr != grs.end(); ++gr) {
    Gap lo = gr->first;
    Gap hi = gr->second;
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
  }
};

void 
Graph::insertRec (GapRanges & grs, TagPhase tFrom, TagPhase tTo) {
  newStamp();
  insertRec (_root, grs, tFrom, tTo);
};
    
void 
Graph::insertRec (Node *n, GapRanges & grs, TagPhase tFrom, TagPhase tTo) {
  // recursively insert a transition from tFrom to tTo because this
  // is a DAG, rather than a tree, a given node might already have
  // been visited by depth-first search, so we don't continue the
  // recursion of the given node already has the transition
  auto id = tFrom.first;
  n->stamp = stamp;
  for(auto i = n->e.begin(); i != n->e.end(); ) {
    auto j = i;
    ++j;
    if (i->second->stamp != stamp && i->second->s->count(id)) {
      insertRec(i->second, grs, tFrom, tTo);
    }
    i = j;
  }
  // possibly add edge from this node
  if (n->s->count(tFrom))
    insert(n, grs, tTo);
};

void 
Graph::erase (Node *n, GapRanges & grs, TagPhase tp) {
  // for any edges from n at points in the range [lo, hi]
  // remove p from the tail node.
  // If edges at lo and/or hi become redundant after 
  // this, remove them.

  for (auto gr = grs.begin(); gr != grs.end(); ++gr) {
    Gap lo = gr->first;
    Gap hi = gr->second;
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
  }
};

void 
Graph::eraseRec (GapRanges & grs, TagPhase tpFrom, TagPhase tpTo) {
  newStamp();
  eraseRec (_root, grs, tpFrom, tpTo);
};

void 
Graph::eraseRec (Node *n, GapRanges & grs, TagPhase tpFrom, TagPhase tpTo) {
  // recursively erase all transitions between tpFrom and tpTo for
  // the range lo, hi
  n->stamp = stamp;
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
      if (i->second->stamp != stamp)
        eraseRec(i->second, grs, tpFrom, tpTo);
    }
    i = j;
  }
  if (usedHere)
    erase(n, grs, tpTo);
};
