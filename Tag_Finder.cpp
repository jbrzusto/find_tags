#include "Tag_Finder.hpp"

Tag_Finder::Tag_Finder (Tag_Foray * owner, Nominal_Frequency_kHz nom_freq, TagSet *tags, string prefix) :
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

  // add each tag to the graph.

  for (auto i = tags->begin(); i != tags->end(); ++i)
    graph.addTag(*i, pulse_slop, burst_slop / (*i)->gaps[3], max_skipped_bursts * (*i)->period);
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
Tag_Finder::init(History *h) {
  cron = h->getTicker();
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


  // process any tag events up to this point in time

  while (cron.ts() <= p.ts)
    process_event(cron.get());

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
      Node * next_state = ci->advance_by_pulse(p);
      
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
        ci->dump_bursts(prefix);
	
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
    cands[2].push_back(Tag_Candidate(this, graph.root(), p));
  }
};


Tag_Finder::~Tag_Finder() {
  // dump any confirmed candidates which have bursts
  // delete them even if not
  for (Cand_List::iterator ci = cands[0].begin(); ci != cands[0].end(); ++ci ) {
      
    if (ci->get_tag_id_level() == Tag_Candidate::CONFIRMED && ci->has_burst()) {
      // dump remaining bursts
      ci->dump_bursts(prefix);
    }
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
Tag_Finder::process_event(Event e) {
  auto t = e.tag;
  if (Freq_Setting::as_Nominal_Frequency_kHz(t->freq) != nom_freq)
    return; // skip tags not on this finder's frequency

  switch (e.code) {
  case Event::E_ACTIVATE:
      graph.addTag(t, pulse_slop, burst_slop / t->gaps[3], max_skipped_bursts * t->period);
    break;
  case Event::E_DEACTIVATE:
    graph.delTag(t, pulse_slop, burst_slop / t->gaps[3], max_skipped_bursts * t->period);
    break;
  default:
    std::cerr << "Warning: Unknown event code " << e.code << " for tag " << t->motusID << std::endl;
  };
};


Gap Tag_Finder::default_pulse_slop = 0.0015; // 1.5 ms
Gap Tag_Finder::default_burst_slop = 0.010; // 10 ms
Gap Tag_Finder::default_burst_slop_expansion = 0.001; // 1ms = 1 part in 10000 for 10s BI
unsigned int Tag_Finder::default_max_skipped_bursts = 60;
