#ifndef DFA_GRAPH_HPP
#define DFA_GRAPH_HPP

#include "find_tags_common.hpp"

#include "DFA_Node.hpp"

#include <vector>

class DFA_Graph {
  // the graph representing a DFA for the NDFA full-burst recognition
  // problem on a set of known tags

public:

  typedef std::map < Tag_ID_Set, DFA_Node * > Node_For_IDs;
  

private:
  // the root node, where any DFA begins its life

  DFA_Node * root;

  // For each phase, a map from sets of tag IDs to DFA_Node
  // this is only used in constructing the tree, but we don't
  // bother deleting.

  std::vector < Node_For_IDs > node_for_set_at_phase;

public:

  DFA_Graph();

  void set_all_ids (Tag_ID_Set &s);

  DFA_Node *get_root();

  // iterators for nodes at each phase

  Node_For_IDs :: const_iterator begin_nodes_at_phase (unsigned int phase);

  Node_For_IDs :: const_iterator end_nodes_at_phase (unsigned int phase);

  // grow the DFA_Graph from a node via an interval_map; edges are added
  // between the specified node and (possibly new nodes) at the specified
  // phase.  "Edges" are really (interval < Gap > , Node*), collected
  // in an interval_map.

  void grow(DFA_Node *p, interval_map < Gap, Tag_ID_Set > &s, unsigned int phase);

  std::vector < Node_For_IDs > & get_node_for_set_at_phase();
};

#endif // DFA_GRAPH_HPP
