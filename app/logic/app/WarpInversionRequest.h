#pragma once
#include "image/WarpInversion.h"
#include <uuid.h>
#include <glm/mat4x4.hpp>
#include <atomic>
#include <memory>
#include <optional>
#include <string>
class AppData;

namespace warp_inversion
{
struct RequestState
{
  uuids::uuid imageUid;
  uuids::uuid sourceWarpUid;
  uuids::uuid domainUid;
  std::optional<uuids::uuid> referenceUid;
  std::optional<uuids::uuid> targetWarpUid;
  uint64_t sourcePixelRevision = 0;
  uint64_t sourceGeometryRevision = 0;
  uint64_t domainGeometryRevision = 0;
  uint64_t imageGeometryRevision = 0;
  uint32_t sourceTimePoint = 0;
  glm::mat4 sourceTransform{1.0f};
  glm::mat4 imageTransform{1.0f};
  ComputedWarpDirection direction{ComputedWarpDirection::Inverse};
  std::string description;
  std::shared_ptr<std::atomic<double>> progress;
  std::shared_ptr<std::atomic_bool> cancel;
};

// UI-thread completion guard. Worker copies never grant permission to replace live state.
bool canPublish(const AppData& data, const RequestState& request, bool latestRequest);
} // namespace warp_inversion
