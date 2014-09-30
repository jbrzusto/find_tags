#include "Tag_Finder.hpp"

Tag_Finder::Tag_Finder (Tag_Foray * owner, Nominal_Frequency_kHz nom_freq, Tag_Set *tags, string prefix) :
  owner(owner),
  nom_freq(nom_freq),
  tags(tags),
  graph(),
  cands(NUM_CAND_LISTS),
  pulse_slop(default_pulse_slop),
  burst_slop(default_burst_slop),
  burst_slop_expansion(default_burst_slop_expansion),
  max_skipped_bursts(default_max_skipped_bursts),
  prefix(prefix)
{
};

void
Tag_Finder::setup_graph() {
  // Create the DFA graph for the database of registered tags

  // put all tag IDs in a set:

  Tag_ID_Set s;
  for (Tag_Set::iterator i = tags->begin(); i != tags->end(); ++i)
    s.insert((*i));

  graph.set_all_ids(s);

  // For each phase up to 2 bursts, add appropriate nodes to the graph
  // This is done breadth-first, but non-recursively because the DFA_Graph
  // class keeps a vector of nodes at each phase.
  // We start by looking at all nodes in the previous phase.
  // Each one represents a set of Tag_IDs.  We build an interval_map from
  // pulse gaps for the current phase (including slop) to subsets of these
  // Tag_IDs.
      
  for (unsigned int phase = 1; phase <= 2 * PULSES_PER_BURST; ++phase) {
    // loop over all Tag_ID_Sets at this level
    for (DFA_Graph::Node_For_IDs::iterator it = graph.get_node_for_set_at_phase()[phase-1].begin(); 
	 it != graph.get_node_for_set_at_phase()[phase-1].end(); ++it) {
	  
      // a map of gap sizes to compatible tag IDs
	  
      interval_map < Gap, Tag_ID_Set > m;
	  
      // sanity check: make sure there's only one tag ID left after a pair of bursts
      if (phase == 2 * PULSES_PER_BURST && it->first.size() > 1) {
	std::ostringstream ids;
	for (Tag_ID_Iter i = it->first.begin(); i != it->first.end(); ++i) {
	  ids << (*i)->fullID << "\n";
	}
        std::cerr << "ERROR!" << std::endl;
#ifdef FIND_TAGS_DEBUG        
        graph.get_root()->dump(std::cerr);
#endif
	throw std::runtime_error(string("Error: the following tag IDs are not distinguishable with current parameters:\n") + ids.str());
      }
  
      // for each tag, add its (gap range, ID) pair to the interval_map
	  
      for (Tag_ID_Iter i = it->first.begin(); i != it->first.end(); ++i) {
	Gap g = (*i)->gaps[(phase - 1) % PULSES_PER_BURST];
	Tag_ID_Set id;
	id.insert(*i);
	Gap slop = ((phase - 1) % PULSES_PER_BURST == PULSES_PER_BURST - 1) ? burst_slop : pulse_slop;
	m.add(make_pair(interval < Gap > :: closed(g - slop, g + slop), id));
      }

      if (phase == 2 * PULSES_PER_BURST) {
	// add multiples of the burst interval to this interval map,
	// so we can detect non-consecutive pulses
	for (Tag_ID_Iter i = it->first.begin(); i != it->first.end(); ++i) {
          Tag_ID_Set id;
          id.insert(*i);
          Gap bi = (*i)->gaps[PULSES_PER_BURST];
          Gap g4 = (*i)->gaps[PULSES_PER_BURST - 1];
          for (unsigned int j=2; j <= max_skipped_bursts + 1; ++j) {
            Gap base = (j - 1) * bi + g4;
            Gap slop = burst_slop + (j - 1) * burst_slop_expansion;
            m.add(make_pair(interval < Gap > :: closed(base - slop, base + slop), id));
          }
        }
      }

      // grow the node by this interval_map; Pulses at phase 2 *
      // PULSES_PER_BURST are linked back to pulses at phase
      // PULSES_PER_BURST, so that we can keep track of runs of
      // consecutive bursts from a tag
      graph.grow(it->second, m, (phase != 2 * PULSES_PER_BURST) ? phase : PULSES_PER_BURST);
    }
  }
#ifdef FIND_TAGS_DEBUG
  //  graph.get_root()->dump(std::cerr);
#endif
};

void
Tag_Finder::set_default_pulse_slop_ms(float pulse_slop_ms) {
  default_pulse_slop = pulse_slop_ms / 1000.0;	// stored as seconds
};

void
Tag_Finder::set_default_burst_slop_ms(float burst_slop_ms) {
  default_burst_slop = burst_slop_ms / 1000.0;	// stored as seconds
};

void
Tag_Finder::set_default_burst_slop_expansion_ms(float burst_slop_expansion_ms) {
  default_burst_slop_expansion = burst_slop_expansion_ms / 1000.0;   // stored as seconds
};

void
Tag_Finder::set_default_max_skipped_bursts(unsigned int skip) {
  default_max_skipped_bursts = skip;
};

void
Tag_Finder::set_out_stream(ostream *os) {
  out_stream = os;
};

void
Tag_Finder::init() {
  setup_graph();
};

