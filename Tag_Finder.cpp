#include "Tag_Finder.hpp"

Tag_Finder::Tag_Finder (Tag_Foray * owner, Nominal_Frequency_kHz nom_freq, TagSet *tags, Graph * g, string prefix) :
  owner(owner),
  nom_freq(nom_freq),
  tags(tags),
  graph(g),
  cands(NUM_CAND_LISTS),
  prefix(prefix)
{
};

void
Tag_Finder::process(Pulse &p) {
  /* 
     Process one pulse from the input stream.  

     - check pulses against all tag candidates, in order from
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
      if ((*ci)->expired(p)) {
        Cand_List::iterator di = ci;
        ++ci;
        delete *di;
        cs.erase(di);
        continue;
      }

      // see whether this Tag Candidate can accept this pulse
      Node * next_state = (*ci)->advance_by_pulse(p);
      
      if (!next_state) {
        ++ci;
        continue;
      }

      Tag_Candidate * clone = (*ci)->clone();
      cloned_candidates.push_back(clone);
  
      if ((*ci)->add_pulse(p, next_state)) {
        // this candidate tag just completed a burst at the CONFIRMED level
        
        // now see what candidates should be deleted because they
        // have the same ID or share any pulses
        
        for (int j = 0; j < NUM_CAND_LISTS; ++j) {
          for (Cand_List::iterator cci = cands[j].begin(); cci != cands[j].end(); /**/ ) {
            if ((*cci) != (*ci) 
                && ((*cci)->has_same_id_as(*ci) || (*cci)->shares_any_pulses(*ci)))
              {
                Cand_List::iterator di = cci;
                ++cci;
                auto p = *di;
                cands[j].erase(di);
                delete p;
              } else {
              ++cci;
            };
          }
        }

        // push this candidate to end of the confirmed list
        // so it has priority for accepting new hits

        if (i > 0) {
          Cand_List &confirmed = cands[0];
          confirmed.splice(confirmed.end(), cs, ci);
        }
	
        // dump all complete bursts from this confirmed tag
        (*ci)->dump_bursts(prefix);
	
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
    cands[2].push_back(new Tag_Candidate(this, graph->root(), p));
  }
};


Tag_Finder::~Tag_Finder() {
  // dump any confirmed candidates which have bursts
  // delete them even if not
  for (Cand_List::iterator ci = cands[0].begin(); ci != cands[0].end(); ++ci ) {
      
    if ((*ci)->get_tag_id_level() == Tag_Candidate::CONFIRMED && (*ci)->has_burst()) {
      // dump remaining bursts
      (*ci)->dump_bursts(prefix);
    }
    delete (*ci);
  }
};

Gap *
Tag_Finder::get_true_gaps(TagID tid) {
      // get the pointer to the true gaps from the database,
      // for calculation of burst parameters
  return &tid->gaps[0];
}

void
Tag_Finder::dump_bogus_burst(Pulse &p) {
  Tag_Candidate::dump_bogus_burst(p.ts, prefix, p.ant_freq);
};

void
Tag_Finder::rename_tag(std::pair < Tag *, Tag * > tp) {
  if (! tp.first)
    return;
  for (int i = 0; i < 2; ++i) {
    Cand_List & cs = cands[i];
    for (Cand_List::iterator ci = cs.begin(); ci != cs.end(); ++ci )
      (*ci)->renTag(tp.first, tp.second);
  }
};
