#include "Ambiguity.hpp"
#include "Tag.hpp"
#include "Tag_Candidate.hpp"
#include "DB_Filer.hpp"

Ambiguity::AmbigBimap Ambiguity::abm;//  = AmbigMap();
int Ambiguity::nextID = 0;

Tag *
Ambiguity::add(Tag *t1, Tag * t2, Motus_Tag_ID proxyID) {
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
  Tag * t = newProxy(t1, proxyID);

  abm.insert(AmbigSetProxy(s, t));
  return t;
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
  Tag * t = newProxy(t1);
  abm.insert(AmbigSetProxy(s, t));
  return t;
};

Tag *
Ambiguity::proxyFor(Tag *t) {
  for (auto i = abm.left.begin(); i != abm.left.end(); ++i)
    if (i->first.count(t))
      return i->second;
  return 0;
}

void
Ambiguity::detected(Tag * t) {
  // first detection, so record this ambiguity group in the DB
  auto i = abm.right.find(t);
  Tag_Candidate::filer->save_ambiguity(t->motusID, i->second);
};

Tag *
Ambiguity::newProxy(Tag * t, Motus_Tag_ID proxyID) {
  Tag * nt = new Tag();
  *nt = *t;
  if (proxyID) {
    // caller is restoring an existing ambiguity group from the
    // database
    nt->motusID = proxyID;
  } else {
    // caller is building a new ambiguity group
    nt->motusID = nextID--;
    // set the count to zero to indicate this ambiguity group
    // has not yet been detected so is mutable: tags can be added
    // to it until a detection renders it immutable.
    nt->count = 0;
  }
  return nt;
};

void
Ambiguity::setNextProxyID(Motus_Tag_ID proxyID) {
  nextID = proxyID;
};
