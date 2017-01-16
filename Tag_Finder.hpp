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

//#include "Tag_Foray.hpp"

// forward declaration for inclusion of Tag_Candidate

class Tag_Finder;

#include "Tag_Candidate.hpp"

// Set of running DFAs representing possible tags burst sequences
//typedef std::list < Tag_Candidate * > Cand_List;

// candidate list sorted in order of smallest gap they are ready
// to accept
typedef std::multimap < Gap, Tag_Candidate * > Cand_List;

typedef std::vector < Cand_List > Cand_List_Vec;

class Tag_Finder {

  /*
    manage the process of finding tag hits (against a database of registered
    tags) in an input sequence of pulses from a single antenna at a single
    nominal frequency.
  */

public:

  static const int NUM_CAND_LISTS = 3; // 3 levels of tag candidates: 0 = confirmed, 1 = single ID, 2 = multiple ID

  Tag_Foray * owner;

  Nominal_Frequency_kHz nom_freq;

  Timestamp last_reap;  // last timestamp at which full candidate list was checked for expiry

  // - internal representation of tag database
  // the set of tags at a single nominal frequency is a "TagSet"

  TagSet * tags;

  Graph * graph;

  Cand_List_Vec	cands;

  // algorithmic parameters


  // output parameters

  string prefix;   // prefix before each tag record (e.g. port number then comma)

  short ant;       // antenna value, interpreted from prefix

  Tag_Finder() {}; //!< default ctor for deserialization

  Tag_Finder(Tag_Foray * owner) {};

  Tag_Finder (Tag_Foray * owner, Nominal_Frequency_kHz nom_freq, TagSet * tags, Graph * g, string prefix="");

  virtual ~Tag_Finder();

  virtual void process (Pulse &p);

  void process_event(Event e); //!< process a tag event; typically adds or removes a tag from the graph of active tags

  Gap *get_true_gaps(Tag * tid);

  void dump_bogus_burst(Pulse &p);

  void rename_tag(std::pair < Tag *, Tag * > tp);

  void reap(Timestamp now); //!< reap all tag candidates which have expired by time now; used in case pulse stream from a given
  // slot ends, so we can free up memory and correctly end runs.

  void dump(Timestamp latest); //!< for debugging, dump all current candidates with numbers of pulses and min_timestamp

  void delete_competitors(Cand_List::iterator ci, Cand_List::iterator &nextci); //!< delete any candidates for the same tag or sharing any pulses with * ci

public:

  // public serialize function.

  template<class Archive>
  void serialize(Archive & ar, const unsigned int version)
  {
    ar & BOOST_SERIALIZATION_NVP( owner );
    ar & BOOST_SERIALIZATION_NVP( last_reap );
    ar & BOOST_SERIALIZATION_NVP( graph );
    ar & BOOST_SERIALIZATION_NVP( cands );
    ar & BOOST_SERIALIZATION_NVP( prefix );

    sscanf(prefix.c_str(), "%hd", &ant);
  };
};


#endif // TAG_FINDER_HPP
