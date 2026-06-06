#include "pnpoly.h"

#include <cstddef>

namespace wrfranklin
{

int pnpoly(const std::vector<glm::vec2>& poly, const glm::vec2& p)
{
  std::size_t i, j;
  bool c = false;

  for (i = 0, j = poly.size() - 1; i < poly.size(); j = i++) {
    if (
      ((poly[i].y > p.y) != (poly[j].y > p.y)) &&
      (p.x < (poly[j].x - poly[i].x) * (p.y - poly[i].y) / (poly[j].y - poly[i].y) + poly[i].x))
    {
      c = !c;
    }
  }

  return c;
}

} // namespace wrfranklin
