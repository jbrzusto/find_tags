#include "DFA_Graph.hpp"

DFA_Graph::DFA_Graph() :
  root(new DFA_Node(0)),

  // NB: preallocate the vector of maps so that iterators to particular
  // maps are not invalidated by reallocation of the vector as it grows.

  node_for_set_at_phase(2 * PULSES_PER_BURST) 
{};

void DFA_Graph::set_all_ids (Tag_ID_Set &s) {
  // initialize the root node with the set of all Tag IDs
  // Root is the only node at phase 0

  node_for_set_at_phase[0][s] = root;
  root->ids = s;
};

DFA_Node *DFA_Graph::get_root() {
  return root;
};

// iterators for nodes at each phase

DFA_Graph::Node_For_IDs :: const_iterator DFA_Graph::begin_nodes_at_phase (unsigned int phase) {
  return node_for_set_at_phase[phase].begin();
};

DFA_Graph::Node_For_IDs :: const_iterator DFA_Graph::end_nodes_at_phase (unsigned int phase) {
  return node_for_set_at_phase[phase].end();
};

// grow the DFA_Graph from a node via an interval_map; edges are added
// between the specified node and (possibly new nodes) at the specified
// phase.  "Edges" are really (interval < Gap > , Node*), collected
// in an interval_map.

void DFA_Graph::grow(DFA_Node *p, interval_map < Gap, Tag_ID_Set > &s, unsigned int phase) {

  if (! root)
    root = new DFA_Node(0);

  if (!p)
    p = root;

  if (phase >= node_for_set_at_phase.size())
    node_for_set_at_phase.push_back(Node_For_IDs());

  Node_For_IDs &node_for_set = node_for_set_at_phase[phase];

  // for each unique Tag_ID_Set in the interval_map,
  // create a new node and link to it via the edges interval_map
    
  for (interval_map < Gap, Tag_ID_Set > :: const_iterator it = s.begin(); it != s.end(); ++it) {
    if (! node_for_set.count(it->second))
      node_for_set[it->second] = new DFA_Node(p->phase + 1, it->second);
    p->edges.set(make_pair(it->first, node_for_set[it->second]));
  }

  // ensure the node's max age is correct, given we may have added
  // new edges

  p->set_max_age();
};

std::vector < DFA_Graph:: Node_For_IDs > & DFA_Graph::get_node_for_set_at_phase() {
  return node_for_set_at_phase;
};
