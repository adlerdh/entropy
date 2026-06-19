#include "logic/app/ImageScaleInteraction.h"

#include <glm/ext/matrix_transform.hpp>
#include <glm/geometric.hpp>
#include <glm/gtc/epsilon.hpp>
#include <glm/vec2.hpp>

#include <array>
#include <cmath>

namespace entropy::app
{
namespace
{

constexpr float kMinScale = 0.1f;
constexpr float kMaxScale = 10.0f;
constexpr float kEpsilon = 1.0e-5f;

glm::vec3 transformPoint(const glm::mat4& transform, const glm::vec3& point)
{
  const glm::vec4 transformed = transform * glm::vec4{point, 1.0f};
  return glm::vec3{transformed / transformed.w};
}

glm::vec3 linearColumn(const glm::mat4& transform, int column)
{
  return glm::vec3{transform[column]};
}

std::optional<glm::vec2> solve2x2(float a00, float a01, float a10, float a11, const glm::vec2& b)
{
  const float det = (a00 * a11) - (a01 * a10);
  if (std::abs(det) <= kEpsilon) {
    return std::nullopt;
  }

  return glm::vec2{((b.x * a11) - (a01 * b.y)) / det, ((a00 * b.y) - (b.x * a10)) / det};
}

int axisMostAlignedWith(const glm::mat4& transform, const glm::vec3& worldAxis)
{
  int bestAxis = 0;
  float bestAlignment = -1.0f;

  for (int axis = 0; axis < 3; ++axis) {
    const glm::vec3 column = linearColumn(transform, axis);
    const float length = glm::length(column);
    if (length <= kEpsilon) {
      continue;
    }

    const float alignment = std::abs(glm::dot(glm::normalize(column), glm::normalize(worldAxis)));
    if (alignment > bestAlignment) {
      bestAlignment = alignment;
      bestAxis = axis;
    }
  }

  return bestAxis;
}

bool validScale(const glm::vec3& scale)
{
  return glm::all(glm::greaterThanEqual(scale, glm::vec3{kMinScale})) &&
         glm::all(glm::lessThanEqual(scale, glm::vec3{kMaxScale}));
}

} // namespace

std::optional<ImageScaleUpdate> computeImageScaleUpdate(
  const glm::mat4& worldDef_T_affine,
  const glm::vec3& currentScale,
  const glm::vec3& fixedWorldCenter,
  const glm::vec3& previousWorldPointer,
  const glm::vec3& currentWorldPointer,
  const glm::vec3& viewRight,
  const glm::vec3& viewUp,
  const glm::vec3& viewFront,
  bool constrainIsotropic)
{
  if (!validScale(currentScale)) {
    return std::nullopt;
  }

  const glm::mat4 affine_T_worldDef = glm::inverse(worldDef_T_affine);
  const glm::vec3 affineCenter = transformPoint(affine_T_worldDef, fixedWorldCenter);
  const glm::vec3 affinePointer = transformPoint(affine_T_worldDef, previousWorldPointer);
  const glm::vec3 affineVector = affinePointer - affineCenter;

  glm::vec3 scaleDelta{1.0f};

  if (constrainIsotropic) {
    const glm::vec2 previousPlane{
      glm::dot(previousWorldPointer - fixedWorldCenter, glm::normalize(viewRight)),
      glm::dot(previousWorldPointer - fixedWorldCenter, glm::normalize(viewUp))};
    const glm::vec2 currentPlane{
      glm::dot(currentWorldPointer - fixedWorldCenter, glm::normalize(viewRight)),
      glm::dot(currentWorldPointer - fixedWorldCenter, glm::normalize(viewUp))};

    const float previousDistance = glm::length(previousPlane);
    if (previousDistance <= kEpsilon) {
      return std::nullopt;
    }

    scaleDelta = glm::vec3{glm::length(currentPlane) / previousDistance};
  }
  else {
    const int lockedAxis = axisMostAlignedWith(worldDef_T_affine, viewFront);
    const std::array<int, 2> freeAxes = (lockedAxis == 0)   ? std::array<int, 2>{1, 2}
                                        : (lockedAxis == 1) ? std::array<int, 2>{0, 2}
                                                            : std::array<int, 2>{0, 1};

    const glm::vec3 target = currentWorldPointer - fixedWorldCenter;
    const glm::vec3 lockedContribution = linearColumn(worldDef_T_affine, lockedAxis) * affineVector[lockedAxis];
    const glm::vec3 rhsWorld = target - lockedContribution;

    const glm::vec3 col0 = linearColumn(worldDef_T_affine, freeAxes[0]) * affineVector[freeAxes[0]];
    const glm::vec3 col1 = linearColumn(worldDef_T_affine, freeAxes[1]) * affineVector[freeAxes[1]];

    const auto solved = solve2x2(
      glm::dot(viewRight, col0),
      glm::dot(viewRight, col1),
      glm::dot(viewUp, col0),
      glm::dot(viewUp, col1),
      glm::vec2{glm::dot(viewRight, rhsWorld), glm::dot(viewUp, rhsWorld)});

    if (!solved) {
      return std::nullopt;
    }

    scaleDelta[freeAxes[0]] = solved->x;
    scaleDelta[freeAxes[1]] = solved->y;
  }

  const glm::vec3 newScale = currentScale * scaleDelta;
  if (!validScale(newScale)) {
    return std::nullopt;
  }

  const glm::mat4 newLinear = glm::mat4{
    glm::vec4{linearColumn(worldDef_T_affine, 0) * scaleDelta.x, 0.0f},
    glm::vec4{linearColumn(worldDef_T_affine, 1) * scaleDelta.y, 0.0f},
    glm::vec4{linearColumn(worldDef_T_affine, 2) * scaleDelta.z, 0.0f},
    glm::vec4{0.0f, 0.0f, 0.0f, 1.0f}};

  const glm::vec3 newTranslation = fixedWorldCenter - glm::vec3{newLinear * glm::vec4{affineCenter, 0.0f}};
  return ImageScaleUpdate{.m_scale = newScale, .m_translation = newTranslation};
}

} // namespace entropy::app
