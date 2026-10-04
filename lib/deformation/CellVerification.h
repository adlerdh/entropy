#pragma once

#include "deformation/FieldDomain.h"
#include "deformation/QualityPolicy.h"

#include <glm/vec4.hpp>

#include <cstddef>
#include <span>

namespace deformation
{
/** @brief Results of conservative checks on the continuous multilinear field represented by grid samples. */
struct CellVerification
{
  std::size_t requested = 0;
  std::size_t verified = 0;
  std::size_t folded = 0;
  std::size_t invalid = 0;
  std::size_t unresolved = 0;

  [[nodiscard]] bool complete() const noexcept
  {
    return requested != 0 && verified == requested && folded == 0 && invalid == 0 && unresolved == 0;
  }
};

/**
 * @brief Check every valid-extent cell of an RGBA displacement image in double precision.
 * @details XYZ are LPS millimeters and W must be exactly one. Each cell is
 * bilinear or trilinear between stored vertices. Entry intervals bound its
 * physical Jacobian throughout the cell; determinant and stretch bounds are
 * tested against policy. Ambiguous cells are subdivided through maxDepth.
 * The bounds are padded for floating-point arithmetic, but this is an
 * engineering check of the represented field, not a global injectivity proof.
 * A singleton active axis has no cells and cannot pass. Throws on mismatched
 * sample count or a sample budget overrun.
 */
[[nodiscard]] CellVerification verifyFieldCells(
  const FieldDomain& domain,
  std::span<const glm::vec4> displacement,
  const QualityPolicy& policy,
  unsigned maxDepth = 3,
  std::size_t maxSamples = 4'000'000,
  std::size_t maxSubcells = 2'000'000);
} // namespace deformation
