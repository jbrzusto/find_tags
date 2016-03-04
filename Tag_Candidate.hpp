#ifndef TAG_CANDIDATE_HPP
#define TAG_CANDIDATE_HPP

#include "find_tags_common.hpp"

#include "Node.hpp"
#include "Pulse.hpp"
#include "Bounded_Range.hpp"
#include "Freq_Setting.hpp"
#include "Burst_Params.hpp"
#include "DB_Filer.hpp"

#include <map>
#include <list>

// forward declaration for include of Tag_Finder.hpp
class Tag_Candidate;
class Tag_Finder;
class Ambiguity;

#include "Tag_Finder.hpp"

class Tag_Candidate {

  /* an automaton walking the DFA graph, recording the pulses it has accepted
     and looking for the first valid burst */
  friend class Tag_Foray;

public:

  typedef enum {CONFIRMED=0, SINGLE=1, MULTIPLE=2} Tag_ID_Level;	// how well-resolved is the tag ID?  Note the order.

protected: 
  // fundamental structure

  Tag_Finder    *owner;
  Node	        *state;		 // where in the appropriate DFA I am
  Pulse_Buffer	 pulses;	 // pulses in the path so far
  Timestamp	 last_ts;        // timestamp of last pulse in last burst
  Timestamp	 last_dumped_ts; // timestamp of last pulse in last dumped burst (used to calculate burst slop when dumping)
  Tag   	 *tag;            // current unique tag ID, if confirmed, or BOGUS_TAG when more than one is compatible
  Tag_ID_Level   tag_id_level;   // how well-resolved is the current tag ID?

  DB_Filer::Run_ID	run_id;	// ID for the run formed by bursts from this candidate (i.e. consecutive in-phase hits on a tag)
  unsigned int		hit_count;	// counter of bursts output by this tag

  unsigned short num_pulses; // number of pulses in burst (once tag has been identified)

  Bounded_Range < Frequency_MHz > freq_range; // range of pulse frequency offsets
  Bounded_Range < float > sig_range;  // range of pulse signal strengths, in dB

  static const float BOGUS_BURST_SLOP; // burst slop reported for first burst of run (where we don't have a previous burst)  Doesn't really matter, since we can distinguish this situation in the data by "pos.in.run==1"

  static Frequency_Offset_kHz freq_slop_kHz; // maximum width of frequency range of pulses (in MHz)
  static float sig_slop_dB; // maximum width of signal range of pulses, in dB

  static unsigned int	pulses_to_confirm_id;	// how many pulses must be seen before an ID level moves to confirmed?

  static bool ending_batch; //!< true iff we're ending a batch; tells dtor whether to end run or not.
  static DB_Filer * filer;

  // buffer used by calculate_burst_params
  static Burst_Params burst_par;

  friend class Tag_Finder;
  friend class Ambiguity;

  static long long num_cands;

  static long long max_num_cands;
  
  static Timestamp max_cand_time;

public:

  Tag_Candidate() {}; // default ctor for deserialization
  
  Tag_Candidate(Tag_Finder *owner, Node *state, const Pulse &pulse);

  Tag_Candidate * clone();

  ~Tag_Candidate();

  bool has_same_id_as(Tag_Candidate *tc);

  bool shares_any_pulses(Tag_Candidate *tc);

  bool expired(const Pulse &p); //!< has tag candidate expired, either due to a long time lag or a tag event which has deleted its state?

  Node * advance_by_pulse(const Pulse &p);

  bool add_pulse(const Pulse &p, Node *new_state);

  Tag * get_tag();

  Tag_ID_Level get_tag_id_level();

  bool is_confirmed();

  bool has_burst();

  bool next_pulse_confirms();

  //  bool at_end_of_burst();

  void clear_pulses();

  void calculate_burst_params(Pulse_Iter &p);
 
  void dump_bursts(string prefix="");

  static void set_freq_slop_kHz(float slop);

  static void set_sig_slop_dB(float slop);

  static void set_pulses_to_confirm_id(unsigned int n);

  static void dump_bogus_burst(Timestamp ts, std::string & prefix, Frequency_MHz antfreq);

  static void set_filer(DB_Filer *dbf);

  static long long get_max_num_cands();

  static long long get_num_cands();

  static Timestamp get_max_cand_time();

  void renTag(Tag * t1, Tag * t2); //!< if this candidate is for tag t1, make it finish any run and start a new one pointing at t2.

  template<class Archive>
  void serialize(Archive & ar, const unsigned int version) {
    ar & BOOST_SERIALIZATION_NVP( owner );
    ar & BOOST_SERIALIZATION_NVP( state );
    ar & BOOST_SERIALIZATION_NVP( pulses );
    ar & BOOST_SERIALIZATION_NVP( last_ts );
    ar & BOOST_SERIALIZATION_NVP( last_dumped_ts );
    ar & BOOST_SERIALIZATION_NVP( tag );
    ar & BOOST_SERIALIZATION_NVP( tag_id_level );
    ar & BOOST_SERIALIZATION_NVP( run_id );
    ar & BOOST_SERIALIZATION_NVP( hit_count );
    ar & BOOST_SERIALIZATION_NVP( num_pulses );
    ar & BOOST_SERIALIZATION_NVP( freq_range );
    ar & BOOST_SERIALIZATION_NVP( sig_range );
  }
};

#endif // TAG_CANDIDATE_HPP
