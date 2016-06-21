#ifndef AMBIGUITY_HPP
#define AMBIGUITY_HPP

/*
  Ambiguities
  When multiple identical tags are deployed, ambiguity arises as to
  which one has been detected.  We manage this as follows:

  - if a node in the detection graph should be unique (e.g. after
  one full cycle of gaps) but isn't, we replace all motusIDs in
  that node's set with a single new motusID chosen to be negative.
  A new Tag is allocated with that negative motusID, and a new set
  of n entries is made in the batchAmbig table: one for each
  tag in the ambiguity.

  - an ambiguity is only realized when it has been detected;
  otherwise, new tags might be added, or existing ones dropped.  We
  only record an ambiguity group (and allocate a negative motusID) for
  detected amibguities.

  - so an amibiguity can be augmented or reduced by further motus tag
  IDs so long as it hasn't yet been detected.

  - once detected, the ambiguity is recorded to the DB's tagAmbig
  table, thereby "realizing" it.

  - ambiguity groups and runs of detections of a given ambiguity group
  can both span multiple batches.  For a given receiver, ambiguity IDs
  are never re-used; i.e. once a realized ambiguity group is no longer
  active, its ID will never be used again.

  - we maintain a map from set < Motus_Tag_ID > to int where value is
  always negative.

*/
#include "find_tags_common.hpp"
#include <set>

#include <boost/bimap.hpp>

struct Ambiguity {              //!< manage groups of indistinguishable tags
  typedef std::set < Tag * > AmbigTags;
  typedef boost::bimap < AmbigTags, Tag * > AmbigBimap; 
  typedef AmbigBimap::value_type AmbigSetProxy;

  static AmbigBimap abm;           //!< bimap between sets of indistinguishable real Tags and their proxy Tag
  static int nextID;               //!< motus_Tag_ID for next proxy created; initialized from DB_Filer::get_next_ambigID(), decremented for each new proxy

  // methods
  
  static Tag * add(Tag *t1, Tag * t2, Motus_Tag_ID proxyID = 0);    //!< return the proxy tag representing both t1 and t2 (t1 might already be a proxy)
  static Tag * remove(Tag * t1, Tag *t2); //!< return a real or proxy tag representing the proxy tag t1 with any t2 removed
  static Tag * proxyFor(Tag *t);          //!< return the proxy for a tag, if it is ambiguous; otherwise, returns 0;

  static void detected(Tag * t);   //!< callback indicating a proxy tag has been detected; used to force recording of the clone information

  static Tag * newProxy(Tag * t, Motus_Tag_ID proxyID = 0);       //!< return a new proxy tag representing tags like t; if id is specified, make that the proxyID

  static void setNextProxyID(Motus_Tag_ID proxyID); //!< set the next proxyID to be used
  
};

#endif // AMBIGUITY_HPP
