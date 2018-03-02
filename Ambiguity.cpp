#include "Ambiguity.hpp"
#include "Tag.hpp"
#include "Tag_Candidate.hpp"
#include "DB_Filer.hpp"

Ambiguity::AmbigBimap Ambiguity::abm;//  = AmbigMap();
Ambiguity::AmbigIDBimap Ambiguity::ids;//  = AmbigIDMap();
int Ambiguity::nextID = -1;

void
Ambiguity::addIDs(Motus_Tag_ID proxyID, AmbigIDs newids) {
  if (proxyID >= 0)
    throw std::runtime_error("Attempt to create a proxy with non-negative ID");
  ids.insert(AmbigIDSetProxy(newids, proxyID));
};

Tag *
Ambiguity::add(Tag *t1, Tag * t2) {
  // return a proxy tag representing the ambiguous tags t1 and t2; t1
  // might already be a proxy
  AmbigTags s; // set of ambiguous tags

  if (! t1 || ! t2)
    return 0;

  if (t1->motusID < 0) {
    // this is a proxy
    auto i = abm.right.find(t1);
    s = i->second; // get the set of tags so far
    if (s.count(t2))
      return t1; // t2 is already in the amibguity set

    if (t1->count == 0) {
      // this proxy tag has not been detected yet, so we can augment
      // it to include t2
      s.insert(t2); // add the new tag
      abm.right.replace_data(i, s); // alter the bimap
      return t1;

    }
  } else {
    s.insert(t1);
  }

  // s now holds IDs from t1 (whether proxy or not) and we need a new
  // proxy to represent union(t1, t2);

  // add the new tag
  s.insert(t2);
  // see whether this new set is already represented by a proxy
  auto j = abm.left.find(s);
  if (j != abm.left.end())
    // a proxy already exists for this set of ambiguous tags
    return j->second;
  // create a new proxy tag for this set
  return newProxy(s, t1);
};

Tag *
Ambiguity::remove(Tag * t1, Tag *t2) {
  // return a tag representing those in t1 but excluding the one in t2.
  // t1 must be a proxy tag.
  // If the resulting set is unique, then return its true node,
  // rather than the proxy.

  if (t1->motusID > 0)
    // fail: this is not a proxy
    throw std::runtime_error("Attempt to remove a tag from a non-proxy tag");

  auto i = abm.right.find(t1);
  if (i == abm.right.end())
    throw std::runtime_error("Sanity check failed: proxy tag is not in ambiguity map");

  auto s = i->second; // get the set of tags so far

  if (! s.count(t2))
     throw std::runtime_error("Trying to remove tag from a proxy that does not represent it");

  if (t1->count > 0)
    record_if_new(i->second, i->first->motusID);
  s.erase(t2);
  if (s.size() == 1) {
    // only a single tag left in the set, so return the original
    // (unproxied) tag
    Tag * orig = * s.begin();
    abm.right.erase(i);
    return orig;
  };

  // see whether this new set is already represented by a proxy
  auto j = abm.left.find(s);
  if (j != abm.left.end())
    // a proxy already exists for this set of ambiguous tags
    return j->second;

  if (t1->count == 0) {
    // this proxy tag has not been detected yet, so we can just reduce
    // its tag set in place.
    abm.right.replace_data(i, s); // alter the bimap
    return t1;
  }
  // create a new proxy tag for the reduced set
  return newProxy(s, t1);
};

Tag *
Ambiguity::proxyFor(Tag *t) {
  for (auto i = abm.left.begin(); i != abm.left.end(); ++i)
    if (i->first.count(t))
      return i->second;
  return 0;
}

Tag *
Ambiguity::newProxy(AmbigTags & tags, Tag * t) {
  // lookup the set in the persistent map to see if there's already
  // a (negative) proxyID for it

  AmbigIDs tmpids;
  for (auto i = tags.begin(); i != tags.end(); ++i)
    tmpids.insert((*i)->motusID);

  Motus_Tag_ID proxyID = 0;
  auto j = ids.left.find(tmpids);
  if (j != ids.left.end()) {
    proxyID = j->second;
  } else {
    proxyID = nextID--;
  };

  Tag * nt = new Tag();
  *nt = *t;
  nt->motusID = proxyID;
  nt->count = 0;
  abm.insert(AmbigSetProxy(tags, nt));
  return nt;
};

void
Ambiguity::setNextProxyID(Motus_Tag_ID proxyID) {
  nextID = proxyID;
};

void
Ambiguity::record_if_new( AmbigTags tags, Motus_Tag_ID id) {
  AmbigIDs tmpids;
  for(auto j = tags.begin(); j != tags.end(); ++j)
    tmpids.insert((*j)->motusID);
  if (ids.left.find(tmpids) == ids.left.end()) {
    Tag_Candidate::filer->save_ambiguity(id, tags);
  }
};

void
Ambiguity::record_new() {
  // if any new sets of ambiguous tags were actually detected,
  // write them to the DB

  for (auto i = abm.left.begin(); i != abm.left.end(); ++i) {
    // if a tag in this proxy set was actually detected, search the original mappings for this set
    // and if not found, record this new pairing
    if (i->second->count > 0) {
      record_if_new (i->first, i->second->motusID);
    }
  }
}
