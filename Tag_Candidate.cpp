#include "Tag_Candidate.hpp"

#include "Tag_Foray.hpp"

Tag_Candidate::Tag_Candidate(Tag_Finder *owner, Node *state, const Pulse &pulse) :
  owner(owner),
  state(state),
  pulses(),
  last_ts(pulse.ts),
  last_dumped_ts(BOGUS_TIMESTAMP),
  tag(BOGUS_TAG),
  tag_id_level(MULTIPLE),
  run_id(0),
  hit_count(0),
  burst_count(0),
  num_pulses(0),
  freq_range(freq_slop_kHz, pulse.dfreq),
  sig_range(sig_slop_dB, pulse.sig),
  burst_step_gcd(0)
{
  pulses.push_back(pulse);
  state->tcLink();
  if (++num_cands > max_num_cands) {
    max_num_cands = num_cands;
    max_cand_time = pulse.ts;
  };
};

Tag_Candidate::~Tag_Candidate() {
  if (tag_id_level == CONFIRMED && run_id > 0) {
    int n = Tag_Foray::num_cands_with_run_id(run_id, -1);
    if (n == 0)
      filer -> end_run(run_id, hit_count, ending_batch);
  }
  --num_cands;
};

Tag_Candidate *
Tag_Candidate::clone() {
  auto tc = new Tag_Candidate(* this);
  tc->state->tcLink();
  if (++num_cands > max_num_cands) {
    max_num_cands = num_cands;
    max_cand_time = last_ts;
  }
  if (tc->tag_id_level == CONFIRMED)
    Tag_Foray::num_cands_with_run_id(run_id, 1);
  return tc;
};

bool Tag_Candidate::has_same_id_as(Tag_Candidate *tc) {
  return tag != BOGUS_TAG && tag == tc->tag;
};

bool Tag_Candidate::shares_any_pulses(Tag_Candidate *tc) {
  // does this tag candidate use any of the pulses
  // used by another candidate?

  // these are two vectors, sorted in order of pulse seq_no,
  // so compare them that way.

  auto i1 = pulses.begin();
  auto e1 = pulses.end();
  auto i2 = tc->pulses.begin();
  auto e2 = tc->pulses.end();

  while(i1 != e1 && i2 != e2) {
    if (i1->seq_no < i2->seq_no) {
      ++ i1;
    } else if (i1->seq_no > i2->seq_no) {
      ++ i2;
    } else {
      return true;
    }
  }
  return false;
};

bool
Tag_Candidate::expired(Timestamp ts) {
  if (! state) {
#ifdef DEBUG
    std::cerr << "whoops - checking for expiry of Tag Candidate with NULL state!" << std::endl;
#endif
    return true;
  }
  bool rv = ts - last_ts > state->get_max_age();

  if (! state->valid()) {
    if (state->tcUnlink())
      state = 0;
    return true;
  }
  return rv;
};

Timestamp
Tag_Candidate::min_next_pulse_ts() {
  return last_ts + state->get_min_age();
};


Node *
Tag_Candidate::advance_by_pulse(const Pulse &p) {

  if (! ( freq_range.is_compatible(p.dfreq)
	  && sig_range.is_compatible(p.sig)))
    return 0;

  Gap gap = p.ts - last_ts;

  // try walk the DFA with this gap
  return state->advance(gap);
}

