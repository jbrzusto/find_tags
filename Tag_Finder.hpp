#ifndef TAG_FINDER_HPP
#define TAG_FINDER_HPP

#include "find_tags_common.hpp"

#include "Tag.hpp"
#include "Freq_Setting.hpp"
#include "Graph.hpp"
#include "Event.hpp"
#include "History.hpp"
#include "Ticker.hpp"
#include <boost/serialization/list.hpp>

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

  TagSet * tags;

  Graph * graph;

  Cand_List_Vec	cands;

  // algorithmic parameters


  // output parameters

  string prefix;   // prefix before each tag record (e.g. port number then comma)

  Tag_Finder() {}; //!< default ctor for deserialization

  Tag_Finder(Tag_Foray * owner) {};

  Tag_Finder (Tag_Foray * owner, Nominal_Frequency_kHz nom_freq, TagSet * tags, Graph * g, string prefix="");

  virtual ~Tag_Finder();

  virtual void process (Pulse &p);

  void process_event(Event e); //!< process a tag event; typically adds or removes a tag from the graph of active tags

  Gap *get_true_gaps(Tag * tid);

  void dump_bogus_burst(Pulse &p);

  void rename_tag(std::pair < Tag *, Tag * > tp);

public:
  
  // public serialize function.
  
  template<class Archive>
  void serialize(Archive & ar, const unsigned int version)
  {
    ar & BOOST_SERIALIZATION_NVP( owner );
    ar & BOOST_SERIALIZATION_NVP( graph );
    ar & BOOST_SERIALIZATION_NVP( cands );
    ar & BOOST_SERIALIZATION_NVP( prefix );
    
  };
};


#endif // TAG_FINDER_HPP
