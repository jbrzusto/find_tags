#ifndef TAG_FINDER_HPP
#define TAG_FINDER_HPP

#include "find_tags_common.hpp"

#include "Known_Tag.hpp"
#include "Freq_Setting.hpp"
#include "DFA_Graph.hpp"

// forward declaration for inclusion of Tag_Candidate

class Tag_Finder;

#include "Tag_Candidate.hpp"

class Tag_Finder {

  /*
    manage the process of finding tag hits (against a database of registered
    tags) in an input sequence of pulses from a single antenna at a single
    nominal frequency.
  */

public:
  // - internal representation of tag database
  // the set of tags at a single nominal frequency is a "Tag_Set"

  Nominal_Frequency_kHz nom_freq;

  Tag_Set *tags;

  DFA_Graph graph;

  // Set of running DFAs representing possible tags burst sequences

  typedef std::list < Tag_Candidate > Cand_Set;

  Cand_Set	cands;

  typedef std::list < Timestamp > Tag_Time_Buffer; // timestamps of recently seen hits on a tag

  typedef std::map < Tag_ID, Tag_Time_Buffer >  All_Tag_Time_Buffer; // timestamps of recent tag hit times by tag ID 

  All_Tag_Time_Buffer tag_times; // retain recent hit times for all tags

  // algorithmic parameters


  Gap pulse_slop;	// (seconds) allowed slop in timing between
			// burst pulses,
  // in seconds for each pair of
  // consecutive pulses in a burst, this
  // is the maximum amount by which the
  // gap between the pair can differ
  // from the gap between the
  // corresponding pair in a registered
  // tag, and still match the tag.

  static Gap default_pulse_slop;

  Gap burst_slop;	// (seconds) allowed slop in timing between
                        // consecutive tag bursts, in seconds this is
                        // meant to allow for measurement error at tag
                        // registration and detection times

  static Gap default_burst_slop;


  Gap burst_slop_expansion; // (seconds) how much slop in timing
			    // between tag bursts increases with each
  // skipped pulse; this is meant to allow for clock drift between
  // the tag and the receiver.
  static Gap default_burst_slop_expansion;

  // how many consecutive bursts can be missing without terminating a
  // run?

  unsigned int max_skipped_bursts;
  static unsigned int default_max_skipped_bursts;

  // over how long a time window do we estimate hit rates for each tag? (seconds)
  Gap hit_rate_window;
  static Gap default_hit_rate_window;

  // output parameters

  ostream * out_stream;

  string prefix;   // prefix before each tag record (e.g. port number then comma)

  Tag_Finder(){};

  Tag_Finder (Nominal_Frequency_kHz nom_freq, Tag_Set * tags, string prefix="");

  void setup_graph();

  static void set_default_pulse_slop_ms(float pulse_slop_ms);

  static void set_default_burst_slop_ms(float burst_slop_ms);

  static void set_default_burst_slop_expansion_ms(float burst_slop_expansion_ms);

  static void set_default_max_skipped_bursts(unsigned int skip);

  void set_out_stream(ostream *os);

  static void set_default_hit_rate_window(Gap h);

  void init();

  static void output_header(ostream *out);
    
  virtual void process (Pulse &p);

  virtual void end_processing();

  void initialize_tag_buffers();

  void add_tag_hit_timestamp(Tag_ID id, Timestamp ts);

  float get_tag_hit_rate(Tag_ID tid);

  float *get_true_gaps(Tag_ID tid);

  void dump_bogus_burst(Pulse &p, Tag_ID tag_id);

};


#endif // TAG_FINDER_HPP
