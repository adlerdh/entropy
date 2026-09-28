#pragma once

#include <compare>
#include <cstdint>

namespace deformation
{

/** @brief Intrinsic spatial dimension, independent of the number of stored slices. */
enum class SpatialDimension : std::uint8_t
{
  Plane = 2, //!< A native 2D image embedded in physical 3D space; motion is tangent to its plane.
  Volume = 3 //!< A spatial 3D image, including a volume with a singleton axis.
};

/** @brief Direction of a physical displacement map; an inverse is never implied by negation. */
enum class MapDirection : std::uint8_t
{
  Forward, //!< F(x) = x + u(x), from source to output; used to move source geometry.
  Inverse  //!< G(y) = y + v(y), from output to source; used to sample source intensities.
};

/**
 * @brief Coordinate convention for this library's physical positions and vectors.
 * @details Inputs must be converted to LPS before construction. Components are
 * physical millimeters, not voxel offsets; the library does not guess RAS or an
 * image-local vector basis. LPS means positive left, posterior, superior.
 */
enum class CoordinateConvention : std::uint8_t
{
  LpsMillimeters //!< The sole canonical convention supported by these contracts.
};

/**
 * @brief Opaque revision identity scoped to one edit history.
 * @details The owner allocates nonzero values and must never reuse one for changed
 * data within that history. This is not a timestamp, persistent UUID, global ID,
 * or content hash. A default value is an unassigned token; map pairs reject it.
 */
struct RevisionId
{
  std::uint64_t value = 0; //!< Owner-assigned identity; zero means unassigned.

  /** @brief Compare tokens within the same history; ordering does not imply ancestry. */
  auto operator<=>(const RevisionId&) const = default;
};

/**
 * @brief Version of the numerical policy interpreting a result.
 * @details Independent of file formats and application versions. A nonzero token
 * identifies an externally defined policy, not proof that its checks have passed.
 * Stage 0 deliberately defines no numerical acceptance thresholds.
 */
struct NumericalPolicyVersion
{
  std::uint32_t value = 0; //!< Policy identity; zero means unspecified.

  /** @brief Compare numerical policy identities. */
  auto operator<=>(const NumericalPolicyVersion&) const = default;
};

} // namespace deformation
