#include "logic/interaction/TransformationGuide.h"

#include <glm/common.hpp>
#include <glm/geometric.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <numeric>
#include <ranges>

namespace interaction
{
namespace
{
constexpr float kEpsilon = 1.0e-6f;
constexpr std::array<std::array<std::size_t, 2>, 12> kBoxEdges{{
  {0, 1},
  {0, 2},
  {0, 4},
  {1, 3},
  {1, 5},
  {2, 3},
  {2, 6},
  {3, 7},
  {4, 5},
  {4, 6},
  {5, 7},
  {6, 7},
}};

bool finite(const float value)
{
  return std::isfinite(value);
}

bool finite(const glm::vec3& value)
{
  return finite(value.x) && finite(value.y) && finite(value.z);
}

bool finite(const glm::quat& value)
{
  return finite(value.w) && finite(value.x) && finite(value.y) && finite(value.z);
}

bool finite(const std::array<glm::vec3, 8>& corners)
{
  return std::ranges::all_of(corners, [](const glm::vec3& corner) { return finite(corner); });
}

TransformationGuidePresentation presentation(const bool dragging, const float opacity)
{
  return {.dragging = dragging, .opacity = opacity};
}

glm::vec3 perpendicularDirection(const glm::vec3& axis)
{
  const glm::vec3 helperAxis = std::abs(axis.x) < 0.75f ? glm::vec3{1.0f, 0.0f, 0.0f} : glm::vec3{0.0f, 1.0f, 0.0f};
  return glm::normalize(glm::cross(axis, helperAxis));
}
} // namespace

void TransformationGuideState::beginTranslation(const uuids::uuid& sourceUid, const glm::vec3& startWorld)
{
  if (!finite(startWorld)) {
    clear();
    return;
  }

  m_guide = TranslationGuide{.startWorld = startWorld, .displacementWorld = glm::vec3{0.0f}, .presentation = {}};
  m_sourceViewUid = sourceUid;
  m_dragging = true;
  m_finishedAt.reset();
}

void TransformationGuideState::appendTranslation(const glm::vec3& appliedWorldDelta)
{
  auto* guideData = m_guide ? std::get_if<TranslationGuide>(&*m_guide) : nullptr;
  if (!m_dragging || !guideData || !finite(appliedWorldDelta)) {
    return;
  }
  guideData->displacementWorld += appliedWorldDelta;
}

void TransformationGuideState::beginRotation(
  const uuids::uuid& sourceUid,
  const glm::vec3& centerWorld,
  const glm::vec3& referenceWorld)
{
  if (!finite(centerWorld) || !finite(referenceWorld)) {
    clear();
    return;
  }

  m_guide = RotationGuide{
    .centerWorld = centerWorld,
    .referenceWorld = referenceWorld,
    .rotationWorld = glm::quat{1.0f, 0.0f, 0.0f, 0.0f},
    .presentation = {}};
  m_sourceViewUid = sourceUid;
  m_dragging = true;
  m_finishedAt.reset();
}

void TransformationGuideState::appendRotation(const glm::quat& appliedWorldRotation)
{
  auto* guideData = m_guide ? std::get_if<RotationGuide>(&*m_guide) : nullptr;
  const float length = glm::length(appliedWorldRotation);
  if (!m_dragging || !guideData || !finite(appliedWorldRotation) || !finite(length) || length <= kEpsilon) {
    return;
  }

  guideData->rotationWorld = glm::normalize(appliedWorldRotation * guideData->rotationWorld);
}

void TransformationGuideState::beginScale(
  const uuids::uuid& sourceUid,
  const glm::vec3& centerWorld,
  const glm::vec3& pointerStartWorld,
  const glm::vec3& initialScale,
  const std::array<glm::vec3, 8>& initialWorldCorners)
{
  if (
    !finite(centerWorld) || !finite(pointerStartWorld) || !finite(initialScale) || !finite(initialWorldCorners) ||
    glm::any(glm::lessThanEqual(initialScale, glm::vec3{kEpsilon})))
  {
    clear();
    return;
  }

  m_guide = ScaleGuide{
    .centerWorld = centerWorld,
    .pointerStartWorld = pointerStartWorld,
    .pointerCurrentWorld = pointerStartWorld,
    .initialScale = initialScale,
    .currentScale = initialScale,
    .initialWorldCorners = initialWorldCorners,
    .currentWorldCorners = initialWorldCorners,
    .presentation = {}};
  m_sourceViewUid = sourceUid;
  m_dragging = true;
  m_finishedAt.reset();
}

void TransformationGuideState::updateScale(
  const glm::vec3& pointerCurrentWorld,
  const glm::vec3& currentScale,
  const std::array<glm::vec3, 8>& currentWorldCorners)
{
  auto* guideData = m_guide ? std::get_if<ScaleGuide>(&*m_guide) : nullptr;
  if (
    !m_dragging || !guideData || !finite(pointerCurrentWorld) || !finite(currentScale) ||
    !finite(currentWorldCorners) || glm::any(glm::lessThanEqual(currentScale, glm::vec3{kEpsilon})))
  {
    return;
  }
  guideData->pointerCurrentWorld = pointerCurrentWorld;
  guideData->currentScale = currentScale;
  guideData->currentWorldCorners = currentWorldCorners;
}

void TransformationGuideState::finish(const TimePoint now)
{
  if (!m_guide || !m_dragging) {
    return;
  }
  m_dragging = false;
  m_finishedAt = now;
}

void TransformationGuideState::clear()
{
  m_guide.reset();
  m_sourceViewUid.reset();
  m_dragging = false;
  m_finishedAt.reset();
}

std::optional<TransformationGuide> TransformationGuideState::guide(const TimePoint now) const
{
  if (!m_guide) {
    return std::nullopt;
  }

  float opacity = 1.0f;
  if (m_finishedAt) {
    const auto elapsed = now - *m_finishedAt;
    if (elapsed >= FadeDuration) {
      return std::nullopt;
    }
    if (elapsed > Clock::duration::zero()) {
      const float elapsedSeconds = std::chrono::duration<float>(elapsed).count();
      const float fadeSeconds = std::chrono::duration<float>(FadeDuration).count();
      opacity = std::clamp(1.0f - elapsedSeconds / fadeSeconds, 0.0f, 1.0f);
    }
  }

  TransformationGuide result = *m_guide;
  std::visit([state = presentation(m_dragging, opacity)](auto& guideData) { guideData.presentation = state; }, result);
  return result;
}

const std::optional<uuids::uuid>& TransformationGuideState::sourceViewUid() const
{
  return m_sourceViewUid;
}

bool guideIsDragging(const TransformationGuide& guide)
{
  return std::visit([](const auto& guideData) { return guideData.presentation.dragging; }, guide);
}

std::array<glm::vec3, 3> translationComponentEndpoints(const TranslationGuide& guide)
{
  return {
    guide.startWorld + glm::vec3{guide.displacementWorld.x, 0.0f, 0.0f},
    guide.startWorld + glm::vec3{0.0f, guide.displacementWorld.y, 0.0f},
    guide.startWorld + glm::vec3{0.0f, 0.0f, guide.displacementWorld.z}};
}

RotationAxisAngle rotationAxisAngle(const RotationGuide& guide)
{
  glm::quat rotation = guide.rotationWorld;
  const float length = glm::length(rotation);
  if (!finite(rotation) || !finite(length) || length <= kEpsilon) {
    return {};
  }

  rotation = glm::normalize(rotation);
  if (rotation.w < 0.0f) {
    rotation = -rotation;
  }

  const float sinHalfAngle = glm::length(glm::vec3{rotation.x, rotation.y, rotation.z});
  if (sinHalfAngle <= kEpsilon) {
    return {};
  }

  return {
    .axisWorld = glm::vec3{rotation.x, rotation.y, rotation.z} / sinHalfAngle,
    .angleRadians = 2.0f * std::atan2(sinHalfAngle, std::clamp(rotation.w, 0.0f, 1.0f))};
}

std::vector<glm::vec3> rotationArcWorldPoints(const RotationGuide& guide, const std::size_t segmentCount)
{
  const RotationAxisAngle axisAngle = rotationAxisAngle(guide);
  const glm::vec3 reference = guide.referenceWorld - guide.centerWorld;
  const float referenceLength = glm::length(reference);

  glm::vec3 radial = reference - glm::dot(reference, axisAngle.axisWorld) * axisAngle.axisWorld;
  const float radialLength = glm::length(radial);
  if (!finite(radial) || !finite(radialLength) || radialLength <= kEpsilon) {
    const float fallbackRadius = finite(referenceLength) && referenceLength > kEpsilon ? referenceLength : 1.0f;
    radial = fallbackRadius * perpendicularDirection(axisAngle.axisWorld);
  }

  const std::size_t segments = std::max<std::size_t>(segmentCount, 1);
  std::vector<glm::vec3> points;
  points.reserve(segments + 1);
  for (std::size_t i = 0; i <= segments; ++i) {
    const float t = static_cast<float>(i) / static_cast<float>(segments);
    const glm::quat rotation = glm::angleAxis(t * axisAngle.angleRadians, axisAngle.axisWorld);
    points.push_back(guide.centerWorld + rotation * radial);
  }
  return points;
}

glm::vec3 relativeScaleFactors(const ScaleGuide& guide)
{
  glm::vec3 factors{1.0f};
  for (int axis = 0; axis < 3; ++axis) {
    if (
      finite(guide.initialScale[axis]) && std::abs(guide.initialScale[axis]) > kEpsilon &&
      finite(guide.currentScale[axis]))
    {
      factors[axis] = guide.currentScale[axis] / guide.initialScale[axis];
    }
  }
  return factors;
}

std::vector<glm::vec3> boxPlaneIntersectionOutline(
  const std::array<glm::vec3, 8>& worldCorners,
  const glm::vec3& planeOriginWorld,
  const glm::vec3& planeNormalWorld)
{
  const float normalLength = glm::length(planeNormalWorld);
  if (!finite(worldCorners) || !finite(planeOriginWorld) || !finite(planeNormalWorld) || normalLength <= kEpsilon) {
    return {};
  }

  const glm::vec3 normal = planeNormalWorld / normalLength;
  const float maximumEdgeLength =
    std::accumulate(kBoxEdges.begin(), kBoxEdges.end(), 0.0f, [&worldCorners](const float maximum, const auto& edge) {
      return std::max(maximum, glm::length(worldCorners[edge[1]] - worldCorners[edge[0]]));
    });
  const float tolerance = std::max(kEpsilon, maximumEdgeLength * 1.0e-5f);

  std::vector<glm::vec3> intersections;
  intersections.reserve(6);
  const auto addUnique = [&](const glm::vec3& point) {
    if (std::ranges::none_of(intersections, [&](const glm::vec3& existing) {
          return glm::length(point - existing) <= tolerance;
        }))
    {
      intersections.push_back(point);
    }
  };

  for (const auto& edge : kBoxEdges) {
    const glm::vec3& start = worldCorners[edge[0]];
    const glm::vec3& end = worldCorners[edge[1]];
    const float startDistance = glm::dot(start - planeOriginWorld, normal);
    const float endDistance = glm::dot(end - planeOriginWorld, normal);
    const bool startOnPlane = std::abs(startDistance) <= tolerance;
    const bool endOnPlane = std::abs(endDistance) <= tolerance;
    if (startOnPlane) {
      addUnique(start);
    }
    if (endOnPlane) {
      addUnique(end);
    }
    if (!startOnPlane && !endOnPlane && std::signbit(startDistance) != std::signbit(endDistance)) {
      const float t = startDistance / (startDistance - endDistance);
      addUnique(start + t * (end - start));
    }
  }

  if (intersections.size() < 3) {
    return {};
  }

  const glm::vec3 centroid = std::accumulate(intersections.begin(), intersections.end(), glm::vec3{0.0f}) /
                             static_cast<float>(intersections.size());

  const glm::vec3 axisU = perpendicularDirection(normal);
  const glm::vec3 axisV = glm::cross(normal, axisU);
  std::ranges::sort(intersections, [&](const glm::vec3& lhs, const glm::vec3& rhs) {
    const glm::vec3 lhsRelative = lhs - centroid;
    const glm::vec3 rhsRelative = rhs - centroid;
    return std::atan2(glm::dot(lhsRelative, axisV), glm::dot(lhsRelative, axisU)) <
           std::atan2(glm::dot(rhsRelative, axisV), glm::dot(rhsRelative, axisU));
  });
  return intersections;
}

} // namespace interaction
