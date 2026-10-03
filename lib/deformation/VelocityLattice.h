#pragma once

#include "deformation/BrushStep.h"
#include "deformation/FieldDomain.h"

#include <glm/vec3.hpp>

#include <cstddef>
#include <span>
#include <vector>

namespace deformation
{
/**
 * @brief Cubic cardinal B-spline representation of a brush's affine velocity generator.
 * @details Coefficients are generator values at lattice centers, not the weighted
 * velocity samples and not interpolated deformation handles. Cubic cardinal
 * splines reproduce these affine generators with a complete halo. The continuous
 * support/protection envelope is applied AFTER interpolation, preserving exact
 * protected cores. This construction is not a general sample-to-spline fitting method.
 */
class VelocityLattice
{
public:
  /**
   * @brief Construct a regular isotropic coefficient lattice with a full cubic halo.
   * @param brush Immutable recipe to reproduce.
   * @param spacingMm Positive control spacing, independent of image spacing.
   * @param maxCoefficients Allocation limit checked before construction; default 1,000,000 vectors.
   * @throws ContractError For non-finite/unsupported geometry or a lattice exceeding its budget.
   */
  VelocityLattice(BrushStep brush, double spacingMm, std::size_t maxCoefficients = 1'000'000);
  /** @brief Return coefficient geometry, with a singleton inactive axis for native 2D. */
  [[nodiscard]] const FieldDomain& domain() const noexcept;
  /** @brief Return the recipe whose continuous envelope must follow coefficient interpolation. */
  [[nodiscard]] const BrushStep& brush() const noexcept
  {
    return m_brush;
  }
  /** @brief Return immutable x-fastest LPS velocity coefficients; lifetime follows this object. */
  [[nodiscard]] std::span<const glm::dvec3> coefficients() const noexcept;
  /** @brief Evaluate the spline generator times the continuous envelope at a physical point. */
  [[nodiscard]] glm::dvec3 velocity(const glm::dvec3& pointMm) const;

private:
  BrushStep m_brush;
  FieldDomain m_domain;
  std::vector<glm::dvec3> m_coefficients;
};
} // namespace deformation
