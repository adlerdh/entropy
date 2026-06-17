#pragma once

#include <glm/fwd.hpp>

#include <array>

namespace intersection
{

inline constexpr int k_numIntersectionVertices = 7; //!< Six polygon vertices plus centroid.

/** @brief Intersection polygon vertices in 3D coordinates. */
using IntersectionVertices = std::array<glm::vec3, k_numIntersectionVertices>;

/** @brief Intersection polygon vertices in homogeneous 4D coordinates. */
using IntersectionVerticesVec4 = std::array<glm::vec4, k_numIntersectionVertices>;

} // namespace intersection
