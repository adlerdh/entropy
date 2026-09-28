#pragma once

#include "deformation/Types.h"

#include <glm/mat3x3.hpp>
#include <glm/vec3.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

namespace deformation
{

/** @brief Stored sample counts or discrete indices in x, y, z order. */
using GridSize = std::array<std::uint32_t, 3>;

/**
 * @brief Rectangular valid sample region, with an inclusive begin and exclusive end.
 * @details Describes sample centers, not half-voxel image edges or interpolation
 * coverage. Per-sample validity masks and interpolation halos are separate concerns.
 */
struct IndexExtent
{
  GridSize begin{0, 0, 0}; //!< First valid sample on each stored axis.
  GridSize end{1, 1, 1};   //!< One past the last valid sample on each stored axis.
};

/** @brief Construction parameters; FieldDomain copies and validates them without normalizing them. */
struct DomainGeometry
{
  SpatialDimension dimension = SpatialDimension::Volume; //!< Native dimensionality; never inferred from size.
  GridSize size{1, 1, 1};                                //!< Nonzero stored sample counts.
  glm::dvec3 origin{0.0};                                //!< LPS position of sample (0, 0, 0), in mm.
  glm::dvec3 spacing{1.0};                               //!< Positive mm per index unit; z must be 1 for native 2D.
  glm::dmat3 directions{1.0};                            //!< Orthonormal columns in LPS; reflections are allowed.
  std::optional<IndexExtent> validExtent;                //!< Defaults to the entire stored grid.
};

/**
 * @brief Validated, value-semantic geometry for a sampled displacement or velocity field.
 * @details Physical points satisfy p = origin + directions * (spacing * index).
 * The origin is a sample center; no half-voxel translation is implicit. For a
 * native 2D domain, the first two direction columns span its plane, and the
 * third completes the frame. Stored z size and inactive z spacing are both 1.
 * Direction frames of either handedness are accepted, including reflected
 * medical-image axes. Reflected axes are not themselves a folded deformation.
 *
 * There are no geometry mutators. Copies and const access are safe across
 * threads provided callers do not concurrently assign to the same object.
 * No image pixels, field samples, GPU state, or interpolation policy are owned.
 */
class FieldDomain
{
public:
  /** @brief Absolute tolerance for direction-column dot products against the identity matrix. */
  static constexpr double directionTolerance = 1.0e-8;

  /** @brief Absolute mm tolerance for numerical residue normal to a native 2D plane. */
  static constexpr double planeToleranceMm = 1.0e-6;

  /**
   * @brief Validate and copy a domain definition.
   * @throws ContractError For invalid dimension, geometry, extent, or sample-count overflow.
   * @details Directions are checked but never silently orthogonalized; spacing
   * is never inferred. Rounded frames within directionTolerance retain their values.
   */
  explicit FieldDomain(const DomainGeometry& geometry);

  /** @brief Return the intrinsic spatial dimensionality. */
  [[nodiscard]] SpatialDimension dimension() const noexcept;
  /** @brief Return the canonical physical coordinate convention. */
  [[nodiscard]] static constexpr CoordinateConvention convention() noexcept
  {
    return CoordinateConvention::LpsMillimeters;
  }
  /** @brief Return stored x/y/z sample counts, including any invalid margin. */
  [[nodiscard]] const GridSize& size() const noexcept;
  /** @brief Return the validated rectangular extent of valid sample centers. */
  [[nodiscard]] const IndexExtent& validExtent() const noexcept;
  /** @brief Return the physical origin in LPS millimeters. */
  [[nodiscard]] const glm::dvec3& origin() const noexcept;
  /** @brief Return the sample spacing in millimeters. */
  [[nodiscard]] const glm::dvec3& spacing() const noexcept;
  /** @brief Return the original validated direction columns. */
  [[nodiscard]] const glm::dmat3& directions() const noexcept;
  /** @brief Return the checked product of all stored dimensions, not a byte allocation size. */
  [[nodiscard]] std::size_t sampleCount() const noexcept;

  /**
   * @brief Convert a continuous sample index into an LPS point in mm.
   * @param index Fractional index; native 2D requires exactly zero z.
   * @throws ContractError For non-finite inputs/results or nonzero native-2D z.
   * @details Extrapolation outside the extent is allowed; no clamping or validity is implied.
   */
  [[nodiscard]] glm::dvec3 indexToPhysical(const glm::dvec3& index) const;

  /**
   * @brief Convert an LPS point in mm into a continuous sample index.
   * @throws ContractError For non-finite inputs/results or an off-plane native-2D point.
   * @details Native-2D normal residue within planeToleranceMm is projected to z = 0.
   * Larger offsets are rejected. Extent membership is not required.
   */
  [[nodiscard]] glm::dvec3 physicalToIndex(const glm::dvec3& point) const;

  /**
   * @brief Convert an index-space offset to a physical displacement in LPS mm.
   * @throws ContractError For non-finite inputs/results or nonzero native-2D z.
   * @details Applies spacing and directions, never origin. This is not a conversion of field values already in mm.
   */
  [[nodiscard]] glm::dvec3 indexVectorToPhysical(const glm::dvec3& offset) const;

  /**
   * @brief Convert a physical displacement in LPS mm to an index-space offset.
   * @throws ContractError For non-finite inputs/results or off-plane native-2D displacement.
   * @details Applies inverse directions and spacing, never origin; native-2D residue is treated as for points.
   */
  [[nodiscard]] glm::dvec3 physicalVectorToIndex(const glm::dvec3& displacement) const;

  /**
   * @brief Test the closed center-to-center valid extent without extrapolation or tolerance.
   * @return False for non-finite coordinates or nonzero native-2D z.
   * @details This does not certify interpolation support, per-sample validity, or map invertibility.
   */
  [[nodiscard]] bool containsIndex(const glm::dvec3& index) const noexcept;

private:
  DomainGeometry m_geometry;
  IndexExtent m_validExtent;
  std::size_t m_sampleCount = 1;
  glm::dmat3 m_indexToPhysical{1.0};
  glm::dmat3 m_physicalToIndex{1.0};
};

} // namespace deformation
