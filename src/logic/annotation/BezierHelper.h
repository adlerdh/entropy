#ifndef BEZIER_HELPER_H
#define BEZIER_HELPER_H

#include <glm/fwd.hpp>

#include <tuple>
#include <vector>

/**
 * @brief computeBezierCommands
 * @param points
 * @param smoothing
 * @param closed
 * @return
 */
std::vector<std::tuple<glm::vec2, glm::vec2, glm::vec2> > computeBezierCommands(
  const std::vector<glm::vec2>& points, float smoothing, bool closed
);

#endif // BEZIER_HELPER
