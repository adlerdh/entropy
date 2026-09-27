#pragma once

#include "image/WarpInversion.h"

#include <glm/mat4x4.hpp>
#include <uuid.h>

#include <atomic>
#include <memory>
#include <optional>
#include <string>

class AppData;

namespace warp_inversion
{
/// Captured live-state dependencies and shared controls for an asynchronous warp inversion.
/// Identity, revision, and transform fields describe the inputs at submission time; canPublish() checks them on
/// completion.
struct RequestState
{
  uuids::uuid imageUid;                     ///< Image whose forward or inverse warp will be assigned.
  uuids::uuid sourceWarpUid;                ///< Active warp used to compute the opposite direction.
  uuids::uuid domainUid;                    ///< Image defining the computed warp's output grid.
  std::optional<uuids::uuid> referenceUid;  ///< Reference image selected when the request was submitted.
  std::optional<uuids::uuid> targetWarpUid; ///< Existing destination assignment, including the absence of one.
  uint64_t sourcePixelRevision = 0;         ///< Source warp's voxel-data revision at submission.
  uint64_t sourceGeometryRevision = 0;      ///< Source warp's spatial-geometry revision at submission.
  uint64_t domainGeometryRevision = 0;      ///< Output domain's spatial-geometry revision at submission.
  uint64_t imageGeometryRevision = 0;       ///< Owning image's spatial-geometry revision at submission.
  uint32_t sourceTimePoint = 0;             ///< Zero-based source warp frame used by the worker.
  glm::mat4 sourceTransform{1.0f};          ///< Source warp's subject-to-display-world affine at submission.
  glm::mat4 imageTransform{1.0f};           ///< Owning image's subject-to-display-world affine at submission.
  ComputedWarpDirection direction{ComputedWarpDirection::Inverse}; ///< Direction being computed.
  std::string description{};                                       ///< Human-readable progress label.
  std::shared_ptr<std::atomic<double>> progress{}; ///< Optional worker progress in [0, 1], shared with the UI.
  std::shared_ptr<std::atomic_bool> cancel{};      ///< Optional shared cancellation flag; true requests cancellation.
};

/**
 * @brief Check whether a completed computation may still replace the live warp assignment.
 * @param data Current application state, inspected on the UI thread.
 * @param state Dependencies captured when the request was submitted.
 * @param latestRequest Whether this is still the newest request for the image and direction.
 * @return True only when the request is current, uncancelled, and its captured dependencies still match.
 * @details Worker-owned image copies do not establish validity; this check must precede publication on the UI thread.
 */
bool canPublish(const AppData& data, const RequestState& state, bool latestRequest);
} // namespace warp_inversion
