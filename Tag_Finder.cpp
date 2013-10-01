#include "Tag_Finder.hpp"

Tag_Finder::Tag_Finder (Nominal_Frequency_kHz nom_freq, Tag_Set *tags, string prefix) :
  nom_freq(nom_freq),
  tags(tags),
  graph(),
  cands(),
  pulse_slop(default_pulse_slop),
  burst_slop(default_burst_slop),
  burst_slop_expansion(default_burst_slop_expansion),
  max_skipped_bursts(default_max_skipped_bursts),
  hit_rate_window(default_hit_rate_window),
  prefix(prefix)
{
};

void
Tag_Finder::setup_graph() {
  // Create the DFA graph for the database of registered tags

  // put all tag IDs in a set:

  Tag_ID_Set s;
  for (Tag_Set::iterator i = tags->begin(); i != tags->end(); ++i)
    s.insert(i->second.id);

  graph.set_all_ids(s);

  // For each phase up to 2 bursts, add appropriate nodes to the graph
  // This is done breadth-first, but non-recursively because the DFA_Graph
  // class keeps a vector of nodes at each phase.
  // We start by looking at all nodes in the previous phase.
  // Each one represents a set of Tag_IDs.  We build an interval_map from
  // pulse gaps for the current phase (including slop) to subsets of these
  // Tag_IDs.
      
  for (unsigned int phase = 1; phase <= 2 * PULSES_PER_BURST - 1; ++phase) {
    // loop over all Tag_ID_Sets at this level
    for (DFA_Graph::Node_For_IDs::iterator it = graph.get_node_for_set_at_phase()[phase-1].begin(); 
	 it != graph.get_node_for_set_at_phase()[phase-1].end(); ++it) {
	  
      // a map of gap sizes to compatible tag IDs
	  
      interval_map < Gap, Tag_ID_Set > m;
	  
      // sanity check: make sure there's only one tag ID left after a pair of bursts
      if (phase == 2 * PULSES_PER_BURST && it->first.size() > 1) {
	std::ostringstream ids;
	for (Tag_ID_Iter i = it->first.begin(); i != it->first.end(); ++i) {
	  ids << (*i) << "\n";
	}
	throw std::runtime_error(string("Error: after 2 bursts, the following tag IDs are not distinguishable with current parameters:\n") + ids.str());
      }
  
      // for each tag, add its (gap range, ID) pair to the interval_map
	  
      for (Tag_ID_Iter i = it->first.begin(); i != it->first.end(); ++i) {
	Gap g = (*tags)[*i].gaps[(phase - 1) % PULSES_PER_BURST];
	Tag_ID_Set id;
	id.insert(*i);
	Gap slop = ((phase - 1) % PULSES_PER_BURST == PULSES_PER_BURST - 1) ? burst_slop : pulse_slop;
	m.add(make_pair(interval < Gap > :: closed(g - slop, g + slop), id));
      }

      if (phase == PULSES_PER_BURST) {
	// add multiples of the burst interval to this interval map,
	// so we can detect non-conseuctive pulses
	Tag_ID_Iter i = it->first.begin();
	Tag_ID_Set id;
	id.insert(*i);
	Gap bi = (*tags)[*i].gaps[PULSES_PER_BURST];
	Gap g4 = (*tags)[*i].gaps[PULSES_PER_BURST - 1];
	for (unsigned int j=2; j <= max_skipped_bursts + 1; ++j) {
	  Gap base = (j - 1) * bi + g4;
	  Gap slop = burst_slop + (j - 1) * burst_slop_expansion;
	  m.add(make_pair(interval < Gap > :: closed(base - slop, base + slop), id));
	}
      }

      // grow the node by this interval_map; Pulses at phase 2 *
      // PULSES_PER_BURST-1 are linked back to pulses at phase
      // PULSES_PER_BURST-1, so that we can keep track of runs of
      // consecutive bursts from a tag
	  
      graph.grow(it->second, m, (phase != 2 * PULSES_PER_BURST - 1) ? phase : PULSES_PER_BURST-1);
    }
  }
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
Tag_Finder::set_default_hit_rate_window(Gap h) {
  default_hit_rate_window = h;
};

void
Tag_Finder::init() {
  setup_graph();
  initialize_tag_buffers();
};

void
Tag_Finder::output_header(ostream * out) {
  (*out) << "\"ant\",\"ts\",\"id\",\"freq\",\"freq.sd\",\"sig\",\"sig.sd\",\"noise\",\"run.id\",\"pos.in.run\",\"slop\",\"burst.slop\",\"hit.rate\",\"ant.freq\"" 
      
#ifdef FIND_TAGS_DEBUG
		<< " ,\"p1\",\"p2\",\"p3\",\"p4\",\"ptr\""
#endif
      
		<< std::endl;
};
    
void
Tag_Finder::process(Pulse &p) {
  /* 
     Process one pulse from the input stream.  

     - destroy existing Tag_Candidates which are too old, given the 
     "now" represented by this pulse's timestamp
    
     - for each remaining Tag_Candidate, see whether this pulse can be added
     (i.e. is compatible with an edge out of the DFA's current state,	and
     has sufficiently similar frequency offset)

     - if so, check whether the pulse confirms the Tag_Candidate:

     - if so, kill any other Tag_Candidates with the same ID

     - otherwise, if the pulse was added to the candidate, but the candidate
     was in the MULTIPLE tag_id_level before the addition, clone the original
     Tag_Candidate (i.e. the one without the added pulse)

     - if this tag candidate has a CONFIRMED tag_id_level and a point was added,
     dump any bursts so far

     - if this pulse didn't confirm any candidate, then start a new
     Tag_Candidate at this pulse.
  */

  Cand_Set cloned_candidates;

  // unless this pulse is used to confirm a tag candidate (in which case it's
  // very likely to be part of that tag) we will start a new tag candidate with it

  bool start_new_candidate_with_pulse = true;

  for (Cand_Set::iterator ci = cands.begin(); ci != cands.end(); /**/ ) {

    if (ci->is_too_old_given_pulse_time(p)) {

      if (ci->get_tag_id_level() == Tag_Candidate::CONFIRMED && ci->has_burst()) {
	bool is_last_of_its_kind = true;

	// if this is the only candidate with the given tag_ID, then dump any bursts before deleting

	for (Cand_Set::iterator cci = cands.begin(); cci != cands.end(); ++cci ) {
	  if (cci != ci 
	      && cci->get_tag_id() == ci->get_tag_id()
	      && cci->get_tag_id_level() == Tag_Candidate::CONFIRMED
	      && cci->has_burst())
	    {
	      is_last_of_its_kind = false;
	      break;
	    }
	}
	if (is_last_of_its_kind)
	  ci->dump_bursts(out_stream, prefix);
      }

#ifdef FIND_TAGS_DEBUG
      std::cerr << "Killing too old candidate " << &(*ci) << " with unique_id = " << ci->unique_id << "; last_ts " << ci->last_ts << "; pulse.ts " << p.ts << std::endl;
#endif

      Cand_Set::iterator di = ci;
      ++ci;
      cands.erase(di);
      continue;
    }

    // see whether this Tag Candidate can accept this pulse
    DFA_Node * next_state = ci->advance_by_pulse(p);

    if (!next_state) {
      ++ci;
      continue;
    }

    // we will be adding the pulse to this tag candidate.  See whether we need
    // to clone a copy without the new pulse.  If a candidate has narrowed
    // down to a unique tag ID, we only clone when adding the first point from
    // a new burst; this allows us to have large burst slop, necessary when dealing
    // with frequent clock resets from some deployments

    if (ci->get_tag_id_level() == Tag_Candidate::MULTIPLE 
	|| ci->at_end_of_burst()) 
      {
	// clone the candidate, but without the added pulse
	cloned_candidates.push_back(*ci);

#ifdef FIND_TAGS_DEBUG
	std::cerr << "cloned candidate " << &(*ci) << " with run id = " 
		  << ci->unique_id  << " to candidate " 
		  << &cloned_candidates.back() << std::endl;
#endif
	  
      }

    if (ci->add_pulse(p, next_state)) {
      // this candidate tag is at level CONFIRMED, and
      // has at least one new burst

      // now see what candidates should be deleted because they
      // have the same ID or share any pulses

      for (Cand_Set::iterator cci = cands.begin(); cci != cands.end(); /**/ ) {
	if (cci != ci 
	    && (cci->has_same_id_as(*ci)
		|| cci->shares_any_pulses(*ci)))
	  {
	    Cand_Set::iterator di = cci;
	    ++cci;
	      
#ifdef FIND_TAGS_DEBUG
	    std::cerr << "Killing (because of id/pulse overlap with confirmed " << &(*ci) << ") candidate " << &(*di) << " with unique_id = " << di->unique_id << std::endl;
#endif
	      
	    cands.erase(di);
	  } else {
	  ++cci;
	};
      }
	
      // dump all complete bursts from this confirmed tag
      ci->dump_bursts(out_stream, prefix);
	
      // don't start a new candidate with this pulse
      start_new_candidate_with_pulse = false;

      // don't try to add this pulse to other candidates
      break;
    }
    ++ci;
  } // continue trying letting other tag_candidates try this pulse

    // add any cloned candidates to the end of the list
  cands.splice(cands.end(), cloned_candidates);

  // maybe start a new Tag_Candidate with this pulse 
  if (start_new_candidate_with_pulse) {
    cands.push_back(Tag_Candidate(this, graph.get_root(), p));

#ifdef FIND_TAGS_DEBUG
    std::cerr << "started candidate " << &cands.back() << " with run id = " << cands.back().unique_id << std::endl;
#endif

  }
};

void
Tag_Finder::end_processing() {
  // dump any confirmed candidates which have bursts

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
};

void
Tag_Finder::initialize_tag_buffers() {
  for (Tag_Set::iterator itag = tags->begin(); itag != tags->end(); ++itag)
    tag_times[itag->first] = std::list < Timestamp > ();
};

void
Tag_Finder::add_tag_hit_timestamp(Tag_ID id, Timestamp ts) {
  std::list < Timestamp > &tts = tag_times[id];
  tts.push_back(ts);
  Timestamp end = tts.back();
  Tag_Finder::Tag_Time_Buffer::iterator it = tts.begin();
  while(end - *it > hit_rate_window)
    ++it;
  tts.erase(tts.begin(), it);
};

float
Tag_Finder::get_tag_hit_rate(Tag_ID tid) {
  std::list < Timestamp > &tts = tag_times[tid];
  if (tts.size() < 2)
    return -1;
  Timestamp gap = tts.back() - tts.front();
  if (gap == 0.0)
    gap = 0.0001; 
  return (tts.size() - 1) / gap;
};

float *
Tag_Finder::get_true_gaps(Tag_ID tid) {
      // get the pointer to the true gaps from the database,
      // for calculation of burst parameters
  return &(*tags)[tid].gaps[0];
}

void
Tag_Finder::dump_bogus_burst(Pulse &p, Tag_ID tag_id) {
  Tag_Candidate::dump_bogus_burst(p.ts, tag_id, p.ant_freq, -96, -96, out_stream);
};

Gap Tag_Finder::default_pulse_slop = 0.0015; // 1.5 ms
Gap Tag_Finder::default_burst_slop = 0.010; // 10 ms
Gap Tag_Finder::default_burst_slop_expansion = 0.001; // 1ms = 1 part in 10000 for 10s BI
unsigned int Tag_Finder::default_max_skipped_bursts = 60;
Gap Tag_Finder::default_hit_rate_window = 60;

