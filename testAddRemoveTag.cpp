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

#include "Tag_Database.hpp"
#include "find_tags_common.hpp"
#include "Graph.hpp"

int main (int argc, char * argv[] ) {
  Node::init();
  Graph g("testAddRemoveTag");

  int maxnt = -1;
  if (argc > 1)
    maxnt = atoi(argv[1]);

  string fn;
  if (argc > 1)
    fn = std::string(argv[1]);
  else
    fn = std::string("/sgm/cleaned_motus_tag_db.sqlite");

  Tag_Database T(fn);

  auto ts = T.get_tags_at_freq(Nominal_Frequency_kHz(166380));

  int nt = ts->size();

  std::cout << "Got " << nt << " tags\n";

  if (maxnt > 0)
    nt = maxnt;

  std::vector < Tag * > tags (nt);
  int i = 0;
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

  int numTags = 1;
  for(int numEvts = 0; /**/ ; ++ numEvts) {
    auto r = uni(rng);
    if (inTree[r]) {
#ifdef DEBUG      
      std::cout << "-" << tags[r].motusID << std::endl;
#endif
      g.delTag(tags[r], tol, timeFuzz, 370);
      g.findTag(tags[r], false);
      inTree[r] = false;
      --numTags;
    } else {
#ifdef DEBUG
      std::cout << "+" << tags[r].motusID << std::endl;
#endif
      g.addTag(tags[r], tol, timeFuzz, 370);
      inTree[r] = true;
      ++numTags;
    };
#ifdef DEBUG
    g.validateSetToNode();
    g.viz();
#endif
    if (numEvts % 100 == 0)
      std::cout << "After "  << numEvts << " events, # tags in tree is " << numTags << ", # Nodes = " << Node::numNodes() << ", # Sets = " << Set::numSets() << ", # Edges = " << Node::numLinks() << std::endl;
  }
  g.viz();
};
