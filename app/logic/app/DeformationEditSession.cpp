#include "logic/app/DeformationEditSession.h"

#include <limits>
#include <stdexcept>
#include <utility>

namespace deformation_edit
{
namespace
{
bool present(const Dependencies& dependencies)
{
  return dependencies.sourcePresent && dependencies.referencePresent && dependencies.baselinePresent &&
         !dependencies.provenance.sourceImageId.empty() && !dependencies.provenance.referenceImageId.empty() &&
         !dependencies.provenance.baselineId.empty();
}
bool completeCheckpoint(
  const deformation::EditRevision& revision,
  const std::shared_ptr<const deformation::FieldCheckpoint>& checkpoint)
{
  return checkpoint && checkpoint->forward.size() == revision.maps().sourceDomain().sampleCount() &&
         checkpoint->inverse.size() == revision.maps().outputDomain().sampleCount();
}
} // namespace

DeformationEditSession::DeformationEditSession(deformation::EditHistory history, deformation::QualityPolicy policy)
  : m_history(std::move(history)), m_policy(std::move(policy))
{
  static_cast<void>(deformation::assessCandidate({}, m_policy));
}

std::optional<Preview> DeformationEditSession::preview() const noexcept
{
  if (!m_stroke || m_stroke->steps.empty()) return std::nullopt;
  return Preview{m_stroke->acceptedRevision, m_stroke->id, m_stroke->provisional};
}

bool DeformationEditSession::matches(const Dependencies& live) const noexcept
{
  return m_stroke && present(live) && live == m_stroke->dependencies &&
         m_history.current().id() == m_stroke->acceptedRevision;
}

bool DeformationEditSession::beginStroke(const Dependencies& live)
{
  if (m_stroke || !present(live) || live.provenance != m_history.current().provenance()) return false;
  if (m_nextStrokeId == std::numeric_limits<std::uint64_t>::max()) {
    throw std::overflow_error("Stroke ID space exhausted");
  }
  const auto revision = m_history.currentPtr();
  m_stroke.emplace(Stroke{m_nextStrokeId, revision->id(), live, revision->checkpointPtr(), {}, {}, nullptr});
  ++m_nextStrokeId;
  return true;
}

std::shared_ptr<const Request> DeformationEditSession::prepareStep(
  deformation::BrushDefinition brush,
  const Dependencies& live)
{
  if (!matches(live) || m_stroke->pending) return nullptr;
  if (m_nextTicket == std::numeric_limits<std::uint64_t>::max()) {
    throw std::overflow_error("Edit request ticket space exhausted");
  }
  static_cast<void>(deformation::BrushStep(brush)); // Validate before marking work pending.
  auto cancel = std::make_shared<std::atomic_bool>(false);
  auto request = std::make_shared<const Request>(Request{
    m_nextTicket,
    m_stroke->id,
    m_stroke->acceptedRevision,
    live,
    m_history.current().maps(),
    m_stroke->provisional,
    std::move(brush),
    m_policy,
    cancel});
  m_stroke->pending = request;
  ++m_nextTicket;
  return request;
}

Completion
DeformationEditSession::complete(const std::shared_ptr<const Request>& request, Result result, const Dependencies& live)
{
  if (
    !request || !m_stroke || !m_stroke->pending || m_stroke->pending != request || m_stroke->id != request->strokeId ||
    m_stroke->acceptedRevision != request->acceptedRevision)
  {
    return Completion::Stale;
  }
  if (request->cancel->load()) {
    cancelStroke();
    return Completion::Canceled;
  }
  if (!matches(live) || request->dependencies != live) {
    cancelStroke();
    return Completion::Stale;
  }
  if (
    result.assessment.decision != deformation::CandidateDecision::Accept ||
    deformation::assessCandidate(result.report, request->policy).decision != deformation::CandidateDecision::Accept ||
    !completeCheckpoint(m_history.current(), result.checkpoint))
  {
    cancelStroke();
    return Completion::Rejected;
  }
  m_stroke->steps.push_back(request->brush);
  m_stroke->provisional = std::move(result.checkpoint);
  m_stroke->latestQuality = std::move(result.report);
  m_stroke->pending.reset();
  return Completion::Provisional;
}

bool DeformationEditSession::endStroke(const Dependencies& live)
{
  if (!m_stroke) return false;
  if (!matches(live)) {
    cancelStroke();
    return false;
  }
  if (m_stroke->pending || m_stroke->steps.empty()) return false;
  static_cast<void>(m_history.append(m_stroke->provisional, m_stroke->latestQuality, m_policy, m_stroke->steps));
  m_stroke.reset();
  return true;
}

void DeformationEditSession::cancelStroke() noexcept
{
  if (m_stroke && m_stroke->pending) m_stroke->pending->cancel->store(true);
  m_stroke.reset();
}

bool DeformationEditSession::undo() noexcept
{
  cancelStroke();
  return m_history.undo();
}
bool DeformationEditSession::redo(deformation::RevisionId child) noexcept
{
  cancelStroke();
  return m_history.redo(child);
}
} // namespace deformation_edit
