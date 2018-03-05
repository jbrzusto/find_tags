// FIXME: when doing delTag, need to eliminate candidates for that tag, as their nodePtr
// is invalid.

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
  //  _root->link();
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

std::pair < Tag *, Tag * >
Graph::addTag(Tag * tag, double tol, double timeFuzz, double maxTime, unsigned int timestamp_wonkiness) {
  auto ot = find(tag, tol, timeFuzz);

  // if we renamed a tag in the graph due to ambiguity management,
  // we return this pair to the caller.  Else, we return (0, 0);

  if (! ot) {
    // tag not already present, so just add
    _addTag(tag, tol, timeFuzz, maxTime, timestamp_wonkiness);
    return std::make_pair((Tag *) 0, (Tag *) 0);
  }
  // another tag is ambiguous with this one (i.e. pulses from this one
  // would be detected as an other, existing tag)
  // Manage the ambiguity by replacing the existing tag with a proxy
  // that represents it (possibly already a proxy) and the new tag.
  auto nt = Ambiguity::add(ot, tag);
  renTag(ot, nt);
  return std::make_pair(ot, nt);
};

std::pair < Tag *, Tag * >
Graph::delTag(Tag * tag) {
  auto p = Ambiguity::proxyFor(tag);
  if (!p) {
    // tag has not been proxied, so just delete
    _delTag(tag);
    return std::make_pair((Tag *) 0, (Tag *) 0);
  }
  // this tag is part of an ambiguity set which has been proxied in the tree
  // remove this tag from the group, remove the original proxy from the tree,
  // and replace it with either a new (reduced) proxy, or a real tag if removing
  // this tag leaves only one other tag in the ambiguity set.
  auto newp = Ambiguity::remove(p, tag);
  renTag(p, newp);
  return std::make_pair(p, newp);
};

