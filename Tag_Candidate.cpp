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
  hit_count(0),
  true_gaps(0),
  num_pulses(0),
  freq_range(freq_slop_kHz, pulse.dfreq),
  sig_range(sig_slop_dB, pulse.sig)
{
  pulses[pulse.seq_no] = pulse;
  state->tcLink();
};

Tag_Candidate::~Tag_Candidate() {
  if (hit_count > 0) {
    filer -> end_run(run_id, hit_count);
  }
};

bool Tag_Candidate::has_same_id_as(Tag_Candidate &tc) {
  return tag != BOGUS_TAG && tag == tc.tag;
};

bool Tag_Candidate::shares_any_pulses(Tag_Candidate &tc) {
  // does this tag candidate use any of the pulses
  // used by another candidate?

  Pulse_Buffer::iterator hit_pulses = tc.pulses.begin();

  for (unsigned int i = 0; i < pulses_to_confirm_id; ++i, ++hit_pulses)
    if (pulses.count(hit_pulses->first))
      return true;
  return false;
};

bool Tag_Candidate::expired(const Pulse &p) {
  if (! state->valid()) {
    state->tcUnlink();
    return true;
  }
  return p.ts - last_ts > state->get_max_age();
};

Node * Tag_Candidate::advance_by_pulse(const Pulse &p) {

  if (! ( freq_range.is_compatible(p.dfreq)
	  && sig_range.is_compatible(p.sig)))
    return 0;
  
  Gap gap = p.ts - last_ts;
	  
  // try walk the DFA with this gap
  return state->advance(gap);
}

bool Tag_Candidate::add_pulse(const Pulse &p, Node *new_state) {

  /*
    Add this pulse to the tag candidate, given the new state this
    will advance the DFA to.

    Return true if adding the pulse confirms the tag ID.
  */

  pulses[p.seq_no] = p;
  last_ts = p.ts;

  // extend the range of frequencies seen in this run of bursts
  // (we already know that of the new pulse is compatible)
  freq_range.extend_by(p.dfreq);

  bool pulse_completes_burst = tag_id_level == CONFIRMED && new_state->get_phase() % num_pulses == num_pulses - 1;
  // Extend the range of signal strengths seen in this burst, but
  // only if this is not the last pulse in a burst.  The range and
  // orientation of antennas can change significantly between
  // bursts, so we don't want to enforce signal strength
  // uniformity across bursts, hence we reset the bounds after
  // each burst.

  if (pulse_completes_burst)
    sig_range.clear_bounds();
  else
    sig_range.extend_by(p.sig);

  // adjust use counts for states
  new_state->tcLink();
  state->tcUnlink();

  state = new_state;
      
  // see whether our level of ID confirmation has changed

  bool rv = false;
  switch (tag_id_level) {
  case MULTIPLE:
    if (state->is_unique()) {
      tag = state->get_tag();
      num_pulses = tag->gaps.size();
      tag_id_level = SINGLE;
    }
    break;

  case SINGLE:
    if (pulses.size() >= pulses_to_confirm_id) {
      tag_id_level = CONFIRMED;
      rv= true;
    };
    break;

  case CONFIRMED:
    break;

  default:
    break;
  };

  return rv;
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

  for (unsigned int i=0; i < num_pulses; ++i)
    pulses.erase(pulses.begin());
};

Burst_Params * Tag_Candidate::calculate_burst_params() {
  // calculate these burst parameters:
  // - mean signal and noise strengths
  // - relative standard deviation (among pulses) of signal strength 
  // - mean and sd (among pulses) of offset frequency, in kHz
  // - total slop in gap sizes between observed pulses and registered tag values

  // return true if successful, false otherwise

  static Burst_Params burst_par;

  float sig		= 0.0;
  float sigsum	= 0.0;
  float sigsumsq	= 0.0;
  float noise		= 0.0;
  float freqsum	= 0.0;
  float freqsumsq	= 0.0;
  float slop   	= 0.0;
  double pts		= 0.0;

  unsigned int n = num_pulses;

  if (pulses.size() < n)
    return 0;

  Pulses_Iter p  = pulses.begin();

  if (last_dumped_ts != BOGUS_TIMESTAMP) {
    Gap g = p->second.ts - last_dumped_ts;
    burst_par.burst_slop = fmodf(g, tag->period) - tag->gaps[n-1];
  } else {
    burst_par.burst_slop = BOGUS_BURST_SLOP;
  }

  for (unsigned int i = 0; i < n; ++i, ++p) {
    sig	   = powf(10.0, p->second.sig / 10.0);
    sigsum	  += sig;
    sigsumsq	  += sig*sig;
    noise	  += powf(10.0, p->second.noise / 10.0);
    freqsum	  += p->second.dfreq;
    freqsumsq	  += p->second.dfreq * p->second.dfreq;
    if (i > 0) {
      slop += fabsf( (p->second.ts - pts) - tag->gaps[i-1]);
    }
    pts = p->second.ts;
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
  return &burst_par;
};
 
void Tag_Candidate::dump_bursts(string prefix) {
  // dump as many bursts as we have data for

  if (pulses.size() < num_pulses)
    return;

  // get the hit rate: this is the number of tag hits
  // since the last time dump of this tag id divided by the time
  // elapsed.  The higher it is, the more bogus hits there have
  // been on this tag ID lately.

  Timestamp ts = pulses.rbegin()->second.ts;

  while (pulses.size() >= num_pulses) {
    if (++hit_count == 1) {
      // first hit, so start a run
      run_id = filer->begin_run(tag->motusID);
    }
    Burst_Params *bp = calculate_burst_params();
    ts = pulses.begin()->second.ts;
    filer->add_hit(
                   run_id,
                   prefix.c_str()[0],
                   ts,
                   bp->sig,
                   bp->sig_sd,
                   bp->noise,
                   bp->freq,
                   bp->freq_sd,
                   bp->slop,
                   bp->burst_slop
                   );
    ++ tag->count;
    if (tag->cb)
      tag->cb(tag);
    clear_pulses();
  }
};

void
Tag_Candidate::dump_bogus_burst(Timestamp ts, std::string &prefix, Frequency_MHz antfreq) {
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


Frequency_Offset_kHz Tag_Candidate::freq_slop_kHz = 2.0;       // (kHz) maximum allowed frequency bandwidth of a burst

float Tag_Candidate::sig_slop_dB = 10;         // (dB) maximum allowed range of signal strengths within a burst

unsigned int Tag_Candidate::pulses_to_confirm_id = 4; // default number of pulses before a hit is confirmed

const float Tag_Candidate::BOGUS_BURST_SLOP = 0.0; // burst slop reported for first burst of ru

DB_Filer * Tag_Candidate::filer = 0; // handle to output filer
