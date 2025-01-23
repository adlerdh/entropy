#include "rendering/utility/math/SliceIntersector.h"
#include "logic/camera/MathUtility.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_inverse.hpp>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/norm.hpp>

SliceIntersector::SliceIntersector()
  : m_positioningMethod(intersection::PositioningMethod::FrameOrigin)
  , m_alignmentMethod(intersection::AlignmentMethod::CameraZ)
  ,

  m_cameraSliceOffset(0.0f, 0.0f, -1.0f)
  , m_userSlicePosition(0.0f, 0.0f, 0.0f)
  , m_userSliceNormal(1.0f, 0.0f, 0.0f)
  ,

  m_modelPlaneEquation(1.0f, 0.0f, 0.0f, 0.0f)
{
}

void SliceIntersector::setPositioningMethod(
  const intersection::PositioningMethod& method, const std::optional<glm::vec3>& p
)
{
  m_positioningMethod = method;

  if (!p)
    return;

  switch (m_positioningMethod)
  {
  case intersection::PositioningMethod::UserDefined:
  {
    m_userSlicePosition = *p;
    break;
  }
  case intersection::PositioningMethod::OffsetFromCamera:
  {
    m_cameraSliceOffset = *p;
    break;
  }
  case intersection::PositioningMethod::FrameOrigin:
    break;
  }
}

void SliceIntersector::setAlignmentMethod(
  const intersection::AlignmentMethod& method, const std::optional<glm::vec3>& worldNormal
)
{
  m_alignmentMethod = method;

  if (intersection::AlignmentMethod::UserDefined == method)
  {
    if (worldNormal)
    {
      if (0.0f < glm::length2(*worldNormal))
      {
        m_userSliceNormal = glm::normalize(*worldNormal);
      }
    }
  }
}

std::pair<std::optional<intersection::IntersectionVertices>, glm::vec4>
SliceIntersector::computePlaneIntersections(
  const glm::mat4& model_O_camera,
  const glm::mat4& model_O_frame,
  const std::array<glm::vec3, 8>& modelBoxCorners
)
{
  updatePlaneEquation(model_O_camera, model_O_frame);

  return std::make_pair(
    math::computeAABBoxPlaneIntersections<float>(modelBoxCorners, m_modelPlaneEquation),
    m_modelPlaneEquation
  );
}

const intersection::PositioningMethod& SliceIntersector::positioningMethod() const
{
  return m_positioningMethod;
}

const intersection::AlignmentMethod& SliceIntersector::alignmentMethod() const
{
  return m_alignmentMethod;
}

void SliceIntersector::updatePlaneEquation(
  const glm::mat4& model_T_camera, const glm::mat4& model_T_frame
)
{
  glm::vec3 position{0.0f};
  glm::vec3 normal{0.0f, 0.0f, 1.0f};

  switch (m_positioningMethod)
  {
  case intersection::PositioningMethod::OffsetFromCamera:
  {
    const glm::vec4 p = model_T_camera * glm::vec4{m_cameraSliceOffset, 1.0f};
    position = glm::vec3{p / p.w};
    break;
  }
  case intersection::PositioningMethod::FrameOrigin:
  {
    const glm::vec4 p = model_T_frame[3];
    position = glm::vec3{p / p.w};
    break;
  }
  case intersection::PositioningMethod::UserDefined:
  default:
  {
    position = m_userSlicePosition;
    break;
  }
  }

  switch (m_alignmentMethod)
  {
  case intersection::AlignmentMethod::CameraZ:
  default:
  {
    normal = glm::vec3(glm::inverseTranspose(model_T_camera)[2]);
    break;
  }
  case intersection::AlignmentMethod::FrameX:
  {
    normal = glm::vec3(glm::inverseTranspose(model_T_frame)[0]);
    break;
  }
  case intersection::AlignmentMethod::FrameY:
  {
    normal = glm::vec3(glm::inverseTranspose(model_T_frame)[1]);
    break;
  }
  case intersection::AlignmentMethod::FrameZ:
  {
    normal = glm::vec3(glm::inverseTranspose(model_T_frame)[2]);
    break;
  }
  case intersection::AlignmentMethod::UserDefined:
  {
    normal = m_userSliceNormal;
    break;
  }
  }

  m_modelPlaneEquation = math::makePlane(glm::normalize(normal), position);
}
