#include "Ambiguity.hpp"
#include "Tag.hpp"
#include "Tag_Candidate.hpp"

Ambiguity::Ambiguity() : 
  abm(), 
  nextID(-1) 
{
};
  
Tag *
Ambiguity::add(Tag *t1, Tag * t2) {
  // return a proxy tag representing the ambiguous tags t1 and t2; t1
  // might already be a proxy
  AmbigTags s; // set of ambiguous tags

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

  // ns now holds IDs from t1 (whether proxy or not) and we need a new
  // proxy to represent union(t1, t2);

  // add the new tag
  s.insert(t2);
  // see whether this new set is already represented by a proxy
  auto j = abm.left.find(s);
  if (j != abm.left.end())
    // a proxy already exists for this set of ambiguous tags
    return j->second;
  // create a new proxy tag for this set
  Tag * t = newProxy(t1);
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

  // 
  auto i = abm.right.find(t1);
  if (i == abm.right.end())
    throw std::runtime_error("Sanity check failed: proxy tag is not in ambiguity map");


  auto s = i->second; // get the set of tags so far

  if (! s.count(t2))
    throw std::runtime_error("Trying to remove tag from a proxy that does not represent it");

  if (t1->count == 0) {
      // this proxy tag has not been detected yet, so we can reduce it
      // to remove t2
    s.erase(t2); // remove the tag
    abm.right.replace_data(i, s); // alter the bimap
    return t1;
  }
  // t1 is a proxy not containing t2 and it has been detected, so
  // make a new proxy
  
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
  // create a new proxy tag for this set
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
  Ambiguity *a = reinterpret_cast < Ambiguity * > (t->cbData);
  if (t->count == 1) {
    // first detection, so record this ambiguity group in the DB
    auto i = a->abm.right.find(t);
    for (auto j = i->second.begin(); j != i->second.end(); ++j)
      Tag_Candidate::filer->add_ambiguity(t->motusID, (*j)->motusID);
  }
};

Tag * 
Ambiguity::newProxy(Tag * t) {
  Tag * nt = new Tag();
  *nt = *t;
  nt->motusID = nextID--;
  nt->count = 0;
  nt->setCallback(Ambiguity::detected, reinterpret_cast < void * > (this));
  return nt;
};

