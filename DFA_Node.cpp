#include "DFA_Node.hpp"

DFA_Node::DFA_Node(unsigned int phase) :
  phase(phase),
  ids(),
  edges(),
  max_age(-1)
{};

DFA_Node::DFA_Node(unsigned int phase, const Tag_ID_Set &ids) :
  phase(phase),
  ids(ids),
  edges(),
  max_age(-1)
{};

DFA_Node * DFA_Node::next (Gap gap) {

  // return the DFA_Node obtained by following the edge labelled "gap",
  // or NULL if no such edge exists.  i.e. move to the state representing
  // those tag IDs which are compatible with the current set of pulses and with
  // the specified gap to the next pulse.

  Const_Edge_iterator it = edges.find(gap);
  if (it == edges.end())
    return 0;
  else
    return it->second;
};

bool DFA_Node::is_unique() {

  // does this DFA state represent a single Tag ID?

  return ids.size() == 1;
};

void DFA_Node::set_max_age() {

  // set the maximum time before death for a DFA in this state
  // This is the longest time we can wait for a pulse that will
  // still lead to a valid NDFA node.

  if (edges.size() > 0)
    max_age = upper(edges.rbegin()->first);
  else
    max_age = -1;
};

Gap DFA_Node::get_max_age() {
  return max_age;
};

unsigned int DFA_Node::get_phase() {
  return phase;
};

Tag_ID DFA_Node::get_id() {
  return *ids.begin();
};

void DFA_Node::dump(ostream & os, string indent, string indent_change) {
    
  // output the tree rooted at this node, appropriately indented,
  // to a stream

  os << indent <<  "NODE @ Phase " << phase << "; max age: " << max_age << "\n"
     << indent << ids << "\n" << indent << "Edges:" << "\n";
  for (Edge_iterator it = edges.begin(); it != edges.end(); ++it) {
    os << indent << it->first << "->";
    if (it->second->phase > phase) {
      os << endl;
      it->second->dump(os, indent + indent_change);
    } else {
      os << " Back to NODE @ Phase " << it->second->phase << ":" << it->second->ids << endl;
    }
  }
};
