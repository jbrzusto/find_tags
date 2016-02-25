#ifndef TAG_FINDER_HPP
#define TAG_FINDER_HPP

#include "find_tags_common.hpp"

#include "Tag.hpp"
#include "Freq_Setting.hpp"
#include "Graph.hpp"
#include "Event.hpp"

class Tag_Foray;

// #include "Tag_Foray.hpp"

// forward declaration for inclusion of Tag_Candidate

class Tag_Finder;

#include "Tag_Candidate.hpp"

// Set of running DFAs representing possible tags burst sequences
typedef std::list < Tag_Candidate > Cand_List;

typedef std::vector < Cand_List > Cand_List_Vec;

class Tag_Finder {

  /*
    manage the process of finding tag hits (against a database of registered
    tags) in an input sequence of pulses from a single antenna at a single
    nominal frequency.
  */

public:

  static const int NUM_CAND_LISTS = 4; // 3 levels of tag candidates: 0 = confirmed, 1 = single ID, 2 = multiple ID, 3 = clones

  Tag_Foray * owner;

  Nominal_Frequency_kHz nom_freq;

  // - internal representation of tag database
  // the set of tags at a single nominal frequency is a "TagSet"

  TagSet *tags;

  Graph graph;

  Cand_List_Vec	cands;

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


  // output parameters

  string prefix;   // prefix before each tag record (e.g. port number then comma)

  Tag_Finder(Tag_Foray * owner){};

  Tag_Finder (Tag_Foray * owner, Nominal_Frequency_kHz nom_freq, TagSet * tags, string prefix="");

  virtual ~Tag_Finder();

  void setup_graph();

  static void set_default_pulse_slop_ms(float pulse_slop_ms);

  static void set_default_burst_slop_ms(float burst_slop_ms);

  static void set_default_burst_slop_expansion_ms(float burst_slop_expansion_ms);

  static void set_default_max_skipped_bursts(unsigned int skip);

  void init();

  virtual void process (Pulse &p);

  void initialize_tag_buffers();

  void process_event(Event e); //!< process a tag event; typically adds or removes a tag from the graph of active tags

  Gap *get_true_gaps(Tag * tid);

  void dump_bogus_burst(Pulse &p);
};


#endif // TAG_FINDER_HPP