void
Tag_Finder::process(Pulse &p) {
  /* 
     Process one pulse from the input stream.  

     - check pulses against all tag candidates, in order form
       most to least confirmed; this gives confirmed candidates
       priority in accepting pulses

     - destroy existing Tag_Candidates which are too old, given the 
     "now" represented by this pulse's timestamp
    
     - for each remaining Tag_Candidate, see whether this pulse can be added
     (i.e. is compatible with an edge out of the DFA's current state,	and
     has sufficiently similar frequency offset)

     - if so, check whether the pulse confirms the Tag_Candidate:

     - if so, kill any other Tag_Candidates with the same ID (note, this
       means the same Lotek ID, frequency, *and* burst interval) or which
       share any pulses with this one

     - otherwise, if the pulse was added to the candidate, but the candidate
     was in the MULTIPLE tag_id_level before the addition, clone the original
     Tag_Candidate (i.e. the one without the added pulse)

     - if this tag candidate has a CONFIRMED tag_id_level and a point was added,
     dump any bursts so far

     - if this pulse didn't confirm any candidate, then start a new
     Tag_Candidate at this pulse.
  */

  // the clone list 
  Cand_List & cloned_candidates = cands[3];

  bool confirmed_acceptance = false; // has hit been accepted by a confirmed candidate?
 
  // check lists of candidates to
  for (int i = 0; i < 3 && ! confirmed_acceptance; ++i) {

    Cand_List & cs = cands[i];

    for (Cand_List::iterator ci = cs.begin(); ci != cs.end(); /**/ ) {
      
      if (ci->is_too_old_given_pulse_time(p)) {
        Cand_List::iterator di = ci;
        ++ci;
        cs.erase(di);
        continue;
      }

      // see whether this Tag Candidate can accept this pulse
      DFA_Node * next_state = ci->advance_by_pulse(p);
      
      if (!next_state) {
        ++ci;
        continue;
      }

      if (! ci->is_confirmed() && ! ci->next_pulse_confirms()) {
        // clone the candidate, but without the added pulse
        cloned_candidates.push_back(*ci);
      }
  
      if (ci->add_pulse(p, next_state)) {
        // this candidate tag just got to the CONFIRMED level
        
        // now see what candidates should be deleted because they
        // have the same ID or share any pulses; only seek among
        // unconfirmed candidates, as the current one would have
        // already been eliminated if it shared any pulses
        // with a confirmed candidate.
        
        for (int j = 1; j < NUM_CAND_LISTS; ++j) {
          Cand_List & ccs = cands[j];
          for (Cand_List::iterator cci = ccs.begin(); cci != ccs.end(); /**/ ) {
            if (cci != ci 
                && (cci->has_same_id_as(*ci) || cci->shares_any_pulses(*ci)))
              {
                Cand_List::iterator di = cci;
                ++cci;
                ccs.erase(di);
              } else {
              ++cci;
            };
          }
        }

        // push this candidate to end of the confirmed list
        // so it has priority for accepting new hits
        
        Cand_List &confirmed = cands[0];
        confirmed.splice(confirmed.end(), cs, ci);
      }
      if (ci->is_confirmed()) {
	
        // dump all complete bursts from this confirmed tag
        ci->dump_bursts(out_stream, prefix);
	
        // don't start a new candidate with this pulse
        confirmed_acceptance = true;
        
        // don't try to add this pulse to other candidates
        break;
      }
      ++ci;
    } // continue trying letting other tag_candidates try this pulse
    
    // add any cloned candidates to the end of the list
    cs.splice(cs.end(), cloned_candidates);
  }
  // maybe start a new Tag_Candidate with this pulse 
  if (! confirmed_acceptance) {
    cands[2].push_back(Tag_Candidate(this, graph.get_root(), p));
  }
};

void
Tag_Finder::end_processing() {
  // dump any confirmed candidates which have bursts

#if 0
  for (Cand_Set::iterator ci = cands.begin(); ci != cands.end(); ++ci ) {
      
    if (ci->get_tag_id_level() == Tag_Candidate::CONFIRMED && ci->has_burst()) {
      // we're about to dump bursts from a particular tag candidate
      // kill any others which have the same ID or share any pulses
	  
      for (Cand_Set::iterator cci = cands.begin(); cci != cands.end(); /**/ ) {
	if (cci != ci
	    && (cci->has_same_id_as(*ci)
		|| cci->shares_any_pulses(*ci)))
	  { 
	    Cand_Set::iterator di = cci;
	    ++cci;
		
#ifdef FIND_TAGS_DEBUG
	    std::cerr << "Killing id-matching too old candidate " << &(*di) << " with unique_id = " << di->unique_id << std::endl;
#endif
	    cands.erase(di);
	    continue;
	  } else {
	  ++cci;
	}
      }
      // dump the bursts
      ci->dump_bursts(out_stream, prefix);
    }
  }

#endif 
};

float *
Tag_Finder::get_true_gaps(Tag_ID tid) {
      // get the pointer to the true gaps from the database,
      // for calculation of burst parameters
  return &tid->gaps[0];
}

void
Tag_Finder::dump_bogus_burst(Pulse &p) {
  Tag_Candidate::dump_bogus_burst(p.ts, prefix, p.ant_freq, out_stream);
};

Gap Tag_Finder::default_pulse_slop = 0.0015; // 1.5 ms
Gap Tag_Finder::default_burst_slop = 0.010; // 10 ms
Gap Tag_Finder::default_burst_slop_expansion = 0.001; // 1ms = 1 part in 10000 for 10s BI
unsigned int Tag_Finder::default_max_skipped_bursts = 60;

