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

class Ambiguity {              //!< manage groups of indistinguishable tags
public:
  typedef std::set < Tag * > AmbigTags;
  typedef boost::bimap < AmbigTags, Tag * > AmbigBimap;
  typedef AmbigBimap::value_type AmbigSetProxy;

  typedef std::set < Motus_Tag_ID > AmbigIDs;
  typedef boost::bimap < AmbigIDs, Motus_Tag_ID > AmbigIDBimap;
  typedef AmbigIDBimap::value_type AmbigIDSetProxy;

  static AmbigBimap abm;           //!< bimap between sets of indistinguishable real tags and their proxy tag; tracks adding/removing of tags over time
  static AmbigIDBimap ids;         //!< bimap between sets of IDs of indistinguishable real tags and the (negative) ID of their proxy
                                   //!Tag; persistent: a given set of indistinguishable tags always uses the same proxyID
  static int nextID;               //!< (negative) motus_Tag_ID for next proxy created; starts at -1, decremented for each new proxy;
                                   //!these ID value are only valid within a (possibly resumed) session of the tag finder

  // methods

  static void addIDs(Motus_Tag_ID proxyID, AmbigIDs newids);    //!< record proxyID as representing the IDs in ids
  static Tag * add(Tag *t1, Tag * t2);    //!< return the proxy tag representing both t1 and t2 (t1 might already be a proxy)
  static Tag * remove(Tag * t1, Tag *t2); //!< return a real or proxy tag representing the proxy tag t1 with any t2 removed
  static Tag * proxyFor(Tag *t);          //!< return the proxy for a tag, if it is ambiguous; otherwise, returns 0;
  static void setNextProxyID(Motus_Tag_ID proxyID); //!< set the next proxyID to be used
  static void record_if_new( AmbigTags tags, Motus_Tag_ID id); //!< save an ambiguity set to the DB if it is new
  static void record_new(); //!< record any new ambiguity sets to the DB (used when a batch completes processing)

#ifdef DEBUG
  // debug methods
  static void dump();//!< dump the full map
  static void * dump_ptr; //!< pointer to force emission of method
#endif

protected:
  static Tag * newProxy(AmbigTags & tags, Tag * t);       //!< return a new proxy tag representing tags like t and representing the tags in tags


};

#endif // AMBIGUITY_HPP
