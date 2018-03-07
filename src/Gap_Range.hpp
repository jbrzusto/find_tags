#ifndef GAP_RANGE_HPP
#define GAP_RANGE_HPP

#include "find_tags_common.hpp"

struct Gap_Range {
  Gap first;
  Gap second;

  Gap chunkDown(Gap g, Gap chunkiness) { return chunkiness * floor(g / chunkiness);};
  Gap chunkUp  (Gap g, Gap chunkiness) { return chunkiness * ceil (g / chunkiness);};

  Gap_Range(Gap first, Gap second) : first(first), second(second) {};

  Gap_Range(Gap g, Gap tol, float timeFuzz) :
    first (chunkDown(std::min(g - tol, g * (1 - timeFuzz)), tol)),
    second(chunkUp  (std::max(g + tol, g * (1 + timeFuzz)), tol))
  {
  };
};

typedef std::vector < Gap_Range > Gap_Ranges;

#endif // GAP_RANGE_HPP
