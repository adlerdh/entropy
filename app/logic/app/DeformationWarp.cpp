#include "logic/app/DeformationWarp.h"

#include "logic/app/Data.h"

#include "image/Image.h"

#include <glm/gtc/epsilon.hpp>

namespace deformation_warp
{
namespace
{
constexpr float k_geometryEpsilon = 1.0e-4f;

bool vec3NearlyEqual(const glm::vec3& a, const glm::vec3& b)
{
  return glm::all(glm::epsilonEqual(a, b, k_geometryEpsilon));
}
} // namespace

glm::vec3 homogeneousPointToVec3(const glm::vec4& point)
{
  return glm::vec3{point} / point.w;
}

bool warpFieldMatchesImageDomain(const Image& warpField, const Image& image)
{
  return warpField.header().pixelDimensions() == image.header().pixelDimensions() &&
         vec3NearlyEqual(warpField.header().spacing(), image.header().spacing()) &&
         vec3NearlyEqual(warpField.header().origin(), image.header().origin()) &&
         vec3NearlyEqual(warpField.header().directions()[0], image.header().directions()[0]) &&
         vec3NearlyEqual(warpField.header().directions()[1], image.header().directions()[1]) &&
         vec3NearlyEqual(warpField.header().directions()[2], image.header().directions()[2]);
}

std::optional<glm::vec3> sampleWarpDisplacementWorld(const Image& warpField, const glm::vec3& worldPos)
{
  if (warpField.header().numComponentsPerPixel() < 3) {
    return std::nullopt;
  }

  const glm::vec4 warpVoxelH = warpField.transformations().pixel_T_worldDef() * glm::vec4{worldPos, 1.0f};
  const glm::vec3 warpVoxel = homogeneousPointToVec3(warpVoxelH);
  const uint32_t timePoint = warpField.timeAxis().clamp(warpField.settings().activeTimePoint());

  const auto dx = warpField.valueLinear<double>(0, warpVoxel.x, warpVoxel.y, warpVoxel.z, timePoint);
  const auto dy = warpField.valueLinear<double>(1, warpVoxel.x, warpVoxel.y, warpVoxel.z, timePoint);
  const auto dz = warpField.valueLinear<double>(2, warpVoxel.x, warpVoxel.y, warpVoxel.z, timePoint);
  if (!dx || !dy || !dz) {
    return std::nullopt;
  }

  return glm::vec3{static_cast<float>(*dx), static_cast<float>(*dy), static_cast<float>(*dz)};
}

glm::vec4 inverseWarpSampleWorldPosition(const AppData& appData, const uuids::uuid& imageUid, const glm::vec4& worldPos)
{
  const Image* image = appData.image(imageUid);
  if (!image || !image->settings().warpEnabled() || image->settings().warpStrength() <= 0.0f) {
    return worldPos;
  }

  const std::optional<uuids::uuid> inverseWarpUid = appData.imageToActiveInverseWarpUid(imageUid);
  const Image* inverseWarp = inverseWarpUid ? appData.warpField(*inverseWarpUid) : nullptr;
  if (!inverseWarp) {
    return worldPos;
  }

  const glm::vec3 worldPoint = homogeneousPointToVec3(worldPos);
  const std::optional<glm::vec3> displacement = sampleWarpDisplacementWorld(*inverseWarp, worldPoint);
  if (!displacement) {
    return worldPos;
  }

  return glm::vec4{worldPoint + image->settings().warpStrength() * *displacement, 1.0f};
}

glm::vec4
forwardWarpDisplayWorldPosition(const AppData& appData, const uuids::uuid& imageUid, const glm::vec4& worldPos)
{
  const Image* image = appData.image(imageUid);
  if (!image || !image->settings().warpEnabled() || image->settings().warpStrength() <= 0.0f) {
    return worldPos;
  }

  const std::optional<uuids::uuid> forwardWarpUid = appData.imageToActiveForwardWarpUid(imageUid);
  const Image* forwardWarp = forwardWarpUid ? appData.warpField(*forwardWarpUid) : nullptr;
  if (!forwardWarp) {
    return worldPos;
  }

  if (!warpFieldMatchesImageDomain(*forwardWarp, *image)) {
    return worldPos;
  }

  const glm::vec3 worldPoint = homogeneousPointToVec3(worldPos);
  const std::optional<glm::vec3> displacement = sampleWarpDisplacementWorld(*forwardWarp, worldPoint);
  if (!displacement) {
    return worldPos;
  }

  return glm::vec4{worldPoint + image->settings().warpStrength() * *displacement, 1.0f};
}

} // namespace deformation_warp
