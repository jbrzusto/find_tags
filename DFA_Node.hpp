#ifndef DFA_NODE_HPP
#define DFA_NODE_HPP

#include "find_tags_common.hpp"

#include <boost/icl/interval_map.hpp>
using namespace boost::icl;

class DFA_Node {

  friend class DFA_Graph;

private:
  typedef interval_map < Gap, DFA_Node * > Edges;
  typedef interval_map < Gap, DFA_Node * > :: iterator Edge_iterator;
  typedef interval_map < Gap, DFA_Node * > :: const_iterator Const_Edge_iterator;

  // fundamental structure

  unsigned int	phase;		// number of gaps seen before this node
  Tag_ID_Set	ids;		// ids of tags in this node
  Edges		edges;		// interval map from gap to other nodes
  Gap	        max_age;	// maximum time a DFA can remain in
                                // this state.  If a DFA in this state
                                // goes longer than this without
                                // adding a pulse, then it is
                                // guaranteed to NOT recognize a valid
                                // burst, and so is killed.
public:  
  DFA_Node(unsigned int phase);

  DFA_Node(unsigned int phase, const Tag_ID_Set &ids);

  DFA_Node * next (Gap gap);

  bool is_unique();

  void set_max_age();

  Gap get_max_age();

  unsigned int get_phase();

  Tag_ID get_id();

  void dump(ostream & os, string indent = "", string indent_change = "   ");
    
};

#endif // DFA_NODE_HPP

