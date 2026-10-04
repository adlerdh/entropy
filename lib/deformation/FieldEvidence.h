#pragma once

#include "deformation/BrushStep.h"
#include "deformation/FieldDomain.h"

#include <glm/vec4.hpp>

#include <cstddef>
#include <optional>
#include <span>

namespace deformation
{
/** @brief Borrowed sampled field; values are x-fastest LPS-mm vectors with W validity. */
struct SampledFieldView
{
  const FieldDomain& domain;
  std::span<const glm::vec4> values;
};

/** @brief Validity-aware bilinear/trilinear displacement at a physical point. */
[[nodiscard]] std::optional<glm::dvec3> sampleDisplacement(SampledFieldView field, const glm::dvec3& pointMm);

/** @brief A borrowed forward/inverse pair with independently specified domains. */
struct SampledFieldPair
{
  SampledFieldView forward;
  SampledFieldView inverse;
};

/** @brief Pointwise difference against a finer calculation at grid centers and cell centers. */
struct RefinementEvidence
{
  double maxErrorMm = 0.0;
  std::size_t requested = 0;
  std::size_t unavailable = 0;
  [[nodiscard]] bool checked() const noexcept
  {
    return requested != 0 && unavailable == 0;
  }
};

/** @brief Compare both map directions on the candidate grids, including off-grid cell centers. */
[[nodiscard]] RefinementEvidence
compareRefinement(SampledFieldPair candidate, SampledFieldPair refined, std::size_t maxPoints = 2'000'000);

/** @brief Conservative upper bound for displacement throughout every requested protected core. */
struct ProtectionEvidence
{
  double maxErrorMm = 0.0;
  std::size_t intersectingCells = 0; //!< CPU cells processed; zero when the caller used a separate proof.
  bool checked = false;
};

/**
 * @brief Bound both increment directions inside protected disks/spheres.
 * @details Multilinear displacement lies in the convex hull of its cell
 * corners. Adaptive subdivision tightens this bound near core boundaries.
 * An uncovered core, invalid contributor, or work-budget exhaustion leaves
 * checked false. No regions means checked true with zero error.
 */
[[nodiscard]] ProtectionEvidence measureProtectedCores(
  SampledFieldPair increment,
  std::span<const ProtectedRegion> regions,
  double targetErrorMm,
  unsigned maxDepth = 5,
  std::size_t maxCells = 1'000'000);
} // namespace deformation
