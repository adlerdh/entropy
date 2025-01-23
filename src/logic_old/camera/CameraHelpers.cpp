#include "logic/camera/CameraHelpers.h"

#include "common/CoordinateFrame.h"
#include "common/Viewport.h"

#include "logic/camera/Camera.h"
#include "logic/camera/OrthogonalProjection.h"
#include "logic/camera/PerspectiveProjection.h"

#include "rendering/utility/math/MathUtility.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_inverse.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/intersect.hpp>
#include <glm/gtx/matrix_cross_product.hpp>
#include <glm/gtx/norm.hpp>
#include <glm/gtx/orthonormalize.hpp>
#include <glm/gtx/rotate_normalized_axis.hpp>
#include <glm/gtx/string_cast.hpp>
#include <glm/gtx/transform.hpp>
#include <glm/gtx/vector_angle.hpp>

#include <algorithm>
#include <cmath>
#include <functional>

namespace
{

static const glm::quat sk_unitRot(1.0f, 0.0f, 0.0f, 0.0f);
static const float sk_eps = glm::epsilon<float>();

} // namespace

/// @todo Differentiate names of functions that return mat4 transformations
/// (a_O_b) from those that actually do the transforming (tx_a_O_b)?

namespace camera
{

//void doExtra( Camera& camera, const CoordinateFrame& frame )
//{
//    glm::mat4 Q = glm::toMat4( frame.world_O_frame_rotation() );
//    glm::mat4 Qinv = glm::inverse( Q );
//    glm::mat4 T = glm::translate( glm::mat4{1.0f}, frame.worldOrigin() );
//    glm::mat4 Tinv = glm::inverse( T );
//    camera.setExtra( T * Qinv * Tinv * camera.extra() );
//}

std::unique_ptr<Projection> createCameraProjection(const ProjectionType& projectionType)
{
  switch (projectionType)
  {
  case ProjectionType::Orthographic:
  {
    return std::make_unique<OrthographicProjection>();
  }
  case ProjectionType::Perspective:
  {
    return std::make_unique<PerspectiveProjection>();
  }
  }

  return std::make_unique<OrthographicProjection>();
}

glm::mat4 clip_O_world(const Camera& camera)
{
  return camera.clip_O_camera() * camera.camera_O_world();
}

glm::mat4 world_O_clip(const Camera& camera)
{
  return camera.world_O_camera() * camera.camera_O_clip();
}

glm::vec3 worldOrigin(const Camera& camera)
{
  const glm::vec4 origin = camera.world_O_camera()[3];
  return glm::vec3(origin / origin.w);
}

glm::vec3 worldDirection(const Camera& camera, const Directions::View& dir)
{
  const glm::mat3 M = glm::inverseTranspose(glm::mat3{camera.world_O_camera()});
  return glm::normalize(M * Directions::get(dir));
}

glm::vec3 worldDirection(const CoordinateFrame& frame, const Directions::Cartesian& dir)
{
  const glm::mat3 w_O_f = glm::inverseTranspose(glm::mat3{frame.world_O_frame()});
  return glm::normalize(w_O_f * Directions::get(dir));
}

glm::vec3 cameraDirectionOfAnatomy(const Camera& camera, const Directions::Anatomy& dir)
{
  const glm::mat3 M = glm::inverseTranspose(glm::mat3{camera.camera_O_world()});
  return glm::normalize(M * Directions::get(dir));
}

glm::vec3 world_O_ndc(const Camera& camera, const glm::vec3& ndcPos)
{
  const glm::vec4 worldPos = world_O_clip(camera) * glm::vec4{ndcPos, 1.0f};
  return glm::vec3{worldPos / worldPos.w};
}

glm::vec3 ndc_O_camera(const Camera& camera, const glm::vec3& cameraPos)
{
  const glm::vec4 ndcPos = camera.clip_O_camera() * glm::vec4{cameraPos, 1.0f};
  return glm::vec3{ndcPos / ndcPos.w};
}

glm::vec3 camera_O_world(const Camera& camera, const glm::vec3& worldPos)
{
  const glm::vec4 cameraPos = camera.camera_O_world() * glm::vec4{worldPos, 1.0f};
  return glm::vec3{cameraPos / cameraPos.w};
}

glm::vec3 ndc_O_world(const Camera& camera, const glm::vec3& worldPos)
{
  const glm::vec4 ndcPos = clip_O_world(camera) * glm::vec4{worldPos, 1.0f};
  return glm::vec3{ndcPos / ndcPos.w};
}

glm::vec3 worldRayDirection(const Camera& camera, const glm::vec2& ndcRay)
{
  const glm::vec3 worldNearPos = world_O_ndc(camera, glm::vec3{ndcRay, -1.0f});
  const glm::vec3 worldFarPos = world_O_ndc(camera, glm::vec3{ndcRay, 1.0f});
  return glm::normalize(worldFarPos - worldNearPos);
}

glm::vec3 cameraRayDirection(const Camera& camera, const glm::vec2& ndcRay)
{
  const glm::vec3 cameraNearPos = camera_O_ndc(camera, glm::vec3{ndcRay, -1.0f});
  const glm::vec3 cameraFarPos = camera_O_ndc(camera, glm::vec3{ndcRay, 1.0f});
  return glm::normalize(cameraFarPos - cameraNearPos);
}

float ndcZofWorldPoint(const Camera& camera, const glm::vec3& worldPos)
{
  glm::vec4 clipPos = clip_O_world(camera) * glm::vec4{worldPos, 1.0f};
  return clipPos.z / clipPos.w;
}

float ndcZofWorldPoint_v2(const Camera& camera, const glm::vec3& worldPoint)
{
  const glm::vec3 v = worldOrigin(camera) - worldPoint;
  const float d = glm::length(v)
                  * glm::sign(glm::dot(v, worldDirection(camera, Directions::View::Back)));

  return 2.0f * (1.0f / d - 1.0f / camera.nearDistance())
           / (1.0f / camera.farDistance() - 1.0f / camera.nearDistance())
         - 1.0f;
}

float ndcZOfCameraDistance(const Camera& camera, const float cameraDistance)
{
  return 2.0f * (1.0f / cameraDistance - 1.0f / camera.nearDistance())
           / (1.0f / camera.farDistance() - 1.0f / camera.nearDistance())
         - 1.0f;
}

void applyViewTransformation(Camera& camera, const glm::mat4& m)
{
  camera.set_camera_O_frameB(m * camera.camera_O_frameB());
}

void resetViewTransformation(Camera& camera)
{
  static const glm::mat4 sk_identity(1.0f);
  camera.set_camera_O_frameB(sk_identity);
}

void resetZoom(Camera& camera)
{
  static constexpr float sk_defaultZoom = 1.0f;
  camera.setZoom(sk_defaultZoom);
}

void translateAboutCamera(Camera& camera, const Directions::View& dir, float distance)
{
  translateAboutCamera(camera, distance * Directions::get(dir));
}

void translateAboutCamera(Camera& camera, const glm::vec3& cameraVec)
{
  applyViewTransformation(camera, glm::translate(-cameraVec));
}

void panRelativeToWorldPosition(
  Camera& camera, const glm::vec2& ndcOldPos, const glm::vec2& ndcNewPos, const glm::vec3& worldPos
)
{
  const float ndcZ = ndcZofWorldPoint(camera, worldPos);
  const float flip = (ndcZ >= 1.0f) ? -1.0f : 1.0f;

  glm::vec4 oldCameraPos = camera.camera_O_clip() * glm::vec4{ndcOldPos, ndcZ, 1.0f};
  glm::vec4 newCameraPos = camera.camera_O_clip() * glm::vec4{ndcNewPos, ndcZ, 1.0f};

  oldCameraPos /= oldCameraPos.w;
  newCameraPos /= newCameraPos.w;

  const glm::vec3 delta = flip * glm::vec3(oldCameraPos - newCameraPos);
  translateAboutCamera(camera, delta);
}

void rotateAboutOrigin(Camera& camera, const Directions::View& dir, float angleRadians)
{
  rotateAboutOrigin(camera, Directions::get(dir), angleRadians);
}

void rotateAboutOrigin(Camera& camera, const glm::vec3& cameraVec, float angleRadians)
{
  applyViewTransformation(camera, glm::rotate(angleRadians, cameraVec));
}

void rotate(
  Camera& camera, const Directions::View& eyeAxis, float angleRadians, const glm::vec3& cameraCenter
)
{
  rotate(camera, Directions::get(eyeAxis), angleRadians, cameraCenter);
}

void rotate(
  Camera& camera, const glm::vec3& cameraAxis, float angleRadians, const glm::vec3& cameraCenter
)
{
  translateAboutCamera(camera, cameraCenter);
  rotateAboutOrigin(camera, cameraAxis, -angleRadians);
  translateAboutCamera(camera, -cameraCenter);
}

void zoom(Camera& camera, float factor, const glm::vec2& cameraCenterPos)
{
  if (factor <= 0.0f)
  {
    return;
  }

  translateAboutCamera(camera, glm::vec3{(1.0f - 1.0f / factor) * cameraCenterPos, 0.0f});
  camera.setZoom(factor * camera.getZoom());
}

void reflectFront(Camera& camera, const glm::vec3& cameraCenter)
{
  rotate(camera, Directions::View::Up, glm::pi<float>(), cameraCenter);
}

void setCameraOrigin(Camera& camera, const glm::vec3& worldPos)
{
  const glm::vec3 cameraOrigin{camera.camera_O_world() * glm::vec4{worldPos, 1.0f}};
  applyViewTransformation(camera, glm::translate(-cameraOrigin));
}

void setWorldTarget(Camera& camera, const glm::vec3& worldPos, float targetDistance)
{
  const glm::vec3 front = worldDirection(camera, Directions::View::Front);
  setCameraOrigin(camera, worldPos - targetDistance * front);
}

void translateInOut(
  Camera& camera, const glm::vec2& ndcOldPos, const glm::vec2& ndcNewPos, float scale
)
{
  translateAboutCamera(camera, Directions::View::Front, scale * (ndcNewPos.y - ndcOldPos.y));
}

void rotateInPlane(Camera& camera, float angle, const glm::vec2& ndcRotationCenter)
{
  rotate(
    camera, Directions::View::Front, angle, camera_O_ndc(camera, glm::vec3{ndcRotationCenter, -1.0f})
  );
}

void rotateInPlane(
  Camera& camera,
  const glm::vec2& ndcOldPos,
  const glm::vec2& ndcNewPos,
  const glm::vec2& ndcRotationCenter
)
{
  if (glm::all(glm::epsilonEqual(ndcOldPos, ndcRotationCenter, sk_eps)) || glm::all(glm::epsilonEqual(ndcNewPos, ndcRotationCenter, sk_eps)))
  {
    return;
  }

  const glm::vec2 oldVec = glm::normalize(ndcOldPos - ndcRotationCenter);
  const glm::vec2 newVec = glm::normalize(ndcNewPos - ndcRotationCenter);

  rotateInPlane(camera, glm::orientedAngle(oldVec, newVec), ndcRotationCenter);
}

void rotateAboutCameraOrigin(Camera& camera, const glm::vec2& ndcOldPos, const glm::vec2& ndcNewPos)
{
  static const glm::vec3 sk_cameraOrigin(0.0f, 0.0f, 0.0f);

  //    const float z = std::exp( -camera.getZoom() );
  //    const float scale = 1.0f - ( 1.0f - z ) / ( 1.0f + 20.0f * z );

  // scale rotation angles, such that they are smaller at higher zoom values
  const float z = camera.getZoom();
  const float scale = 1.0f - z / std::sqrt(z * z + 5.0f);

  const glm::vec2 angles = scale * glm::pi<float>() * (ndcNewPos - ndcOldPos);

  rotate(camera, Directions::View::Down, angles.x, sk_cameraOrigin);
  rotate(camera, Directions::View::Right, angles.y, sk_cameraOrigin);
}

void rotateAboutWorldPoint(
  Camera& camera,
  const glm::vec2& ndcOldPos,
  const glm::vec2& ndcNewPos,
  const glm::vec3& worldRotationPos
)
{
  const glm::vec2 angles = glm::pi<float>() * (ndcNewPos - ndcOldPos);

  const glm::vec3 cameraRotationCenter = glm::vec3{
    camera.camera_O_world() * glm::vec4{worldRotationPos, 1.0f}
  };

  rotate(camera, Directions::View::Down, angles.x, cameraRotationCenter);
  rotate(camera, Directions::View::Right, angles.y, cameraRotationCenter);
}

void zoomNdc(Camera& camera, float factor, const glm::vec2& ndcCenterPos)
{
  zoom(camera, factor, glm::vec2{camera_O_ndc(camera, glm::vec3{ndcCenterPos, -1.0f})});
}

void zoomNdc(
  Camera& camera,
  const glm::vec2& ndcOldPos,
  const glm::vec2& ndcNewPos,
  const glm::vec2& ndcCenterPos
)
{
  const float factor = (ndcNewPos.y - ndcOldPos.y) / 2.0f + 1.0f;
  zoomNdc(camera, factor, ndcCenterPos);
}

void zoomNdcDelta(Camera& camera, float delta, const glm::vec2& ndcCenterPos)
{
  static constexpr float sk_scale = 1.0f;
  const float factor = (1.0f / (1.0f + std::exp(-delta)) - 0.5f) + 1.0f;
  zoomNdc(camera, sk_scale * factor, ndcCenterPos);
}

glm::vec3 camera_O_ndc(const Camera& camera, const glm::vec3& ndcPos)
{
  const glm::vec4 cameraCenter = camera.camera_O_clip() * glm::vec4{ndcPos, 1.0f};
  return glm::vec3{cameraCenter / cameraCenter.w};
}

float convertOpenGlDepthToNdc(float depth)
{
  /// @todo Depth range values should be queried from OpenGL
  static constexpr float sk_depthRangeNear = 0.0f;
  static constexpr float sk_depthRangeFar = 1.0f;
  static constexpr float sk_depthRange = sk_depthRangeFar - sk_depthRangeNear;

  return (2.0f * depth - sk_depthRangeNear - sk_depthRangeFar) / sk_depthRange;
}

glm::vec3 sphere_O_ndc(
  const Camera& camera, const glm::vec2& ndcPos, const glm::vec3& worldSphereCenter
)
{
  static constexpr float sk_ndcRadius = 1.0f;
  const glm::vec4 clipSphereCenter = clip_O_world(camera) * glm::vec4{worldSphereCenter, 1.0f};
  const glm::vec2 ndcSphereCenter = glm::vec2(clipSphereCenter) / clipSphereCenter.w;
  const glm::vec2 unitCirclePos = (ndcPos - ndcSphereCenter) / sk_ndcRadius;
  const float rSq = glm::length2(unitCirclePos);

  if (rSq < 1.0f)
  {
    return glm::vec3(unitCirclePos, 1.0f - rSq);
  }
  else
  {
    return glm::vec3(glm::normalize(unitCirclePos), 0.0f);
  }
}

glm::quat rotationAlongArc(
  const Camera& camera,
  const glm::vec2& ndcStartPos,
  const glm::vec2& ndcNewPos,
  const glm::vec3& worldSphereCenter
)
{
  static constexpr float sk_minAngle = 0.001f;

  const glm::vec3 sphereStartPos = sphere_O_ndc(camera, ndcStartPos, worldSphereCenter);
  const glm::vec3 sphereNewPos = sphere_O_ndc(camera, ndcNewPos, worldSphereCenter);

  const float angle = std::acos(glm::clamp(glm::dot(sphereStartPos, sphereNewPos), -1.0f, 1.0f));

  if (std::abs(angle) < sk_minAngle)
  {
    return sk_unitRot;
  }

  const glm::vec3 sphereAxis = glm::normalize(glm::cross(sphereStartPos, sphereNewPos));
  glm::vec3 worldAxis = glm::inverseTranspose(glm::mat3{camera.world_O_camera()}) * sphereAxis;

  return glm::rotateNormalizedAxis(sk_unitRot, angle, worldAxis);
}

glm::quat rotation2dInCameraPlane(
  const Camera& camera,
  const glm::vec2& ndcOldPos,
  const glm::vec2& ndcNewPos,
  const glm::vec2& ndcRotationCenter
)
{
  if (glm::all(glm::epsilonEqual(ndcOldPos, ndcRotationCenter, sk_eps)) || glm::all(glm::epsilonEqual(ndcNewPos, ndcRotationCenter, sk_eps)))
  {
    return sk_unitRot;
  }

  const glm::vec2 oldVec = glm::normalize(ndcOldPos - ndcRotationCenter);
  const glm::vec2 newVec = glm::normalize(ndcNewPos - ndcRotationCenter);

  const float angle = -1.0f * glm::orientedAngle(oldVec, newVec);
  const glm::mat3 w_O_c = inverseTranspose(glm::mat3{world_O_clip(camera)});

  return glm::quat_cast(glm::rotate(angle, w_O_c[2]));
}

glm::quat rotation3dAboutCameraPlane(
  const Camera& camera, const glm::vec2& ndcOldPos, const glm::vec2& ndcNewPos
)
{
  const glm::vec2 angles = glm::pi<float>() * (ndcNewPos - ndcOldPos);
  const glm::mat3 w_O_c = inverseTranspose(glm::mat3{world_O_clip(camera)});

  const glm::mat4 R_horiz = glm::rotate(-angles.y, w_O_c[0]);
  const glm::mat4 R_vert = glm::rotate(angles.x, w_O_c[1]);

  return glm::quat_cast(R_horiz * R_vert);
}

glm::vec3 translationInCameraPlane(
  const Camera& camera, const glm::vec2& ndcOldPos, const glm::vec2& ndcNewPos, float ndcZ
)
{
  // If the frame origin is behind the camera origin, then flip the
  // delta vector, so that we still translate in the correct direction
  const float flipSign = (ndcZ >= 1.0f) ? -1.0f : 1.0f;

  const glm::vec3 oldWorldPos = world_O_ndc(camera, glm::vec3{ndcOldPos, ndcZ});
  const glm::vec3 newWorldPos = world_O_ndc(camera, glm::vec3{ndcNewPos, ndcZ});

  return flipSign * glm::vec3(newWorldPos - oldWorldPos);
}

glm::vec3 translationAboutCameraFrontBack(
  const Camera& camera, const glm::vec2& ndcOldPos, const glm::vec2& ndcNewPos, float scale
)
{
  const float distance = scale * (ndcNewPos.y - ndcOldPos.y);
  const glm::vec3 front = worldDirection(camera, Directions::View::Front);
  return distance * front;
}

float axisTranslationAlongWorldAxis(
  const Camera& camera,
  const glm::vec2& ndcOldPos,
  const glm::vec2& ndcNewPos,
  float ndcZ,
  const glm::vec3& worldAxis
)
{
  const glm::vec3 oldWorldPos = world_O_ndc(camera, glm::vec3{ndcOldPos, ndcZ});
  const glm::vec3 newWorldPos = world_O_ndc(camera, glm::vec3{ndcNewPos, ndcZ});

  return glm::dot(glm::normalize(worldAxis), newWorldPos - oldWorldPos);
}

float rotationAngleAboutWorldAxis(
  const Camera& camera,
  const glm::vec2& ndcOldPos,
  const glm::vec2& ndcNewPos,
  float ndcZ,
  const glm::vec3& worldRotationAxis,
  const glm::vec3& worldRotationCenter
)
{
  const glm::vec3 oldWorldPos = world_O_ndc(camera, glm::vec3{ndcOldPos, ndcZ});
  const glm::vec3 newWorldPos = world_O_ndc(camera, glm::vec3{ndcNewPos, ndcZ});

  const glm::vec3 worldAxisNorm = glm::normalize(worldRotationAxis);

  const glm::vec3 centerToOld = glm::normalize(oldWorldPos - worldRotationCenter);
  const glm::vec3 centerToNew = glm::normalize(newWorldPos - worldRotationCenter);

  return glm::degrees(glm::orientedAngle(centerToOld, centerToNew, worldAxisNorm));
}

glm::vec2 scaleFactorsAboutWorldAxis(
  const Camera& camera,
  const glm::vec2& ndcOldPos,
  const glm::vec2& ndcNewPos,
  float ndcZ,
  const glm::mat4& slide_O_world,
  const glm::vec3& slideRotationCenter
)
{
  const glm::vec4 a = slide_O_world * world_O_clip(camera) * glm::vec4{ndcOldPos, ndcZ, 1.0f};
  const glm::vec4 b = slide_O_world * world_O_clip(camera) * glm::vec4{ndcNewPos, ndcZ, 1.0f};

  const glm::vec3 slideOldPos = glm::vec3(a / a.w);
  const glm::vec3 slideNewPos = glm::vec3(b / b.w);

  static const glm::vec3 sk_slideAxis(0.0f, 0.0f, 1.0f);

  // Projections onto slide:
  const glm::vec3 projSlideOldPos = slideOldPos
                                    - glm::dot(sk_slideAxis, slideOldPos) * sk_slideAxis;
  const glm::vec3 projSlideNewPos = slideNewPos
                                    - glm::dot(sk_slideAxis, slideNewPos) * sk_slideAxis;

  // Vectors from center:
  const glm::vec2 numer = glm::vec2(projSlideNewPos - slideRotationCenter);
  const glm::vec2 denom = glm::vec2(projSlideOldPos - slideRotationCenter);

  static const glm::vec2 sk_zero(0.0f, 0.0f);

  if (glm::any(glm::equal(denom, sk_zero)))
  {
    static const glm::vec2 sk_ident(1.0f, 1.0f);
    return sk_ident;
  }

  return glm::vec2(numer.x / denom.x, numer.y / denom.y);
}

glm::vec2 worldViewportDimensions(const Camera& camera, float ndcZ)
{
  static const glm::vec3 ndcLeftPos(-1.0f, 0.0f, ndcZ);
  static const glm::vec3 ndcRightPos(1.0f, 0.0f, ndcZ);
  static const glm::vec3 ndcBottomPos(0.0f, -1.0f, ndcZ);
  static const glm::vec3 ndcTopPos(0.0f, 1.0f, ndcZ);

  const glm::vec3 worldLeftPos = world_O_ndc(camera, ndcLeftPos);
  const glm::vec3 worldRightPos = world_O_ndc(camera, ndcRightPos);
  const glm::vec3 worldBottomPos = world_O_ndc(camera, ndcBottomPos);
  const glm::vec3 worldTopPos = world_O_ndc(camera, ndcTopPos);

  const float width = glm::length(worldRightPos - worldLeftPos);
  const float height = glm::length(worldTopPos - worldBottomPos);

  return glm::vec2{width, height};
}

glm::vec3 worldTranslationPerpendicularToWorldAxis(
  const Camera& camera,
  const glm::vec2& ndcOldPos,
  const glm::vec2& ndcNewPos,
  float ndcZ,
  const glm::vec3& worldAxis
)
{
  const glm::vec3 oldWorldPos = world_O_ndc(camera, glm::vec3{ndcOldPos, ndcZ});
  const glm::vec3 newWorldPos = world_O_ndc(camera, glm::vec3{ndcNewPos, ndcZ});

  const glm::vec3 worldDeltaVec = newWorldPos - oldWorldPos;

  // Projection of worldDeltaVec along worldAxis:
  const glm::vec3 worldAxisNorm = glm::normalize(worldAxis);
  const glm::vec3 worldProjVec = glm::dot(worldAxisNorm, worldDeltaVec) * worldAxisNorm;

  // Return the vector rejection:
  return worldDeltaVec - worldProjVec;
}

glm::vec4 ndc_O_view(const Viewport& viewport, const glm::vec2& viewPos)
{
  return glm::vec4(
    2.0f * (viewPos.x - viewport.left()) / viewport.width() - 1.0f,
    2.0f * (viewPos.y - viewport.bottom()) / viewport.height() - 1.0f,
    -1.0f,
    1.0f
  );
}

glm::vec2 ndc2d_O_view(const Viewport& viewport, const glm::vec2& viewPos)
{
  return glm::vec2(
    2.0f * (viewPos.x - viewport.left()) / viewport.width() - 1.0f,
    2.0f * (viewPos.y - viewport.bottom()) / viewport.height() - 1.0f
  );
}

glm::vec2 viewDevice_O_ndc(const Viewport& viewport, const glm::vec2& ndcPos)
{
  return viewport.devicePixelRatio() * view_O_ndc(viewport, ndcPos);
}

glm::vec2 view_O_ndc(const Viewport& viewport, const glm::vec2& ndcPos)
{
  return glm::vec2(
    (ndcPos.x + 1.0f) * viewport.width() / 2.0f + viewport.left(),
    (ndcPos.y + 1.0f) * viewport.height() / 2.0f + viewport.bottom()
  );
}

glm::vec2 view_O_mouse(const Viewport& viewport, const glm::vec2& mousePos)
{
  return glm::vec2(viewport.left() + mousePos.x, viewport.bottom() + viewport.height() - mousePos.y);
}

glm::vec4 ndc_O_mouse(const Viewport& viewport, const glm::vec2& mousePos)
{
  return ndc_O_view(viewport, view_O_mouse(viewport, mousePos));
}

glm::vec2 ndc2d_O_mouse(const Viewport& viewport, const glm::vec2& mousePos)
{
  return ndc2d_O_view(viewport, view_O_mouse(viewport, mousePos));
}

glm::mat4 get_ndc_O_view(const Viewport& viewport)
{
  const glm::vec4 k_scaleX(2.0f / viewport.width(), 0.0f, 0.0f, 0.0f);
  const glm::vec4 k_scaleY(0.0f, 2.0f / viewport.height(), 0.0f, 0.0f);
  const glm::vec4 k_scaleZ(0.0f, 0.0f, 1.0f, 0.0f);

  const glm::vec4 k_translation(
    -2.0f * viewport.left() / viewport.width() - 1.0f,
    -2.0f * viewport.bottom() / viewport.height() - 1.0f,
    -1.0f,
    1.0f
  );

  return glm::mat4(k_scaleX, k_scaleY, k_scaleZ, k_translation);
}

std::optional<glm::vec3> worldCameraPlaneIntersection(
  const Camera& camera, const glm::vec2& ndcRayPos, const glm::vec3& worldPlanePos
)
{
  static constexpr float sk_ndcNearPlane = -1.0f;

  const glm::vec3 worldPlaneNormal = worldDirection(camera, Directions::View::Back);
  const glm::vec3 worldRayPos = world_O_ndc(camera, glm::vec3{ndcRayPos, sk_ndcNearPlane});
  const glm::vec3 worldRayDir = worldRayDirection(camera, ndcRayPos);

  float intersectionDistance;

  bool intersected = glm::intersectRayPlane(
    worldRayPos, worldRayDir, worldPlanePos, worldPlaneNormal, intersectionDistance
  );

  if (intersected)
  {
    return worldRayPos + intersectionDistance * worldRayDir;
  }
  else
  {
    return std::nullopt;
  }
}

void positionCameraForWorldTargetAndFov(
  Camera& camera, const glm::vec3& worldBoxSize, const glm::vec3& worldTarget
)
{
  // Camera target is image bounding box center.
  // FOV at focal plane equals maximum reference space bounding box size.
  // Set Camera origin back by twice the bounding box diameter.

  const float fov = glm::compMax(worldBoxSize);
  const float diameter = glm::length(worldBoxSize);

  // Minimum distance to avoid clipping the image
  const float minDistance = glm::length(0.5f * worldBoxSize);

  float distance = 0.0f;

  if (camera.isOrthographic())
  {
    distance = 2.0f * minDistance;
    camera.setDefaultFov(glm::vec2{fov, fov});
  }
  else
  {
    distance = std::max(0.5f * fov / std::tan(camera.angle()), minDistance);
  }

  setWorldTarget(camera, worldTarget, distance);

  camera.setFarDistance(distance + diameter);
}

std::array<glm::vec3, 8> worldFrustumCorners(const Camera& camera)
{
  static const std::array<glm::vec3, 8> sk_ndCorners = {{
    {
      1.0f,
      1.0f,
      -1.0f,
    },
    {
      -1.0f,
      1.0f,
      -1.0f,
    },
    {
      -1.0f,
      -1.0f,
      -1.0f,
    },
    {
      1.0f,
      -1.0f,
      -1.0f,
    },
    {
      1.0f,
      1.0f,
      1.0f,
    },
    {
      -1.0f,
      1.0f,
      1.0f,
    },
    {
      -1.0f,
      -1.0f,
      1.0f,
    },
    {
      1.0f,
      -1.0f,
      1.0f,
    },
  }};

  std::array<glm::vec3, 8> worldCorners;

  std::transform(
    std::begin(sk_ndCorners),
    std::end(sk_ndCorners),
    std::begin(worldCorners),
    std::bind(&world_O_ndc, std::ref(camera), std::placeholders::_1)
  );

  return worldCorners;
}

std::array<glm::vec4, 6> worldFrustumPlanes(const Camera& camera)
{
  const auto C = worldFrustumCorners(camera);

  std::array<glm::vec3, 6> normals;
  normals[0] = glm::normalize(glm::cross(C[7] - C[0], C[4] - C[0]));
  normals[1] = glm::normalize(glm::cross(C[4] - C[0], C[5] - C[0]));
  normals[2] = glm::normalize(glm::cross(C[5] - C[1], C[6] - C[1]));
  normals[3] = glm::normalize(glm::cross(C[6] - C[2], C[7] - C[2]));
  normals[4] = glm::normalize(glm::cross(C[1] - C[0], C[3] - C[0]));
  normals[5] = glm::normalize(glm::cross(C[7] - C[4], C[5] - C[4]));

  std::array<glm::vec3, 6> p;
  p[0] = (C[0] + C[3] + C[4] + C[7]) / 4.0f;
  p[1] = (C[0] + C[1] + C[4] + C[5]) / 4.0f;
  p[2] = (C[1] + C[2] + C[5] + C[6]) / 4.0f;
  p[3] = (C[2] + C[3] + C[6] + C[7]) / 4.0f;
  p[4] = (C[0] + C[1] + C[2] + C[3]) / 4.0f;
  p[5] = (C[4] + C[5] + C[6] + C[7]) / 4.0f;

  std::array<glm::vec4, 6> planes;

  for (uint i = 0; i < 6; ++i)
  {
    planes[i] = math::makePlane(normals[i], p[i]);
  }

  return planes;
}

glm::vec4 world_O_view(
  const Viewport& viewport, const camera::Camera& camera, const glm::vec2& viewPos, float ndcZ
)
{
  /// @note Maybe replace ndcZ with focal distance in clip space?
  const glm::vec4 clipPos{ndc2d_O_view(viewport, viewPos), ndcZ, 1.0f};
  const glm::vec4 worldPos = camera.world_O_camera() * camera.camera_O_clip() * clipPos;
  return worldPos / worldPos.w;
}

/// @todo Make this function valid for perspective views, too!
/// Currently not valid for perspective projection.
glm::vec2 worldPixelSize(const Viewport& viewport, const camera::Camera& camera)
{
  static constexpr float nearPlaneZ = -1.0f;

  static const glm::vec2 viewO(0.0f, 0.0f);
  static const glm::vec2 viewX(1.0f, 0.0f);
  static const glm::vec2 viewY(0.0f, 1.0f);

  const glm::vec4 worldViewO = world_O_view(viewport, camera, viewO, nearPlaneZ);
  const glm::vec4 worldViewX = world_O_view(viewport, camera, viewX, nearPlaneZ);
  const glm::vec4 worldViewY = world_O_view(viewport, camera, viewY, nearPlaneZ);

  return glm::vec2{glm::length(worldViewX - worldViewO), glm::length(worldViewY - worldViewO)};
}

// This version of the function is valid for both orthogonal and perspective projections
glm::vec2 worldPixelSizeAtWorldPosition(
  const Viewport& viewport, const camera::Camera& camera, const glm::vec3& worldPos
)
{
  static const glm::vec2 viewX(1.0f, 0.0f);
  static const glm::vec2 viewY(0.0f, 1.0f);

  const glm::vec3 ndcPos = ndc_O_world(camera, worldPos);

  const glm::vec2 viewPosO = view_O_ndc(viewport, glm::vec2{ndcPos});
  const glm::vec2 viewPosX = viewPosO + viewX;
  const glm::vec2 viewPosY = viewPosO + viewY;

  const glm::vec4 worldViewO = world_O_view(viewport, camera, viewPosO, ndcPos.z);
  const glm::vec4 worldViewX = world_O_view(viewport, camera, viewPosX, ndcPos.z);
  const glm::vec4 worldViewY = world_O_view(viewport, camera, viewPosY, ndcPos.z);

  return glm::vec2{glm::length(worldViewX - worldViewO), glm::length(worldViewY - worldViewO)};
}

float computeSmallestWorldDepthOffset(const camera::Camera& camera, const glm::vec3& worldPos)
{
  // Small epsilon in NDC space. Using a float32 depth buffer, as we do,
  // this value should be just large enough to differentiate depths
  static const glm::vec3 smallestNdcOffset{0.0f, 0.0f, -1.0e-5};

  const glm::vec3 ndcPos = ndc_O_world(camera, worldPos);
  const glm::vec3 worldPosOffset = world_O_ndc(camera, ndcPos + smallestNdcOffset);

  return glm::length(worldPos - worldPosOffset);
}

} // namespace camera
