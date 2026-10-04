#include "deformation/EditHistory.h"

#include <limits>
#include <stdexcept>
#include <utility>

namespace deformation
{
namespace
{
void validateCheckpoint(
  const FieldDomain& source,
  const FieldDomain& output,
  const std::shared_ptr<const FieldCheckpoint>& checkpoint)
{
  if (
    !checkpoint || checkpoint->forward.size() != source.sampleCount() ||
    checkpoint->inverse.size() != output.sampleCount())
  {
    throw std::invalid_argument("A complete forward/inverse checkpoint is required");
  }
}
} // namespace

EditRevision::EditRevision(
  RevisionId id,
  RevisionId parent,
  FieldDomain sourceDomain,
  FieldDomain outputDomain,
  EditProvenance provenance,
  QualityReport quality,
  NumericalPolicyVersion policyVersion,
  std::vector<BrushDefinition> stroke,
  std::shared_ptr<const FieldCheckpoint> checkpoint)
  : m_parent(parent)
  , m_maps(std::move(sourceDomain), std::move(outputDomain), id, policyVersion)
  , m_provenance(std::move(provenance))
  , m_quality(std::move(quality))
  , m_stroke(std::move(stroke))
  , m_checkpoint(std::move(checkpoint))
{
  validateCheckpoint(m_maps.sourceDomain(), m_maps.outputDomain(), m_checkpoint);
}

EditHistory::EditHistory(
  FieldDomain sourceDomain,
  FieldDomain outputDomain,
  EditProvenance provenance,
  NumericalPolicyVersion policyVersion,
  std::shared_ptr<const FieldCheckpoint> initial)
{
  auto root = std::make_shared<const EditRevision>(
    RevisionId{1},
    RevisionId{},
    std::move(sourceDomain),
    std::move(outputDomain),
    std::move(provenance),
    QualityReport{},
    policyVersion,
    std::vector<BrushDefinition>{},
    std::move(initial));
  m_nodes.emplace(1, Node{std::move(root), {}});
}

EditHistory EditHistory::restore(
  FieldDomain sourceDomain,
  FieldDomain outputDomain,
  EditProvenance provenance,
  NumericalPolicyVersion rootPolicyVersion,
  std::shared_ptr<const FieldCheckpoint> initial,
  const std::vector<ArchivedRevision>& revisions,
  RevisionId cursor)
{
  EditHistory history(
    std::move(sourceDomain),
    std::move(outputDomain),
    std::move(provenance),
    rootPolicyVersion,
    std::move(initial));
  for (const auto& record : revisions) {
    if (
      record.id.value != history.m_nextId || record.parent.value == 0 ||
      !history.m_nodes.contains(record.parent.value) || record.stroke.empty())
    {
      throw std::invalid_argument("Invalid archived edit revision graph");
    }
    const auto parent = history.m_nodes.at(record.parent.value).revision;
    auto revision = std::make_shared<const EditRevision>(
      record.id,
      record.parent,
      parent->maps().sourceDomain(),
      parent->maps().outputDomain(),
      parent->provenance(),
      record.quality,
      record.policyVersion,
      record.stroke,
      record.checkpoint);
    history.m_nodes.at(record.parent.value).children.push_back(record.id);
    history.m_nodes.emplace(record.id.value, Node{std::move(revision), {}});
    ++history.m_nextId;
  }
  if (!history.m_nodes.contains(cursor.value)) throw std::invalid_argument("Archived edit cursor is missing");
  history.m_cursor = cursor;
  return history;
}

const EditRevision& EditHistory::current() const noexcept
{
  return *m_nodes.find(m_cursor.value)->second.revision;
}
std::shared_ptr<const EditRevision> EditHistory::currentPtr() const noexcept
{
  return m_nodes.find(m_cursor.value)->second.revision;
}
std::shared_ptr<const EditRevision> EditHistory::find(RevisionId id) const noexcept
{
  const auto found = m_nodes.find(id.value);
  return found == m_nodes.end() ? nullptr : found->second.revision;
}
std::vector<RevisionId> EditHistory::children(RevisionId id) const
{
  const auto found = m_nodes.find(id.value);
  return found == m_nodes.end() ? std::vector<RevisionId>{} : found->second.children;
}

RevisionId EditHistory::append(
  std::shared_ptr<const FieldCheckpoint> checkpoint,
  QualityReport quality,
  const QualityPolicy& policy,
  std::vector<BrushDefinition> stroke)
{
  if (stroke.empty()) throw std::invalid_argument("An accepted stroke must contain at least one step");
  if (assessCandidate(quality, policy).decision != CandidateDecision::Accept) {
    throw std::invalid_argument("Only numerically accepted map pairs may enter history");
  }
  if (m_nextId == std::numeric_limits<std::uint64_t>::max()) {
    throw std::overflow_error("Edit revision ID space exhausted");
  }
  const auto parentRevision = currentPtr();
  validateCheckpoint(parentRevision->maps().sourceDomain(), parentRevision->maps().outputDomain(), checkpoint);
  const RevisionId id{m_nextId};
  auto revision = std::make_shared<const EditRevision>(
    id,
    m_cursor,
    parentRevision->maps().sourceDomain(),
    parentRevision->maps().outputDomain(),
    parentRevision->provenance(),
    std::move(quality),
    policy.version,
    std::move(stroke),
    std::move(checkpoint));
  auto& siblings = m_nodes.find(m_cursor.value)->second.children;
  siblings.reserve(siblings.size() + 1);
  m_nodes.emplace(id.value, Node{std::move(revision), {}});
  siblings.push_back(id);
  m_cursor = id;
  ++m_nextId;
  return id;
}

bool EditHistory::undo() noexcept
{
  const auto parent = current().parent();
  if (parent.value == 0) return false;
  m_cursor = parent;
  return true;
}
bool EditHistory::redo(RevisionId child) noexcept
{
  const auto& siblings = m_nodes.find(m_cursor.value)->second.children;
  for (const auto id : siblings) {
    if (id == child) {
      m_cursor = child;
      return true;
    }
  }
  return false;
}
} // namespace deformation
