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
  sscanf(prefix.c_str(), "%hd", &ant);
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

  bool confirmed_acceptance = false; // has pulse been accepted by a confirmed candidate?

  for (int i = 0; i < NUM_CAND_LISTS; ++i) {

    Cand_List & cs = cands[i];

    Cand_List::iterator nextci; // "next" iterator in case we need to delete current one while traversing list

    for (Cand_List::iterator ci = cs.begin(); ci != cs.end() && p.ts >= ci->first; ci = nextci ) {
      nextci = ci;
      ++nextci;

      // check whether candidate has expired
      if (ci->second->expired(p.ts)) {
        auto p = ci->second;
        cs.erase(ci);
        delete p;
        continue;
      }

      // check whether candidate can accept this pulse
      Node * next_state = (ci->second)->advance_by_pulse(p);

      if (! next_state)
        continue;

      // clone the candidate to fork over the "add pulse or don't add pulse" choice

      Tag_Candidate * clone = (ci->second)->clone();

      // NB: DANGEROUS ASSUMPTION!!  Because we've already computed
      // the next value for ci as nextci, inserting the clone (which
      // has the same key as ci does before it accepts the pulse) into
      // the same Cand_List at the position for ci *should* mean the
      // clone is not reached on the remaining iterations of the
      // current loop.  This assumes the hinted insert is placing the
      // new element either before or after the hint, since they share
      // a key.  This assumption will fail if the hinted insert method
      // ever places the new element later in the portion of the list
      // sharing the same key, and if the candidate pointed to
      // by nextci also has the same key.  If the algorithm crashes
      // due to out of memory, we'll know not to do this!

      cs.insert(ci, std::make_pair(clone->min_next_pulse_ts(), clone));

      // add the pulse
      if ((ci->second)->add_pulse(p, next_state)) {
        // this candidate tag just completed a burst at the CONFIRMED level
        // So delete any other candidate for this tag, and delete any
        // other candidate that shares any of the same points.
        delete_competitors(ci, nextci);

        // dump all complete bursts from this confirmed tag
        (ci->second)->dump_bursts(ant);

        // mark that this pulse has been accepted by a candidate at the CONFIRMED level
        confirmed_acceptance = true;
      }

      // this candidate has accepted a pulse, and needs to be re-indexed
      // within its Cand_list, which might also have changed.

      cands[ci->second->tag_id_level].insert(std::make_pair((ci->second)->min_next_pulse_ts(), ci->second));
      cs.erase(ci);

      if (confirmed_acceptance) {
        // we won't try to add this pulse to other candidates
        // so quit booth loops

        i = NUM_CAND_LISTS;
        break;
      }
    } // continue trying letting other tag_candidates try this pulse

  }
  // maybe start a new Tag_Candidate with this pulse
  if (! confirmed_acceptance) {
    auto ntc = new Tag_Candidate(this, graph->root(), p);
    cands[Tag_Candidate::MULTIPLE].insert(std::make_pair(ntc->min_next_pulse_ts(), ntc));
  }
};


Tag_Finder::~Tag_Finder() {
  // dump any confirmed candidates which have bursts
  // delete them even if not
  for (Cand_List::iterator ci = cands[0].begin(); ci != cands[0].end(); ++ci ) {

    if ((ci->second)->get_tag_id_level() == Tag_Candidate::CONFIRMED && (ci->second)->has_burst()) {
      // dump remaining bursts
      (ci->second)->dump_bursts(ant);
    }
    delete (ci->second);
  }
};

void
Tag_Finder::delete_competitors(Cand_List::iterator ci, Cand_List::iterator &nextci) {
  // drop any candidates for the same tag as ci, or sharing any pulses
  // with it.  We do this when ci has just accepted a pulse that completes
  // a burst at the CONFIRMED tag_id_level

  // nextci is bumped up in case we delete the candidate it points to

  for (int j = 0; j < NUM_CAND_LISTS; ++j) {
    for (Cand_List::iterator cci = cands[j].begin(); cci != cands[j].end(); /**/ ) {
      if ((cci->second) != (ci->second)
          && ((cci->second)->has_same_id_as(ci->second)
              || (cci->second)->shares_any_pulses(ci->second))) {
        Cand_List::iterator di = cci;
        ++cci;
        auto p = di->second;
        if (nextci == di) {
          ++nextci;
        }
        cands[j].erase(di);
        delete p;
      } else {
        ++cci;
      }
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
  Tag_Candidate::dump_bogus_burst(p.ts, ant, p.ant_freq);
};

void
Tag_Finder::tag_added(std::pair < Tag *, Tag * > tp) {
  // possibly rename a tag, due to it now being ambiguous
  if (tp.first)
    rename_tag(tp);
  // check for candidates at level SINGLE which might now
  // be at level MULTIPLE
  Cand_List & cs = cands[Tag_Candidate::SINGLE];
  Cand_List::iterator nextci; // "next" iterator in case we need to delete current one while traversing list
  for (Cand_List::iterator ci = cs.begin(); ci != cs.end(); ci = nextci ) {
    nextci = ci;
    ++nextci;
    Tag_Candidate *tc = ci->second;
    if (! tc->state->is_unique()) {
      tc->tag_id_level = Tag_Candidate::MULTIPLE;
      tc->tag = BOGUS_TAG;
      cands[tc->tag_id_level].insert(std::make_pair((ci->second)->min_next_pulse_ts(), ci->second));
      cs.erase(ci);
    }
  }
}

void
Tag_Finder::tag_removed(std::pair < Tag *, Tag * > tp) {
  // possibly rename a tag, due to it now being ambiguous
  if (tp.first)
    rename_tag(tp);
  // check for candidates at level MULTIPLE which might now
  // be at level SINGLE
  Cand_List & cs = cands[Tag_Candidate::MULTIPLE];
  Cand_List::iterator nextci; // "next" iterator in case we need to delete current one while traversing list
  for (Cand_List::iterator ci = cs.begin(); ci != cs.end(); ci = nextci ) {
    nextci = ci;
    ++nextci;
    Tag_Candidate *tc = ci->second;
    if (tc->state->is_unique()) {
      tc->tag_id_level = Tag_Candidate::SINGLE;
      tc->tag = tc->state->get_tag();
      tc->num_pulses = tc->tag->gaps.size();
      cands[tc->tag_id_level].insert(std::make_pair((ci->second)->min_next_pulse_ts(), ci->second));
      cs.erase(ci);
    }
  }
}


void
Tag_Finder::rename_tag(std::pair < Tag *, Tag * > tp) {
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

void
Tag_Finder::dump(Timestamp latest) {
  std::cerr << "Tag_Finder::dump @ " << std::setprecision(14) << latest << std::endl;
  for (int i = 0; i < NUM_CAND_LISTS; ++i) {
    std::cerr << "List " << i << std::endl;

    Cand_List & cs = cands[i];
    for (Cand_List::iterator ci = cs.begin(); ci != cs.end(); ++ci) {
      std::cerr << "Candidate with " << ci->second->pulses.size() << " pulses (ID_level = " << ci->second->tag_id_level << ", hit count=" << ci->second->hit_count << ") min next pulse ts: " << ci->second->min_next_pulse_ts() << "," << " expired? " << (ci->second)->expired(latest) << std::endl;
    }
  }
}
