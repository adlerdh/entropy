#pragma once

#include "deformation/FieldDomain.h"

#include <glm/geometric.hpp>
#include <glm/mat3x3.hpp>
#include <glm/matrix.hpp>
#include <glm/vec3.hpp>

#include <cmath>
#include <cstdint>
#include <functional>
#include <vector>

namespace deformation::test
{

/** @brief Analytic test oracle operating on physical points, not index offsets. */
using PointMap = std::function<glm::dvec3(const glm::dvec3&)>;

/** @brief Exact affine map pair used by contract tests; no production inversion is implied. */
struct AnalyticPair
{
  PointMap forward; //!< Source to output in physical mm.
  PointMap inverse; //!< Output to source in physical mm.
};

/** @brief Construct mutually inverse affine maps; fixtures supply nonsingular matrices. */
inline AnalyticPair affinePair(const glm::dmat3& linear, const glm::dvec3& translation)
{
  return {
    [linear, translation](const glm::dvec3& point) { return linear * point + translation; },
    [inverse = glm::inverse(linear), translation](const glm::dvec3& point) {
      return inverse * (point - translation);
    }};
}

/** @brief Return outer(inner(point)); composition order is deliberate. */
inline PointMap compose(const PointMap& outer, const PointMap& inner)
{
  return [outer, inner](const glm::dvec3& point) {
    return outer(inner(point));
  };
}

/** @brief A smooth scalar phantom in physical mm, independent of voxel geometry. */
inline double gaussian(const glm::dvec3& point, const glm::dvec3& center, double radiusMm)
{
  const glm::dvec3 relative = (point - center) / radiusMm;
  return std::exp(-0.5 * glm::dot(relative, relative));
}

/** @brief Visit every stored sample center in x-fastest order, including invalid margins. */
template<class Visitor>
void visitSamples(const FieldDomain& domain, Visitor visitor)
{
  for (std::uint32_t z = 0; z < domain.size()[2]; ++z) {
    for (std::uint32_t y = 0; y < domain.size()[1]; ++y) {
      for (std::uint32_t x = 0; x < domain.size()[0]; ++x) {
        visitor(domain.indexToPhysical(glm::dvec3{x, y, z}));
      }
    }
  }
}

/** @brief Sample an analytic point map as physical displacements, not absolute positions. */
inline std::vector<glm::dvec3> displacementSamples(const FieldDomain& domain, const PointMap& map)
{
  std::vector<glm::dvec3> samples;
  samples.reserve(domain.sampleCount());
  visitSamples(domain, [&](const glm::dvec3& point) { samples.push_back(map(point) - point); });
  return samples;
}

/** @brief Sample a scalar Gaussian without introducing a dependency on an image library. */
inline std::vector<double> scalarSamples(const FieldDomain& domain, const glm::dvec3& center, double radiusMm)
{
  std::vector<double> samples;
  samples.reserve(domain.sampleCount());
  visitSamples(domain, [&](const glm::dvec3& point) { samples.push_back(gaussian(point, center, radiusMm)); });
  return samples;
}

/** @brief Oblique orthonormal frame whose first two axes define a non-axial plane. */
inline glm::dmat3 obliqueFrame()
{
  const glm::dvec3 first = glm::normalize(glm::dvec3{1.0, 1.0, 0.0});
  const glm::dvec3 second = glm::normalize(glm::dvec3{-1.0, 1.0, 2.0});
  return {first, second, glm::cross(first, second)};
}

} // namespace deformation::test
