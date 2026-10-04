#pragma once

#include "deformation/BrushStep.h"
#include "deformation/MapPairDescriptor.h"
#include "deformation/QualityPolicy.h"

#include <glm/vec4.hpp>

#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace deformation
{
/** @brief Lossless x-fastest RGBA32F values for both map directions. */
struct FieldCheckpoint
{
  std::vector<glm::vec4> forward; //!< Samples on the source domain; XYZ LPS mm, W validity.
  std::vector<glm::vec4> inverse; //!< Samples on the output domain; XYZ LPS mm, W validity.
};

/** @brief Opaque application identities captured with every accepted revision. */
struct EditProvenance
{
  std::string sourceImageId;
  std::string referenceImageId;
  std::string baselineId;
  std::uint32_t frame = 0;
  bool operator==(const EditProvenance&) const = default;
};

/**
 * @brief One immutable accepted map pair and its canonical stroke recipe.
 * @details Checkpoint values, not recipe replay, are authoritative for exact
 * restoration. IDs are scoped to one EditHistory and never reused after a
 * branch. The initial revision has an unassigned parent and an empty stroke.
 */
class EditRevision final
{
public:
  EditRevision(
    RevisionId id,
    RevisionId parent,
    FieldDomain sourceDomain,
    FieldDomain outputDomain,
    EditProvenance provenance,
    QualityReport quality,
    NumericalPolicyVersion policyVersion,
    std::vector<BrushDefinition> stroke,
    std::shared_ptr<const FieldCheckpoint> checkpoint);

  [[nodiscard]] RevisionId id() const noexcept
  {
    return m_maps.revision();
  }
  [[nodiscard]] RevisionId parent() const noexcept
  {
    return m_parent;
  }
  [[nodiscard]] const MapPairDescriptor& maps() const noexcept
  {
    return m_maps;
  }
  [[nodiscard]] const EditProvenance& provenance() const noexcept
  {
    return m_provenance;
  }
  [[nodiscard]] const QualityReport& quality() const noexcept
  {
    return m_quality;
  }
  [[nodiscard]] const std::vector<BrushDefinition>& stroke() const noexcept
  {
    return m_stroke;
  }
  [[nodiscard]] const FieldCheckpoint& checkpoint() const noexcept
  {
    return *m_checkpoint;
  }
  [[nodiscard]] std::shared_ptr<const FieldCheckpoint> checkpointPtr() const noexcept
  {
    return m_checkpoint;
  }

private:
  RevisionId m_parent;
  MapPairDescriptor m_maps;
  EditProvenance m_provenance;
  QualityReport m_quality;
  std::vector<BrushDefinition> m_stroke;
  std::shared_ptr<const FieldCheckpoint> m_checkpoint;
};

/**
 * @brief Branch-preserving cursor over immutable accepted revisions.
 * @details append has the strong exception guarantee: invalid evidence,
 * invalid snapshots, or allocation failure leaves the cursor and graph intact.
 * Undo and redo select stored checkpoints without numerical recomputation.
 */
class EditHistory final
{
public:
  EditHistory(
    FieldDomain sourceDomain,
    FieldDomain outputDomain,
    EditProvenance provenance,
    NumericalPolicyVersion policyVersion,
    std::shared_ptr<const FieldCheckpoint> initial);

  [[nodiscard]] const EditRevision& current() const noexcept;
  [[nodiscard]] std::shared_ptr<const EditRevision> currentPtr() const noexcept;
  [[nodiscard]] std::shared_ptr<const EditRevision> find(RevisionId id) const noexcept;
  [[nodiscard]] std::vector<RevisionId> children(RevisionId id) const;
  [[nodiscard]] std::size_t size() const noexcept
  {
    return m_nodes.size();
  }

  /** @brief Publish one complete stroke after policy acceptance; preserves redo branches. */
  [[nodiscard]] RevisionId append(
    std::shared_ptr<const FieldCheckpoint> checkpoint,
    QualityReport quality,
    const QualityPolicy& policy,
    std::vector<BrushDefinition> stroke);
  /** @brief Move to the saved parent; false at the root. */
  bool undo() noexcept;
  /** @brief Select a direct child; false if it is not a child of the cursor. */
  bool redo(RevisionId child) noexcept;

private:
  struct Node
  {
    std::shared_ptr<const EditRevision> revision;
    std::vector<RevisionId> children;
  };
  std::map<std::uint64_t, Node> m_nodes;
  RevisionId m_cursor{1};
  std::uint64_t m_nextId = 2;
};
} // namespace deformation
