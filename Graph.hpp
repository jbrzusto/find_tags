#ifndef GRAPH_HPP
#define GRAPH_HPP

#include "find_tags_common.hpp"
#include "Node.hpp"

class Graph {
  // the graph representing a DFA for the NDFA full-burst recognition
  // problem on a set of known tags
  
protected:
  
  Node * _root;
  std::string vizPrefix;
  int numViz;

  // map from sets to nodes
  std::unordered_map < Set *, Node *, hashSet, SetEqual > setToNode; 

  // stamp; each time a recursive algorithm is run, the stamp value is
  // first increased; as the graph is traversed, nodes are stamped with
  // the new value to indicate they have been visited.  Avoids having
  // to reset a "visited" flag across the graph at the start of each
  // run of the algorithm.  When the stamp value wraps back to 0, all
  // nodes get stamped with 0 and the new stamp value is set to 1.
  int stamp;

public:

  Graph(std::string vizPrefix = "graph");
  Node * root();
  void addTag(Tag * tag, double tol, double timeFuzz, double maxTime);    
  void delTag(Tag * tag, double tol, double timeFuzz, double maxTime);
  void viz();
  void dumpSetToNode();
  void validateSetToNode();

protected:

  void newStamp(); //!< update the stamp value; restamp all nodes when it wraps.

  void resetAllStamps();

  void mapSet( Set * s, Node * n);

  void unmapSet ( Set * s);

  void insert (const TagPhase &t);

  void erase (const TagPhase &t);

  void insert (Gap lo, Gap hi, TagPhase p);

  void erase (Gap lo, Gap hi, TagPhase p);

  bool hasEdge ( Node *n, Gap b, TagPhase p);

  Node::Edges::iterator ensureEdge ( Node *n, Gap b);

  void unlinkNode (Node *n); 

  void augmentEdge(Node::Edges::iterator i, TagPhase p);
  
  void reduceEdge(Node::Edges::iterator i, TagPhase p);

  void dropEdgeIfExtra(Node * n, Node::Edges::iterator i);

  void insert (Node *n, Gap lo, Gap hi, TagPhase p);
    
  void insertRec (Gap lo, Gap hi, TagPhase tFrom, TagPhase tTo);

  void insertRec (Node *n, Gap lo, Gap hi, TagPhase tFrom, TagPhase tTo);

  void erase (Node *n, Gap lo, Gap hi, TagPhase tp);

  void eraseRec (Gap lo, Gap hi, TagPhase tpFrom, TagPhase tpTo);

  void eraseRec (Node *n, Gap lo, Gap hi, TagPhase tpFrom, TagPhase tpTo);

};

#endif // GRAPH_HPP