bool
Tag_Candidate::add_pulse(const Pulse &p, Node *new_state) {

  /*
    Add this pulse to the tag candidate, given the new state this
    will advance the DFA to.

    Return true if adding the pulse completes a burst or the tag_id_level changes.
  */

  int rv = false;

  pulses.push_back(p);
  last_ts = p.ts;

  // adjust use counts for states
  new_state->tcLink();
  state->tcUnlink();

  state = new_state;

  bool pulse_completes_burst = false;
  // see whether our level of ID confirmation has changed

  switch (tag_id_level) {
  case MULTIPLE:

#ifdef DEBUG
    if (pulses.size() > pulses_to_confirm_id)
      throw std::runtime_error("Still at MULTIPLE tag_id_level but with pulses_to_confirm bursts");
#endif

    if (state->is_unique()) {
      tag = state->get_tag();
      num_pulses = tag->gaps.size();
      tag_id_level = SINGLE;
      rv = true;
    }
    break;

  case SINGLE:
  case CONFIRMED:
    pulse_completes_burst = new_state->get_phase() % num_pulses == num_pulses - 1;
    if (pulse_completes_burst && tag_id_level == SINGLE) {
      bool confirm = pulses.size() >= pulses_to_confirm_id;
      if (max_unconfirmed_bursts > 0) {
        // do the gcd test for confirmation (result is pass, fail, or inconclusive)

        // This is the last pulse in a burst.  Get the number of burst
        // intervals between this burst and the previous one detected,
        // which is still in the pulses buffer because we only dump
        // bursts at the CONFIRMED level.  Note that this pulse has
        // already been pushed into the pulses buffer, so we need to
        // look back num_pulses+1 from the end.
        int nbi = round((p.ts - pulses[pulses.size() - (num_pulses + 1)].ts) / state->get_tag()->period);
        burst_step_gcd = gcd(burst_step_gcd, nbi);

        if (burst_step_gcd == 0)
          throw std::runtime_error("Got gcd=0");

        if (burst_step_gcd != 1) {
          // not confirming this candidate because we haven't (yet) passed the gcd test
          confirm = false;
          if (burst_count == 0)
            burst_count = 1;    // first burst
          burst_count += nbi;   // additional burst intervals
          if (burst_count > max_unconfirmed_bursts) {
            // This candidate failed the gcd test:
            // we've gone too many bursts without getting to burst_step_gcd==1, so pretend
            // the last timestamp for this candidate was way too long ago.
            // This forces the candidate to expire next time it is checked.
#ifdef DEBUG
            std::cerr << "Candidate " << (void * ) this << " forced expiry with gcd = " << burst_step_gcd << std::endl;
#endif
            last_ts = FORCE_EXPIRY_TIMESTAMP;
          }
        }
      }
      if (confirm) {
        tag_id_level = CONFIRMED;
        rv =true;
      }
    }
    break;

  default:
    break;
  };


  // Extend the range of signal strengths seen in this burst, but
  // only if this is not the last pulse in a burst.  The range and
  // orientation of antennas can change significantly between
  // bursts, so we don't want to enforce signal strength
  // uniformity across bursts, hence we reset the bounds after
  // each burst.  For frequency offset, the change from burst to burst
  // will be smaller, as it is due to slowly-varying temperature changes
  // and a bit to doppler effects, so for frequency, we recentre the bounded
  // range after each burst.

  if (pulse_completes_burst) {
    sig_range.clear_bounds();
    freq_range.pin_to_centre(); // range can now grow in either direction from centre of current range.
  } else {
    sig_range.extend_by(p.sig);
    freq_range.extend_by(p.dfreq);
  }

  return rv | pulse_completes_burst;
};

Tag *
Tag_Candidate::get_tag() {
  // get the tag associated with this candidate
  // if more than one tag is still compatible, this returns BOGUS_TAG

  return tag;
};

Tag_Candidate::Tag_ID_Level Tag_Candidate::get_tag_id_level() {
  return tag_id_level;
};

bool
Tag_Candidate::is_confirmed() {
  return tag_id_level == CONFIRMED;
};

bool Tag_Candidate::has_burst() {
  return pulses.size() >= num_pulses;
};

bool Tag_Candidate::next_pulse_confirms() {
  return pulses.size() == pulses_to_confirm_id - 1;
};

// bool Tag_Candidate::at_end_of_burst() {
//   return state->get_phase() % num_pulses == num_pulses - 1;
// };

void Tag_Candidate::clear_pulses() {
  // drop pulses from the most recent burst (presumably after
  // outputting it)
  pulses.clear();
};

void
Tag_Candidate::calculate_burst_params(Pulse_Iter & p) {
  // calculate these burst parameters:
  // - mean signal and noise strengths
  // - relative standard deviation (among pulses) of signal strength
  // - mean and sd (among pulses) of offset frequency, in kHz
  // - total slop in gap sizes between observed pulses and registered tag values

  // return true if successful, false otherwise

  float sig		= 0.0;
  float sigsum	= 0.0;
  float sigsumsq	= 0.0;
  float noise		= 0.0;
  float freqsum	= 0.0;
  float freqsumsq	= 0.0;
  float slop   	= 0.0;
  double pts		= 0.0;

  unsigned int n = num_pulses;

  if (last_dumped_ts != BOGUS_TIMESTAMP) {
    Gap g = p->ts - last_dumped_ts;
    burst_par.burst_slop = fmodf(g, tag->period) - tag->gaps[n-1];
  } else {
    burst_par.burst_slop = BOGUS_BURST_SLOP;
  }

  for (unsigned int i = 0; i < n; ++i, ++p) {
    sig	   = powf(10.0, p->sig / 10.0);
    sigsum	  += sig;
    sigsumsq	  += sig*sig;
    noise	  += powf(10.0, p->noise / 10.0);
    freqsum	  += p->dfreq;
    freqsumsq	  += p->dfreq * p->dfreq;
    if (i > 0) {
      slop += fabsf( (p->ts - pts) - tag->gaps[i-1]);
    }
    pts = p->ts;
  }
  last_dumped_ts     = pts;
  burst_par.sig      = 10.0 *	log10f(sigsum / n);
  burst_par.noise    = 10.0 *	log10f(noise / n);
  double sig_radicand = n * sigsumsq - sigsum * sigsum;
  burst_par.sig_sd   = sig_radicand >= 0.0 ? sqrtf((n * sigsumsq - sigsum * sigsum) / (n * (n - 1))) / (sigsum / n) * 100 : 0.0;	// units: % of mean signal strength
  burst_par.freq     = freqsum / n;
  double freq_radicand = n * freqsumsq - freqsum * freqsum;
  burst_par.freq_sd  = freq_radicand >= 0.0 ? sqrtf((n * freqsumsq - freqsum * freqsum) / (n * (n - 1))) : 0.0;
  burst_par.slop     = slop;
  burst_par.num_pred = hit_count;
};

