#include <set>
#include <map>
#include <iostream>
#include <fstream>
#include <stdexcept>
#include <cmath>
#include <memory>
#include <vector>
#include <unordered_map>
#include <string>
#include <random>
#include <stdlib.h>
#include <ctype.h>

#include "Tag_Database.hpp"
#include "find_tags_common.hpp"
#include "Graph.hpp"
#include "Ticker.hpp"

int main (int argc, char * argv[] ) {
  Node::init();
  Graph g("testAddRemoveTag");

  int maxnt = -1;
  int i=1;
  int maxEvts = 0;

  if (argc > i && isdigit(argv[i][0]))
    maxnt = atoi(argv[i++]);

  if (argc > i && isdigit(argv[i][0]))
    maxEvts = atoi(argv[i++]);

  bool randomize = true;
  if (argc > i && std::string(argv[i]) == "-R") {
    randomize = true;
    ++i;
  }

  string fn;
  if (argc > i)
    fn = std::string(argv[i++]);
  else
    fn = std::string("/sgm/cache/motus_meta_db.sqlite");


  Tag_Database T(fn, true);

  auto ts = T.get_tags_at_freq(Nominal_Frequency_kHz(166380));

  int nt = ts->size();

  std::cout << "Got " << nt << " tags\n";

  if (maxnt > 0) {
    nt = maxnt;
    std::cout << "Only using " << nt << "\n";
  }

  std::vector < Tag * > tags (nt);
  i = 0;
  for (auto j = ts->begin(); j != ts->end() && i < nt; ++j) {
#ifdef DEBUG
    std::cout << (*j)->motusID << std::endl;
#endif
    tags[i++] = *j;
  }

  std::vector < bool > inTree;
  if (randomize)
    inTree.reserve(nt);
  else
    inTree.reserve(T.get_max_motusID() + 1); // maximum

  std::random_device rd;     // only used once to initialise (seed) engine
  std::mt19937 rng(rd());    // random-number engine used (Mersenne-Twister in this case)
  std::uniform_int_distribution<int> uni(0, nt - 1); // guaranteed unbiased

  double tol = 0.0015;
  double timeFuzz = 0;

  int numTags = 0;
#ifdef DEBUG
  g.validateSetToNode();
  g.viz();
#endif
  std::cout << "Before any events, # tags in tree is " << numTags << ", # Nodes = " << Node::numNodes() << ", # Sets = " << Set::numSets() << ", # Edges = " << Node::numLinks() << std::endl;

  History *hist = 0;
  Ticker cron;

  if (randomize) {
    hist = T.get_history();
    cron = hist->getTicker();
    maxEvts = hist->size();
  }

  unsigned r;

  for(int numEvts = 0; (! maxEvts) || numEvts < maxEvts ; ++ numEvts) {
    Tag * t;
    if (randomize) {
      r = uni(rng);
      t = tags[r];
    } else {
      auto e = cron.get();
      t = e.tag;
      r = t->motusID;
    }

    if (inTree[r]) {
#ifdef DEBUG
      std::cout << "-" << t->motusID << std::endl;
#endif
      g.delTag(t);
      auto p = Ambiguity::proxyFor(t);
      if (p) {
        std::cerr << "Tag " << t->motusID << " found in ambiguity " << p->motusID << " after deletion.\n";
      } else {
        g.findTag(t, false);
      }
      inTree[r] = false;
      t->active = false;
      --numTags;
    } else {
#ifdef DEBUG
      std::cout << "+" << t->motusID << std::endl;
#endif
      g.addTag(t, tol, timeFuzz, 30);
      auto p = Ambiguity::proxyFor(t);
      if (p)
        g.findTag(p, true);
      else
        g.findTag(t, true);
      inTree[r] = true;
      t->active = true;
      ++numTags;
    };
#ifdef DEBUG
    g.validateSetToNode();
    g.viz();
#endif
    if (numEvts % 1 == 0)
      std::cout << "After "  << (1 + numEvts) << " events, # tags in tree is " << numTags << ", # Nodes = " << Node::numNodes() << ", # Sets = " << Set::numSets() << ", # Edges = " << Node::numLinks() << std::endl;
  }
};
