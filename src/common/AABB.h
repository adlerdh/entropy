#ifndef AABB_H
#define AABB_H

#include <glm/vec3.hpp>

#include <utility>

/// Alias for a generic 3D vector type that uses GLM
template<typename T>
using gvec3 = glm::vec<3, T, glm::highp>;

/// Alias for generic 3D axis-aligned bounding box, defined by a pair of minimum and maximum 3D corners
template<typename T>
using AABB = std::pair<gvec3<T>, gvec3<T> >;

/// Alias for a generic N-D axis-aligned bounding box, defined by a pair of minimum and maximum N-D corners
template<uint32_t N, typename T>
using AABB_N = std::pair<glm::vec<N, T>, glm::vec<N, T> >;

#endif // AABB_H
