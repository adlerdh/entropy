#include "common/CoordinateFrame.h"
#include "common/Exception.hpp"

#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/gtc/epsilon.hpp>
#include <glm/gtc/matrix_inverse.hpp>
#include <glm/gtc/matrix_transform.hpp>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/norm.hpp>
#include <glm/gtx/orthonormalize.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/gtx/rotate_normalized_axis.hpp>
#include <glm/gtx/string_cast.hpp>
#include <glm/gtx/transform.hpp>
#include <glm/gtx/vector_angle.hpp>
#include <glm/gtx/vector_query.hpp>

#include <cmath>

namespace
{

bool isFinite(const glm::vec3& value) noexcept
{
  return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
}

bool isFinite(const glm::quat& value) noexcept
{
  return std::isfinite(value.w) && std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
}

glm::quat normalizedRotation(const glm::quat& rotation)
{
  const float magnitudeSquared = glm::dot(rotation, rotation);
  if (!isFinite(rotation) || !std::isfinite(magnitudeSquared) || magnitudeSquared <= glm::epsilon<float>()) {
    throwDebug("Coordinate frame rotation must be a finite, nonzero quaternion.");
  }
  return rotation * glm::inversesqrt(magnitudeSquared);
}

glm::vec3 normalizedAxis(const glm::vec3& axis, const char* description)
{
  const float magnitudeSquared = glm::dot(axis, axis);
  if (!isFinite(axis) || !std::isfinite(magnitudeSquared) || magnitudeSquared <= glm::epsilon<float>()) {
    throwDebug(description);
  }
  return axis * glm::inversesqrt(magnitudeSquared);
}

} // namespace

CoordinateFrame::CoordinateFrame(glm::vec3 worldOrigin, glm::quat world_T_frame_rotation)
  : m_worldFrameOrigin(worldOrigin)
{
  setFrameToWorldRotation(world_T_frame_rotation);
}

CoordinateFrame::CoordinateFrame(glm::vec3 worldOrigin, float angleDegress, const glm::vec3& worldAxis)
  : m_worldFrameOrigin(worldOrigin)
{
  setFrameToWorldRotation(angleDegress, worldAxis);
}

CoordinateFrame::CoordinateFrame(
  glm::vec3 worldOrigin,
  const glm::vec3& frameAxis1,
  const glm::vec3& worldAxis1,
  const glm::vec3& frameAxis2,
  const glm::vec3& worldAxis2)
  : m_worldFrameOrigin(worldOrigin)
{
  static constexpr bool k_requireEqualAngles = false;
  setFrameToWorldRotation(frameAxis1, worldAxis1, frameAxis2, worldAxis2, k_requireEqualAngles);
}

void CoordinateFrame::setWorldOrigin(glm::vec3 origin)
{
  m_worldFrameOrigin = origin;
}

void CoordinateFrame::setFrameToWorldRotation(glm::quat world_T_frame_rotationArg)
{
  m_world_T_frame_rotation = normalizedRotation(world_T_frame_rotationArg);
}

void CoordinateFrame::setFrameToWorldRotation(float angleDegrees, const glm::vec3& worldAxis)
{
  if (!std::isfinite(angleDegrees)) {
    throwDebug("Coordinate frame rotation angle must be finite.");
  }
  static const glm::quat sk_ident(1.0f, 0.0f, 0.0f, 0.0f);
  const glm::vec3 axis = normalizedAxis(worldAxis, "Coordinate frame rotation axis must be finite and nonzero.");
  setFrameToWorldRotation(glm::rotateNormalizedAxis(sk_ident, glm::radians(angleDegrees), axis));
}

void CoordinateFrame::setFrameToWorldRotation(
  const glm::vec3& frameAxis1,
  const glm::vec3& worldAxis1,
  const glm::vec3& frameAxis2,
  const glm::vec3& worldAxis2,
  bool requireEqualAngles)
{
  const glm::vec3 normalizedFrameAxis1 = normalizedAxis(frameAxis1, "First frame axis must be finite and nonzero.");
  const glm::vec3 normalizedFrameAxis2 = normalizedAxis(frameAxis2, "Second frame axis must be finite and nonzero.");
  const glm::vec3 normalizedWorldAxis1 = normalizedAxis(worldAxis1, "First world axis must be finite and nonzero.");
  const glm::vec3 normalizedWorldAxis2 = normalizedAxis(worldAxis2, "Second world axis must be finite and nonzero.");

  const float frameAngle = glm::angle(normalizedFrameAxis1, normalizedFrameAxis2);
  const float worldAngle = glm::angle(normalizedWorldAxis1, normalizedWorldAxis2);

  if (requireEqualAngles && !glm::epsilonEqual(frameAngle, worldAngle, glm::epsilon<float>())) {
    throwDebug("Angle between input frame and world axes are not equal.");
  }

  if (
    glm::length2(glm::cross(normalizedFrameAxis1, normalizedFrameAxis2)) <= glm::epsilon<float>() ||
    glm::length2(glm::cross(normalizedWorldAxis1, normalizedWorldAxis2)) <= glm::epsilon<float>())
  {
    throwDebug("Each axis pair must contain two non-collinear directions.");
  }

  const glm::mat3 frame_T_ident{
    normalizedFrameAxis1,
    normalizedFrameAxis2,
    glm::cross(normalizedFrameAxis1, normalizedFrameAxis2)};
  const glm::mat3 world_T_ident{
    normalizedWorldAxis1,
    normalizedWorldAxis2,
    glm::cross(normalizedWorldAxis1, normalizedWorldAxis2)};
  const glm::mat3 world_T_frameLocal = glm::orthonormalize(world_T_ident * glm::inverse(frame_T_ident));

  setFrameToWorldRotation(glm::quat_cast(world_T_frameLocal));
}

void CoordinateFrame::setIdentity()
{
  m_worldFrameOrigin = {0.0f, 0.0f, 0.0f};
  m_world_T_frame_rotation = {1.0f, 0.0f, 0.0f, 0.0f};
}

void CoordinateFrame::setIdentityRotation()
{
  m_world_T_frame_rotation = {1.0f, 0.0f, 0.0f, 0.0f};
}

glm::vec3 CoordinateFrame::worldOrigin() const
{
  return m_worldFrameOrigin;
}

glm::quat CoordinateFrame::world_T_frame_rotation() const
{
  return m_world_T_frame_rotation;
}

glm::mat4 CoordinateFrame::world_T_frame() const
{
  return glm::translate(m_worldFrameOrigin) * glm::toMat4(m_world_T_frame_rotation);
}

glm::mat4 CoordinateFrame::frame_T_world() const
{
  return glm::affineInverse(world_T_frame());
}

CoordinateFrame& CoordinateFrame::operator+=(const CoordinateFrame& rhs)
{
  return (*this = *this + rhs);
}

CoordinateFrame CoordinateFrame::operator+(const CoordinateFrame& rhs) const
{
  CoordinateFrame res;
  res.setWorldOrigin(worldOrigin() + rhs.worldOrigin());
  res.setFrameToWorldRotation(world_T_frame_rotation() * rhs.world_T_frame_rotation());
  return res;
}
