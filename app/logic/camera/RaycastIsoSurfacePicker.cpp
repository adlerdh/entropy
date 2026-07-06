#include "logic/camera/RaycastIsoSurfacePicker.h"

#include "common/MathFuncs.h"

#include <glm/glm.hpp>

#include <algorithm>
#include <cmath>
#include <limits>

namespace
{

constexpr int k_bisectionIterations = 12;
constexpr float k_minStepLength = 1.0e-5f;

bool isValidRequest(const camera3d::IsoSurfacePickRequest& request)
{
  const auto finiteVec = [](const glm::vec3& value) {
    return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
  };
  return finiteVec(request.worldRayOrigin) && finiteVec(request.worldRayDirection) &&
         glm::dot(request.worldRayDirection, request.worldRayDirection) > 0.0f &&
         glm::all(glm::greaterThan(request.pixelDimensions, glm::vec3{0.0f})) && std::isfinite(request.stepLength) &&
         request.stepLength > 0.0f && !request.isoValues.empty() && static_cast<bool>(request.sampleValue);
}

bool crossesIso(double oldValue, double newValue, double isoValue, bool renderFrontFaces, bool renderBackFaces)
{
  const bool frontHit = renderFrontFaces && newValue >= isoValue && oldValue < isoValue;
  const bool backHit = renderBackFaces && newValue < isoValue && oldValue >= isoValue;
  return frontHit || backHit;
}

float refineIsoCrossing(
  const camera3d::IsoSurfacePickRequest& request,
  float t0,
  float t1,
  double value0,
  double isoValue)
{
  float low = t0;
  float high = t1;
  const bool lowIsBelow = value0 < isoValue;

  for (int i = 0; i < k_bisectionIterations; ++i) {
    const float mid = 0.5f * (low + high);
    const glm::vec3 pixelPos{
      request.pixel_T_world * glm::vec4{request.worldRayOrigin + mid * request.worldRayDirection, 1.0f}};
    const std::optional<double> value = request.sampleValue(pixelPos);
    if (!value) {
      break;
    }

    const bool midIsBelow = *value < isoValue;
    if (midIsBelow == lowIsBelow) {
      low = mid;
    }
    else {
      high = mid;
    }
  }

  return 0.5f * (low + high);
}

} // namespace

namespace camera3d
{

std::optional<IsoSurfacePickHit> pickFirstIsoSurfaceHit(const IsoSurfacePickRequest& request)
{
  if (!isValidRequest(request)) {
    return std::nullopt;
  }

  const glm::vec3 worldDir = glm::normalize(request.worldRayDirection);
  const glm::vec3 pixelRayOrigin{request.pixel_T_world * glm::vec4{request.worldRayOrigin, 1.0f}};
  const glm::vec3 pixelRayDirection{request.pixel_T_world * glm::vec4{worldDir, 0.0f}};
  const glm::vec3 pixelBoxMin{-0.5f};
  const glm::vec3 pixelBoxMax = request.pixelDimensions - glm::vec3{0.5f};

  const auto [hitsBox, entryT, exitT] = math::slabs(pixelRayOrigin, pixelRayDirection, pixelBoxMin, pixelBoxMax);
  if (!hitsBox || exitT <= 0.0f) {
    return std::nullopt;
  }

  const float tStart = std::max(0.0f, entryT);
  const float tEnd = exitT;
  const float step = std::max(k_minStepLength, request.stepLength);

  float oldT = tStart;
  glm::vec3 oldPixelPos{request.pixel_T_world * glm::vec4{request.worldRayOrigin + oldT * worldDir, 1.0f}};
  std::optional<double> oldValue = request.sampleValue(oldPixelPos);
  if (!oldValue) {
    return std::nullopt;
  }

  for (float t = std::min(tStart + step, tEnd); t <= tEnd + 0.5f * step; t += step) {
    const float clampedT = std::min(t, tEnd);
    const glm::vec3 pixelPos{request.pixel_T_world * glm::vec4{request.worldRayOrigin + clampedT * worldDir, 1.0f}};
    const std::optional<double> value = request.sampleValue(pixelPos);
    if (!value) {
      oldT = clampedT;
      oldValue = std::nullopt;
      if (clampedT >= tEnd) {
        break;
      }
      continue;
    }

    if (oldValue) {
      std::optional<IsoSurfacePickHit> nearestStepHit;
      for (std::size_t i = 0; i < request.isoValues.size(); ++i) {
        const double isoValue = request.isoValues[i];
        if (!crossesIso(*oldValue, *value, isoValue, request.renderFrontFaces, request.renderBackFaces)) {
          continue;
        }

        const float hitT = refineIsoCrossing(request, oldT, clampedT, *oldValue, isoValue);
        if (!nearestStepHit || hitT < nearestStepHit->rayDistance) {
          const glm::vec3 hitPixelPos{
            request.pixel_T_world * glm::vec4{request.worldRayOrigin + hitT * worldDir, 1.0f}};
          const glm::vec4 worldH = request.world_T_pixel * glm::vec4{hitPixelPos, 1.0f};
          nearestStepHit =
            IsoSurfacePickHit{.worldPosition = glm::vec3{worldH} / worldH.w, .rayDistance = hitT, .isoIndex = i};
        }
      }

      if (nearestStepHit) {
        return nearestStepHit;
      }
    }

    oldT = clampedT;
    oldValue = value;
    if (clampedT >= tEnd) {
      break;
    }
  }

  return std::nullopt;
}

} // namespace camera3d
