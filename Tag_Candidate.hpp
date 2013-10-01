#ifndef TAG_CANDIDATE_HPP
#define TAG_CANDIDATE_HPP

#include "find_tags_common.hpp"

#include "DFA_Node.hpp"
#include "Pulse.hpp"
#include "Bounded_Range.hpp"
#include "Freq_Setting.hpp"
#include "Burst_Params.hpp"

#include <map>
#include <list>

// forward declaration for include of Tag_Finder.hpp
class Tag_Candidate;

#include "Tag_Finder.hpp"

class Tag_Candidate {

  /* an automaton walking the DFA graph, recording the pulses it has accepted
     and looking for the first valid burst */

public:

  typedef enum {MULTIPLE, SINGLE, CONFIRMED} Tag_ID_Level;	// how well-resolved is the tag ID?

private: 
  // fundamental structure

  Tag_Finder    *owner;
  DFA_Node	*state;		 // where in the appropriate DFA I am
  Pulse_Buffer	 pulses;	 // pulses in the path so far
  std::list < float > hit_rates; // hit rates estimated at each tag hit
  Timestamp	 last_ts;        // timestamp of last pulse in last burst
  Timestamp	 last_dumped_ts; // timestamp of last pulse in last dumped burst (used to calculate burst slop when dumping)
  Tag_ID	 tag_id;         // current unique tag ID, or BOGUS_TAG_ID when more than one is compatible
  Tag_ID_Level   tag_id_level;   // how well-resolved is the current tag ID?

  unsigned long long	unique_id;	// unique ID for bursts output by this candidate (i.e. consecutive in-phase hits on a tag)
  unsigned int		in_a_row;	// counter of bursts output by this tag

  Gap * true_gaps;              // once tag has been identified, this points to the sequence of PULSES_PER_BURST gaps in the tag database

  Bounded_Range < Frequency_MHz > freq_range; // range of pulse frequency offsets
  Bounded_Range < float > sig_range;  // range of pulse signal strengths, in dB

  static const float BOGUS_BURST_SLOP; // burst slop reported for first burst of run (where we don't have a previous burst)  Doesn't really matter, since we can distinguish this situation in the data by "pos.in.run==1"

  static Frequency_Offset_kHz freq_slop_kHz; // maximum width of frequency range of pulses (in MHz)
  static float sig_slop_dB; // maximum width of signal range of pulses, in dB

  static unsigned int	pulses_to_confirm_id;	// how many pulses must be seen before an ID level moves to confirmed?
  

public:
  
  Tag_Candidate(Tag_Finder *owner, DFA_Node *state, const Pulse &pulse);

  bool has_same_id_as(Tag_Candidate &tc);

  bool shares_any_pulses(Tag_Candidate &tc);

  bool is_too_old_given_pulse_time(const Pulse &p);

  DFA_Node * advance_by_pulse(const Pulse &p);

  bool add_pulse(const Pulse &p, DFA_Node *new_state);

  Tag_ID get_tag_id();

  Tag_ID_Level get_tag_id_level();

  bool has_burst();

  bool at_end_of_burst();

  void clear_pulses();

  void set_true_gaps(float *true_gaps);

  Burst_Params * calculate_burst_params();
 
  void dump_bursts(ostream *os, string prefix="");

  static void set_freq_slop_kHz(float slop);

  static void set_sig_slop_dB(float slop);

  static void set_pulses_to_confirm_id(unsigned int n);

  static void dump_bogus_burst(Timestamp ts, Tag_ID tag_id, Frequency_MHz freq, float sig, float noise, ostream *os);
};

#endif // TAG_CANDIDATE_HPP
