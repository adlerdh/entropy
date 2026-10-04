#pragma once

#include "deformation/EditHistory.h"

#include <atomic>
#include <cstdint>
#include <memory>
#include <optional>

namespace deformation_edit
{
/** @brief Live application dependencies that must still match before publication. */
struct Dependencies
{
  deformation::EditProvenance provenance;
  std::uint64_t sourcePixelRevision = 0;
  std::uint64_t sourceGeometryRevision = 0;
  std::uint64_t referenceGeometryRevision = 0;
  std::uint64_t baselineRevision = 0;
  std::uint64_t maskRevision = 0;
  bool sourcePresent = true;
  bool referencePresent = true;
  bool baselinePresent = true;
  bool operator==(const Dependencies&) const = default;
};

/** @brief Immutable input to one microstep calculation; cancel is the only mutable shared control. */
struct Request
{
  std::uint64_t ticket;
  std::uint64_t strokeId;
  deformation::RevisionId acceptedRevision;
  Dependencies dependencies;
  deformation::MapPairDescriptor maps;
  std::shared_ptr<const deformation::FieldCheckpoint> base;
  deformation::BrushDefinition brush;
  deformation::QualityPolicy policy;
  std::shared_ptr<std::atomic_bool> cancel;
};

/** @brief Backend result; only a complete accepted pair may become provisional. */
struct Result
{
  std::shared_ptr<const deformation::FieldCheckpoint> checkpoint;
  deformation::QualityReport report;
  deformation::CandidateAssessment assessment{
    deformation::CandidateDecision::Refine,
    deformation::QualityReason::MissingEvidence};
};

enum class Completion
{
  Provisional, //!< Valid microstep stored privately until endStroke.
  Rejected,    //!< The complete stroke was discarded; accepted revision is unchanged.
  Stale,       //!< Ticket or captured dependencies no longer match.
  Canceled     //!< The request was canceled; accepted revision is unchanged.
};

/** @brief Preview values carry the accepted revision they build on; they are never active maps. */
struct Preview
{
  deformation::RevisionId acceptedRevision;
  std::uint64_t strokeId;
  std::shared_ptr<const deformation::FieldCheckpoint> checkpoint;
};

/**
 * @brief Single-owner-thread edit transaction over immutable accepted history.
 * @details Background workers may read Request and its atomic cancel flag, but
 * every method of this class must run on the owning application thread. Only
 * endStroke changes the active revision. Undo and redo restore exact saved
 * values. Deleted images, changed frames, masks, baselines, or cursor movement
 * invalidate in-flight requests before they can publish.
 */
class DeformationEditSession final
{
public:
  DeformationEditSession(deformation::EditHistory history, deformation::QualityPolicy policy);

  [[nodiscard]] const deformation::EditHistory& history() const noexcept
  {
    return m_history;
  }
  [[nodiscard]] std::shared_ptr<const deformation::EditRevision> active() const noexcept
  {
    return m_history.currentPtr();
  }
  [[nodiscard]] std::optional<Preview> preview() const noexcept;
  [[nodiscard]] bool strokeActive() const noexcept
  {
    return m_stroke.has_value();
  }

  /** @brief Capture the current context; false if another stroke is active or identities do not match. */
  bool beginStroke(const Dependencies& live);
  /** @brief Prepare one immutable microstep request; at most one may be pending. */
  [[nodiscard]] std::shared_ptr<const Request> prepareStep(
    deformation::BrushDefinition brush,
    const Dependencies& live);
  /** @brief Guard and privately apply a completed microstep on the owner thread. */
  Completion complete(const std::shared_ptr<const Request>& request, Result result, const Dependencies& live);
  /** @brief Publish all validated microsteps as one revision; false for empty or stale strokes. */
  bool endStroke(const Dependencies& live);
  /** @brief Discard provisional values and cancel the pending request. */
  void cancelStroke() noexcept;
  /** @brief Called when an image, frame, baseline, mask, or session binding changes. */
  void invalidate() noexcept
  {
    cancelStroke();
  }
  /** @brief Cancel provisional work then move to saved accepted values. */
  bool undo() noexcept;
  /** @brief Cancel provisional work then select a saved child branch. */
  bool redo(deformation::RevisionId child) noexcept;

private:
  struct Stroke
  {
    std::uint64_t id;
    deformation::RevisionId acceptedRevision;
    Dependencies dependencies;
    std::shared_ptr<const deformation::FieldCheckpoint> provisional;
    std::vector<deformation::BrushDefinition> steps;
    deformation::QualityReport latestQuality;
    std::shared_ptr<const Request> pending;
  };
  [[nodiscard]] bool matches(const Dependencies& live) const noexcept;

  deformation::EditHistory m_history;
  deformation::QualityPolicy m_policy;
  std::optional<Stroke> m_stroke;
  std::uint64_t m_nextStrokeId = 1;
  std::uint64_t m_nextTicket = 1;
};
} // namespace deformation_edit