void
Graph::_addTag(Tag *tag, double tol, double timeFuzz, double maxTime, unsigned int timestamp_wonkiness) {
  // add the repeated sequence of gaps from a tag, with fractional
  // tolerance tol, starting at phase 0, until adding the next gap
  // would exceed a total elapsed time of maxTime.  The gap is set
  // to an interval given by gap * (1-tol), gap * (1 + tol)

  // NB: gap 0 takes the tag from phase 0 to phase 1, etc.

  // The sequence of n gaps from a tag should be unique.  When we reach
  // a node with tag T in phase n(T), all paths leading from there only
  // reach nodes whose tag set is the singleton T, in some phase.

  // To achieve this, we build the tree out to phase n, such that nodes
  // with phase < n may have other tags in their sets.  After the node at
  // phase n for T, we build nodes for each phase out to 2 * n - 1,
  // and then link back to node n. So once a sequence of pulse gaps has
  // led to a unique tag, subsequent gaps are only accepted if they
  // are compatible with that tag.

  // TODO:
  // new timing model (ms / s), based on pulse+slop = 1.5, burst_slop = 4, burst_slop_expansion = 1
  // and bi=10:

  // (%i71) plot2d(s(x, 0.5, 100) * (4 - 1.5) + 1.5 + s(x, 10, 0.1) * 0.1 * (x-5), [x, 0, 50],[y,0,10]);
  // (%o71) /home/john/maxout.gnuplot_pipes
  // (%i73) s(x,r,a);
  //                                        1
  // (%o73)                         -----------------
  //                                  - a (x - r)
  //                                %e            + 1
  //
  // Should be similar to existing model, but with a second plateau for intermediate-valued intervals.
  // (i.e. 1 to 10 seconds).

  int n = tag->gaps.size();
  insert(TagPhase(tag, 0));
  // Add a single cycle of gaps for the tag.
  Gap g;
  int i;
  for(i = 0; i < 2 * n - 1; ++i ) {
    g = tag->gaps[i % n];
    // Given a clock mismatch of timeFuzz, we consider two measurements
    // of time to be equal when:
    //
    //   t1/t2 <= (1 + timeFuzz) and t2/t1 <= (1 + timeFuzz)
    //
    // where timeFuzz is on the order of a few parts per million (i.e. ~ 10E-6)
    // Then   t2 / (1 + timeFuzz) <= t1 <= t2 * (1 + timeFuzz)
    // but  1/(1 + timeFuzz) = 1 - timeFuzz + timeFuzz^2 -/+ ...
    // and because timeFuzz << 1, the RHS is ~ 1 - timeFuzz.
    // So the condition is t2 * (1 - timeFuzz) <= t1 <= t2 * (1 + timeFuzz)
    // except that we force the allowed interval to have width at least 2 * tol

    Gap_Range gr(g, tol, timeFuzz);
    Gap_Ranges grs;
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
  // to missed bursts immediately after the first (only if n > 1; a beeper tag has n = 1)

  // back edges
  Gap_Ranges grs;
  for(g = tag->gaps[n - 1]; g < maxTime; g += tag->period)
    grs.push_back(Gap_Range(g, tol, timeFuzz));

  insertRec(grs, TagPhase(tag, 2 * n - 1), TagPhase(tag, n)); // self-linked edges for a beeper tag: 2 * 1 - 1 == 1

  // skip edges (only for n > 1).  FIXME:  We are privileging the last gap because
  // for coded ID tags, it is typically much larger than the others, so that the pulses
  // forming the first n - 1 gaps are called a "burst".  A more comprehensive
  // approach, especially at low-noise, low-activity sites, would be to add edges for any number
  // of missed pulses, not just entire "bursts".

  if (n > 1) {
    grs.clear();
    for(g = tag->gaps[n - 1] + tag->period; g < maxTime; g += tag->period)
      grs.push_back(Gap_Range(g, tol, timeFuzz));
    insertRec(grs, TagPhase(tag, n - 1), TagPhase(tag, n));
  }

  if (timestamp_wonkiness > 0) {
    // timestamp wonkiness: to handle clock jumps of +/- 1s in data from Lotek .DTA files, we add extra nodes
    // and extra edges. See the file dfa_graph.pdf for an example.

    // there will be two new subgraphs linked to the existing graph:
    // G-, where the clock has jumped back by 1s; phases 2*n, 2*n+1, ..., 3*n-1
    // G+, where the clock has jumped forward by 1s; phases 3*n, 3*n+1, ..., 4*n-1

    Gap_Ranges grsPlus, grsMinus;
    for(g = tag->gaps[n - 1] + tag->period; g < maxTime; g += tag->period) {
      grsPlus.push_back(Gap_Range(g + 1, tol, timeFuzz));
      grsMinus.push_back(Gap_Range(g - 1, tol, timeFuzz));
    }

    // 1. edges/nodes for G-, the "clock jumped back by 1s" subgraph

    // a) long edges (clock jump possible)
    //  insertRec(grsMinus, TagPhase(tag, n - 1),     TagPhase(tag, 2 * n)); // to (after 1st burst)
    insertRec(grsMinus, TagPhase(tag, 2 * n - 1), TagPhase(tag, 2 * n)); // to (after later bursts)
    insertRec(grsPlus,  TagPhase(tag, 3 * n - 1), TagPhase(tag, n - 1)); // from
    insertRec(grs,      TagPhase(tag, 3 * n - 1), TagPhase(tag, 2 * n)); // within

    // b) short edges (no clock jump possible)

    for(i = 0; i < n - 1; ++i ) {
      Gap_Ranges grs2;
      grs2.push_back(grs[i]);
      insertRec(grs2, TagPhase(tag, 2 * n + i), TagPhase(tag, 2 * n + i + 1));
    }

    // 2. edges/nodes for G+, the "clock jumped forward by 1s" subgraph

    // a) long edges (clock jump possible)
    //  insertRec(grsPlus,  TagPhase(tag, n - 1),     TagPhase(tag, 3 * n)); // to (after 1st burst)
    insertRec(grsPlus,  TagPhase(tag, 2 * n - 1), TagPhase(tag, 3 * n)); // to (after later bursts)
    insertRec(grsMinus, TagPhase(tag, 4 * n - 1), TagPhase(tag, n - 1)); // from
    insertRec(grs,      TagPhase(tag, 4 * n - 1), TagPhase(tag, 3 * n)); // within

    // b) short edges (no clock jump possible)

    for(i = 0; i < n - 1; ++i ) {
      Gap_Ranges grs2;
      grs2.push_back(grs[i]);
      insertRec(grs2, TagPhase(tag, 3 * n + i), TagPhase(tag, 3 * n + i+1));
    }
  }
};

void
Graph::_delTag(Tag *tag) {
  // remove the tag

  eraseRec(tag);
#ifdef DEBUG
  validateSetToNode();
#endif

  erase_at_root(tag);
#ifdef DEBUG
  validateSetToNode();
#endif
};

Tag *
Graph::find(Tag * tag, double tol, double timeFuzz) {
  // FIXME: we're only looking for match of the
  // exact tag values; we really should be doing a tree search
  // for each node where the tag should be unique but isn't
  // (i.e. each node with TagPhase(tag, n) where n is the number
  // of pulses in n

  Node * n = _root;
  int ng = tag->gaps.size();
  int i;
  Gap g;
  for (i = 0; i < ng - 1; ++i) {
    g = tag->gaps[i];
    n = n->advance(g);
    if (! n)
      return 0;
  }
  // To work around at least one problem, we widen the search
  // at the last gap (the 'long' gap) to the bounds provided
  // by tol and timeFuzz, checking the endpoints as well as
  // the centre.

  Gap_Range gr(tag->gaps[i], tol, timeFuzz);
  std::vector < Gap > gg;
  gg.push_back(tag->gaps[i]);
  gg.push_back(gr.first);
  gg.push_back(gr.second);

  for (i = 0; i < gg.size(); ++i) {
    auto m = n->advance(gg[i]);
    if (m) {
      if (m->s->s.size() > 1)
        throw std::runtime_error("Graph::find: tag not unique");
      if (! (m->s->s.begin()->first-> active)) {
        std::cerr << "motusID = " << m->s->s.begin()->first->motusID << "=" << (void *) m->s->s.begin()->first << std::endl;
        throw std::runtime_error("Graph::find: tag not active");
      }
      return m->s->s.begin()->first;
    }
  }
  return 0;
};


void
Graph::viz() {
  // dump graph in the "dot" format for the graphviz package
  // These .gv files can be converted to nice svg plots like so:
  // for x in *.gv; do dot -Tsvg $x > ${x/gv/svg}; done

  std::ofstream out(vizPrefix + std::to_string(numViz++) + ".gv");

  out << "digraph TEST {\n";

  // generate node symbols and labels
  for (auto i = setToNode.begin(); i != setToNode.end(); ++i) {
    // define the node, labelled with its TagPhase set
    out << "a" << i->second->label <<
      "[label=\"a" << i->second->label;
    for (auto t = i->second->s->s.begin(); t != i->second->s->s.end(); ++t)
      out << "\\n" << t->first->motusID << ':' << t->second;
    out << "\"];\n";

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
Graph::erase_at_root (Tag * t) {
  _root->s = _root->s->reduce(t);
    // note: we don't remap root in setToNode
};

void
Graph::insert (Gap_Ranges & grs, TagPhase p) {
  insert (_root, grs, p);
};

void
Graph::erase (Tag *t) {
  erase(_root, t);
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
  linkNode(i->second);
  // return iterator to the new edge, which
  // will be immediately to the right of i
  return ++i;
};

void
Graph::linkNode (Node *n) {
  n->link();
};

void
Graph::unlinkNode (Node *n) {
  if (n->unlink() && n != Node::empty()) {
#ifdef DEBUG2
    std::cout << "Unlink Node(" << n->label << ") for set " << n->s->s << std::endl;
#endif
    unmapSet(n->s);
    // unlink tail nodes
    for (auto i = n->e.begin(); i != n->e.end(); ) {
      auto j = i;
      ++j;
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
    linkNode(i->second);
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
  linkNode(nn);
  i->second = nn;
};


void
Graph::reduceEdge(Node::Edges::iterator i, Tag * t) {
  // given an existing edge, reduce its tail node by t

  Node * n = i->second;
  if (n->s->count(t) == 0)
    return;
  Set * s = n->s->cloneReduce(t);
  auto j = setToNode.find(s);
  if (j != setToNode.end()) {
    // already have a node for this set
    if (s != Set::empty())
      delete s;
    i->second = j->second;
    linkNode(i->second);
    unlinkNode(n);
    return;
  }
  if (n->useCount == 1) {
    // special case to save work: re-use this node
    unmapSet(n->s);
    n->s = n->s->reduce(t);
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
  linkNode(nn);
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
Graph::insert (Node *n, Gap_Ranges & grs, TagPhase p)
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
Graph::insertRec (Gap_Ranges & grs, TagPhase tFrom, TagPhase tTo) {
  newStamp();
  insertRec (_root, grs, tFrom, tTo);
};

void
Graph::insertRec (Node *n, Gap_Ranges & grs, TagPhase tFrom, TagPhase tTo) {
  // recursively insert a transition from tFrom to tTo

  // Because this is a DAG, rather than a tree, a given node might
  // already have been visited by depth-first search, so we don't
  // continue the recursion if the given node already has the
  // transition.

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
Graph::renTag(Tag *t1, Tag *t2) {
  newStamp();
  renTagRec(_root, t1, t2);
};

void
Graph::renTagRec(Node * n, Tag *t1, Tag *t2) {
  // for any occurence of t1 in the tree, replace it with t2

  n->stamp = stamp;

  // check descendents first
  for(auto i = n->e.begin(); i != n->e.end(); ++i)
    if (i->second->stamp != stamp && i->second->s->count(t1))
      renTagRec(i->second, t1, t2);

  // for this node's set, replace any tagphase having t1
  // with a tagphase having t2

  TagPhaseSet & s = n->s->s;
  auto r = s.equal_range(t1);

  for (auto i = r.first; i != r.second; /**/) {
    auto j = i;
    ++j;
    auto p = i->second;
    s.erase(i);
    s.insert(std::make_pair(t2, p));
    i = j;
  }
};

void
Graph::erase (Node *n, Tag * t) {
  // remove t from the tail node of any edges
  // If edges at lo and/or hi become redundant after
  // this, remove them.

  for(auto i = n->e.begin(); i != n->e.end(); ) {
    auto j = i;
    ++j;
    if (i->second->s->count(t)) {
      reduceEdge(i, t);
    }
    i = j;
  }

  // Algorithm that only looks at edges in range
  // for (auto gr = grs.begin(); gr != grs.end(); ++gr) {
  //   Gap lo = gr->first;
  //   Gap hi = gr->second;
  //   auto i = n->e.lower_bound(lo);
  //   auto j = i; // save value for later
  //   while (i->first <= hi) {
  //     auto j = i;
  //     ++j;
  //     reduceEdge(i, tp);
  //     i = j;
  //   }
  //   --i;
  //   if (std::isfinite(i->first))
  //     dropEdgeIfExtra(n, i); // rightmost edge
  //   if (i != j && std::isfinite(j->first))
  //     dropEdgeIfExtra(n, j);  // leftmost edge
  // }
};

void
Graph::eraseRec (Tag *t) {
  newStamp();
  eraseRec (_root, t);
};

void
Graph::eraseRec (Node *n, Tag *t) {
  // recursively erase tag t
  n->stamp = stamp;
  bool here = n->s->count(t) > 0;
  for(auto i = n->e.begin(); i != n->e.end(); ) {
    auto j = i;
    ++j;
    if (i->second->stamp != stamp && i->second->s->count(t)) {
      // recurse to any child node that has this tag ID in
      // its set
        eraseRec(i->second, t);
    }
    i = j;
  }
  if (here)
    erase(n, t);
};

#ifdef DEBUG
static int findCount;

void
Graph::findTag(Tag *tag, bool expected) {
  newStamp();
  findCount = 0;
  findTagRec(_root, tag);
  if ((findCount > 0) ^ expected)
    std::cerr << "Tag " << tag->motusID << " found in " << findCount << " node sets.\n";
};

void
Graph::findTagRec(Node * n, Tag * tag)
{
    n->stamp = stamp;
    for(auto i = n->e.begin(); i != n->e.end(); ++i) {
      if (i->second->stamp != stamp) {
        findTagRec(i->second, tag);
      }
    }
    // see whether tag is in this node's set (at any phase)
    if (n->s->count(tag)) {
      ++findCount;
    }
};
#endif
