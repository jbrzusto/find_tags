#include "Tag_Finder.hpp"

Tag_Finder::Tag_Finder (Tag_Foray * owner, Nominal_Frequency_kHz nom_freq, TagSet *tags, Graph * g, string prefix) :
  owner(owner),
  nom_freq(nom_freq),
  last_reap(0),
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

  bool confirmed_acceptance = false; // has hit been accepted by a confirmed candidate?

  // check lists of candidates to
  for (int i = 0; i < NUM_CAND_LISTS; ++i) {

    // the clone list
    Cand_List cloned_candidates;


    Cand_List & cs = cands[i];

    //    for (Cand_List::iterator ci = cs.begin(); ci != cs.end(); /**/ ) {
    auto cse = cs.end();
    for (Cand_List::iterator ci = cs.begin(); ci != cse && p.ts >= ci->first; /**/ ) {
      if ((ci->second)->expired(p.ts)) {
        Cand_List::iterator di = ci;
        ++ci;
        auto pdi = di->second;
        cs.erase(di);
        delete pdi;
        continue;
      }

      // if the pulse has already been accepted at the "confirmed" level
      if (confirmed_acceptance) {
        ++ci;
        continue;
      }

      // see whether this Tag Candidate can accept this pulse
      Node * next_state = (ci->second)->advance_by_pulse(p);

      if (!next_state) {
        ++ci;
        continue;
      }

      Tag_Candidate * clone = (ci->second)->clone();
      //      cloned_candidates.push_back(clone);
      cloned_candidates.insert(std::make_pair(clone->min_next_pulse_ts(), clone));

      if ((ci->second)->add_pulse(p, next_state)) {
        // this candidate tag just completed a burst at the CONFIRMED level

        // now see what candidates should be deleted because they
        // have the same ID or share any pulses

        // We keep maintain the next value of ci, which might have
        // to be bumped up if we delete its target because of points
        // shared with the newly-confirmed candidate.
        // Otherwise, we risk using an invalidated iterator on the
        // next iteration of the inner loop.

        auto nextci = ci;
        ++nextci;

        for (int j = 0; j < NUM_CAND_LISTS; ++j) {
          for (Cand_List::iterator cci = cands[j].begin(); cci != cands[j].end(); /**/ ) {
            if ((cci->second) != (ci->second)
                && ((cci->second)->has_same_id_as(ci->second) || (cci->second)->shares_any_pulses(ci->second)))
              {
                Cand_List::iterator di = cci;
                ++cci;
                auto p = di->second;
                if (j == i && nextci == di)
                  ++nextci;
                cands[j].erase(di);
                delete p;
              } else {
              ++cci;
            };
          }
        }

        // if not already confirmed, add this candidate to the
        // confirmed list so it has priority for accepting new hits

        if (i > 0) {
          // Cand_List &confirmed = cands[0];
          // confirmed.splice(confirmed.end(), cs, ci);
          auto di=ci;
          ci = cands[0].insert(std::make_pair((ci->second)->min_next_pulse_ts(), ci->second)).first;
          cs.erase(di);
        }

        // dump all complete bursts from this confirmed tag
        (ci->second)->dump_bursts(prefix);

        // don't start a new candidate with this pulse
        confirmed_acceptance = true;

        // we won't try to add this pulse to other candidates
        // so quit booth loops, unless it's time to reap
        // stale candidates
        i = NUM_CAND_LISTS;
        break;

        // NOTE: if this algorithm changes and for some reason
        // we no longer break from both loops here, then
        // if the inner loop is to be continued, it must be
        // with the nextci iterator, which has been protected
        // from invalidation by deletion in the code above.
        //
        // ci = nextci;  // <- in case of not breaking out of
        //               //    inner loop here

      } else {
        // a pulse has been accepted by this candidate; it will
        // have to get re-orderd in its list
        auto tc = ci->second;
        auto nci = ci;
        ++nci;
        cs.erase(ci);
        cs.insert(std::make_pair(tc->min_next_pulse_ts(), tc));
        ci = nci;
      }
    } // continue trying letting other tag_candidates try this pulse

    // add any cloned candidates to the end of the list
    //    cs.splice(cs.end(), cloned_candidates);
    cs.insert(cloned_candidates.begin(), cloned_candidates.end());
  }
  // maybe start a new Tag_Candidate with this pulse
  if (! confirmed_acceptance) {
    //    cands[2].push_back(new Tag_Candidate(this, graph->root(), p));
    auto ntc = new Tag_Candidate(this, graph->root(), p);
    cands[2].insert(std::make_pair(ntc->min_next_pulse_ts(), ntc));
  }
};


Tag_Finder::~Tag_Finder() {
  // dump any confirmed candidates which have bursts
  // delete them even if not
  for (Cand_List::iterator ci = cands[0].begin(); ci != cands[0].end(); ++ci ) {

    if ((ci->second)->get_tag_id_level() == Tag_Candidate::CONFIRMED && (ci->second)->has_burst()) {
      // dump remaining bursts
      (ci->second)->dump_bursts(prefix);
    }
    delete (ci->second);
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
      (ci->second)->renTag(tp.first, tp.second);
  }
};

void
Tag_Finder::reap(Timestamp now) {

  for (int i = 0; i < NUM_CAND_LISTS; ++i) {
    Cand_List & cs = cands[i];
    for (Cand_List::iterator ci = cs.begin(); ci != cs.end(); /**/ ) {
      if ((ci->second)->expired(now)) {
        Cand_List::iterator di = ci;
        ++ci;
        auto pdi = di->second;
        cs.erase(di);
        delete pdi;
      } else {
        ++ci;
      }
    }
  }
  last_reap = now;
}