void Tag_Candidate::dump_bursts(short ant) {
  // dump as many bursts as we have data for

  if (pulses.size() < num_pulses)
    return;

  auto p = pulses.begin();
  while (p != pulses.end()) {
    if (++hit_count == 1) {
      // first hit, so start a run
      run_id = filer->begin_run(tag->motusID, ant);
      Tag_Foray::num_cands_with_run_id(run_id, 1);
    }
    Timestamp ts = p->ts;
    calculate_burst_params(p);
    filer->add_hit(
                   run_id,
                   ts,
                   burst_par.sig,
                   burst_par.sig_sd,
                   burst_par.noise,
                   burst_par.freq,
                   burst_par.freq_sd,
                   burst_par.slop,
                   burst_par.burst_slop
                   );
    ++ tag->count;
    if (tag->count == 1 && tag->motusID < 0)
      Ambiguity::detected(tag);
  }
  clear_pulses();
};

void
Tag_Candidate::dump_bogus_burst(Timestamp ts, short ant, Frequency_MHz antfreq) {
  // FIXME: what, if anything, should we do here?
}

void Tag_Candidate::set_freq_slop_kHz(float slop) {
  freq_slop_kHz = slop;
};

void Tag_Candidate::set_sig_slop_dB(float slop) {
  sig_slop_dB = slop;
};

void Tag_Candidate::set_pulses_to_confirm_id(unsigned int n) {
  pulses_to_confirm_id = n;
};

void
Tag_Candidate::set_filer(DB_Filer *dbf) {
  filer = dbf;
};


void
Tag_Candidate::renTag(Tag * t1, Tag * t2) {
  if (tag != t1)
    return;
  // end the current run for t1
  if (hit_count > 0 && run_id > 0) {
    filer -> end_run(run_id, hit_count);
  }
  hit_count = 0;
  // maintain the current confirmation level and pulse buffer;
  // subsequent hits will be reported as t2;
  tag = t2;
}

long long
Tag_Candidate::get_max_num_cands() {
  return max_num_cands;
};

long long
Tag_Candidate::get_num_cands() {
  return num_cands;
};

Timestamp
Tag_Candidate::get_max_cand_time() {
  return max_cand_time;
};

void
Tag_Candidate::set_max_unconfirmed_bursts(int m) {
  max_unconfirmed_bursts = m;
};

int
Tag_Candidate::gcd(int x, int y) {
  while(x != 0) {
    int m = y % x;
    y = x;
    x = m;
  }
  return(y);
};


Frequency_Offset_kHz Tag_Candidate::freq_slop_kHz = 2.0;       // (kHz) maximum allowed frequency bandwidth of a burst

float Tag_Candidate::sig_slop_dB = 10;         // (dB) maximum allowed range of signal strengths within a burst

unsigned int Tag_Candidate::pulses_to_confirm_id = 4; // default number of pulses before a hit is confirmed

const float Tag_Candidate::BOGUS_BURST_SLOP = 0.0; // burst slop reported for first burst of ru

DB_Filer * Tag_Candidate::filer = 0; // handle to output filer

bool Tag_Candidate::ending_batch = false; // true iff we're ending a batch; set by Tag_Foray

Burst_Params Tag_Candidate::burst_par;

long long Tag_Candidate::num_cands = 0; // count of allocated but not freed candidates.
long long Tag_Candidate::max_num_cands = 0; // count of allocated but not freed candidates.
Timestamp Tag_Candidate::max_cand_time = 0; // timestamp at maximum candidate count
int Tag_Candidate::max_unconfirmed_bursts = 10;// maximum bursts before burst_step_gcd reaches 1; if exceeded, we discard candidate
