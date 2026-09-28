#pragma once

#include "deformation/FieldDomain.h"
#include "deformation/Types.h"

namespace deformation
{

/**
 * @brief Immutable metadata describing one paired displacement-field revision.
 * @details The forward field is sampled on sourceDomain(), the inverse field
 * on outputDomain(). Both contain LPS-mm displacements: F(x) = x + u(x),
 * G(y) = y + v(y). Their grids may have different sizes, origins, directions,
 * spacings, and valid extents. Intrinsic dimensions must agree.
 *
 * Native-2D domains may have different physical plane embeddings; adapters must
 * account for the baseline mapping between them. Tangency applies to edit
 * increments in their own plane, not necessarily a total source-to-output map.
 *
 * The descriptor owns geometry, not sample buffers. Its revision identifies
 * the entire pair and associated validity data in the owner's history; neither
 * direction has a setter or an independent revision. This contract does NOT
 * certify that samples exist, cover each other's images, are mutual inverses,
 * or pass a numerical policy. Those checks belong to later numerical stages.
 */
class MapPairDescriptor
{
public:
  /**
   * @brief Construct a complete paired-map metadata value.
   * @param sourceDomain Sampling domain for forward displacement values.
   * @param outputDomain Sampling domain for inverse displacement values.
   * @param revision Nonzero identity allocated by the owning edit history.
   * @param policyVersion Nonzero numerical-policy identity, independent of file schema.
   * @throws ContractError For mismatched intrinsic dimensions or unassigned identity/version.
   */
  MapPairDescriptor(
    FieldDomain sourceDomain,
    FieldDomain outputDomain,
    RevisionId revision,
    NumericalPolicyVersion policyVersion);

  /** @brief Return the source-space forward sampling domain. */
  [[nodiscard]] const FieldDomain& sourceDomain() const noexcept;
  /** @brief Return the output-space inverse sampling domain. */
  [[nodiscard]] const FieldDomain& outputDomain() const noexcept;
  /** @brief Return the sampling domain for a direction; invalid enum values throw ContractError. */
  [[nodiscard]] const FieldDomain& samplingDomain(MapDirection direction) const;
  /** @brief Return the identity shared by both directions and their validity data. */
  [[nodiscard]] RevisionId revision() const noexcept;
  /** @brief Return the selected numerical policy, without asserting it has passed. */
  [[nodiscard]] NumericalPolicyVersion policyVersion() const noexcept;

private:
  FieldDomain m_sourceDomain;
  FieldDomain m_outputDomain;
  RevisionId m_revision;
  NumericalPolicyVersion m_policyVersion;
};

} // namespace deformation
