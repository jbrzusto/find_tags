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

  string fn;
  if (argc > i)
    fn = std::string(argv[i++]);
  else
    fn = std::string("/sgm/cache/motus_meta_db.sqlite");

  Tag_Database T(fn);

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

  std::vector < bool > inTree(nt);

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

    for(int numEvts = 0; (! maxEvts) || numEvts < maxEvts ; ++ numEvts) {
    auto r = uni(rng);
    if (inTree[r]) {
#ifdef DEBUG
      std::cout << "-" << tags[r]->motusID << std::endl;
#endif
      g.delTag(tags[r], tol, timeFuzz, 30);
      g.findTag(tags[r], false);
      inTree[r] = false;
      tags[r]->active = false;
      --numTags;
    } else {
#ifdef DEBUG
      std::cout << "+" << tags[r]->motusID << std::endl;
#endif
      g.addTag(tags[r], tol, timeFuzz, 30);
      g.findTag(tags[r], true);
      inTree[r] = true;
      tags[r]->active = true;
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
