#include "logic/camera/CameraHelpers.h"

#include "common/CoordinateFrame.h"
#include "common/Viewport.h"

#include "logic/camera/Camera.h"
#include "logic/camera/MathUtility.h"
#include "logic/camera/OrthogonalProjection.h"
#include "logic/camera/PerspectiveProjection.h"

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
/// (a_T_b) from those that actually do the transforming (tx_a_T_b)?

namespace camera
{

//void doExtra( Camera& camera, const CoordinateFrame& frame )
//{
//    glm::mat4 Q = glm::toMat4( frame.world_T_frame_rotation() );
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

glm::mat4 clip_T_world(const Camera& camera)
{
  return camera.clip_T_camera() * camera.camera_T_world();
}

glm::mat4 world_T_clip(const Camera& camera)
{
  return camera.world_T_camera() * camera.camera_T_clip();
}

glm::vec3 worldOrigin(const Camera& camera)
{
  const glm::vec4 origin = camera.world_T_camera()[3];
  return glm::vec3(origin / origin.w);
}

glm::vec3 worldDirection(const Camera& camera, const Directions::View& dir)
{
  const glm::mat3 M = glm::inverseTranspose(glm::mat3{camera.world_T_camera()});
  return glm::normalize(M * Directions::get(dir));
}

glm::vec3 worldDirection(const CoordinateFrame& frame, const Directions::Cartesian& dir)
{
  const glm::mat3 w_T_f = glm::inverseTranspose(glm::mat3{frame.world_T_frame()});
  return glm::normalize(w_T_f * Directions::get(dir));
}

glm::vec3 cameraDirectionOfAnatomy(const Camera& camera, const Directions::Anatomy& dir)
{
  const glm::mat3 M = glm::inverseTranspose(glm::mat3{camera.camera_T_world()});
  return glm::normalize(M * Directions::get(dir));
}

glm::vec3 cameraDirectionOfWorld(const Camera& camera, const Directions::Cartesian& dir)
{
  const glm::mat3 M = glm::inverseTranspose(glm::mat3{camera.camera_T_world()});
  return glm::normalize(M * Directions::get(dir));
}

glm::vec3 world_T_ndc(const Camera& camera, const glm::vec3& ndcPos)
{
  const glm::vec4 worldPos = world_T_clip(camera) * glm::vec4{ndcPos, 1.0f};
  return glm::vec3{worldPos / worldPos.w};
}

glm::vec3 ndc_T_camera(const Camera& camera, const glm::vec3& cameraPos)
{
  const glm::vec4 ndcPos = camera.clip_T_camera() * glm::vec4{cameraPos, 1.0f};
  return glm::vec3{ndcPos / ndcPos.w};
}

glm::vec3 camera_T_world(const Camera& camera, const glm::vec3& worldPos)
{
  const glm::vec4 cameraPos = camera.camera_T_world() * glm::vec4{worldPos, 1.0f};
  return glm::vec3{cameraPos / cameraPos.w};
}

glm::vec3 ndc_T_world(const Camera& camera, const glm::vec3& worldPos)
{
  const glm::vec4 ndcPos = clip_T_world(camera) * glm::vec4{worldPos, 1.0f};
  return glm::vec3{ndcPos / ndcPos.w};
}

glm::vec3 worldRayDirection(const Camera& camera, const glm::vec2& ndcRay)
{
  const glm::vec3 worldNearPos = world_T_ndc(camera, glm::vec3{ndcRay, -1.0f});
  const glm::vec3 worldFarPos = world_T_ndc(camera, glm::vec3{ndcRay, 1.0f});
  return glm::normalize(worldFarPos - worldNearPos);
}

glm::vec3 cameraRayDirection(const Camera& camera, const glm::vec2& ndcRay)
{
  const glm::vec3 cameraNearPos = camera_T_ndc(camera, glm::vec3{ndcRay, -1.0f});
  const glm::vec3 cameraFarPos = camera_T_ndc(camera, glm::vec3{ndcRay, 1.0f});
  return glm::normalize(cameraFarPos - cameraNearPos);
}

float ndcZofWorldPoint(const Camera& camera, const glm::vec3& worldPos)
{
  glm::vec4 clipPos = clip_T_world(camera) * glm::vec4{worldPos, 1.0f};
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
  camera.set_camera_T_anatomy(m * camera.camera_T_anatomy());
}

void applyViewRotationAboutWorldPoint(
  Camera& camera, const glm::quat& rotation, const glm::vec3& worldRotationPos
)
{
  const glm::vec3 cameraRotationCenter = glm::vec3{
    camera.camera_T_world() * glm::vec4{worldRotationPos, 1.0f}
  };

  translateAboutCamera(camera, cameraRotationCenter);
  applyViewTransformation(camera, glm::toMat4(rotation));
  translateAboutCamera(camera, -cameraRotationCenter);
}

void resetViewTransformation(Camera& camera)
{
  static const glm::mat4 sk_identity(1.0f);
  camera.set_camera_T_anatomy(sk_identity);
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

  glm::vec4 oldCameraPos = camera.camera_T_clip() * glm::vec4{ndcOldPos, ndcZ, 1.0f};
  glm::vec4 newCameraPos = camera.camera_T_clip() * glm::vec4{ndcNewPos, ndcZ, 1.0f};

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
  const glm::vec3 cameraOrigin{camera.camera_T_world() * glm::vec4{worldPos, 1.0f}};
  applyViewTransformation(camera, glm::translate(-cameraOrigin));
}

void setWorldTarget(
  Camera& camera, const glm::vec3& worldPos, const std::optional<float>& targetDistance
)
{
  // By default, push camera back from its target on the view plane by a distance equal to
  // 10% of the view frustum depth, so that it doesn't clip the image quad vertices:
  static constexpr float sk_pushBackFraction = 0.10f;

  const float eyeToTargetOffset = targetDistance
                                    ? *targetDistance
                                    : sk_pushBackFraction
                                        * (camera.farDistance() - camera.nearDistance());

  const glm::vec3 front = worldDirection(camera, Directions::View::Front);
  setCameraOrigin(camera, worldPos - eyeToTargetOffset * front);
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
    camera, Directions::View::Front, angle, camera_T_ndc(camera, glm::vec3{ndcRotationCenter, -1.0f})
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
    camera.camera_T_world() * glm::vec4{worldRotationPos, 1.0f}
  };

  rotate(camera, Directions::View::Down, angles.x, cameraRotationCenter);
  rotate(camera, Directions::View::Right, angles.y, cameraRotationCenter);
}

void zoomNdc(Camera& camera, float factor, const glm::vec2& ndcCenterPos)
{
  zoom(camera, factor, glm::vec2{camera_T_ndc(camera, glm::vec3{ndcCenterPos, -1.0f})});
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

glm::vec3 camera_T_ndc(const Camera& camera, const glm::vec3& ndcPos)
{
  const glm::vec4 cameraCenter = camera.camera_T_clip() * glm::vec4{ndcPos, 1.0f};
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

glm::vec3 sphere_T_ndc(
  const Camera& camera, const glm::vec2& ndcPos, const glm::vec3& worldSphereCenter
)
{
  static constexpr float sk_ndcRadius = 1.0f;
  const glm::vec4 clipSphereCenter = clip_T_world(camera) * glm::vec4{worldSphereCenter, 1.0f};
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

  const glm::vec3 sphereStartPos = sphere_T_ndc(camera, ndcStartPos, worldSphereCenter);
  const glm::vec3 sphereNewPos = sphere_T_ndc(camera, ndcNewPos, worldSphereCenter);

  const float angle = std::acos(glm::clamp(glm::dot(sphereStartPos, sphereNewPos), -1.0f, 1.0f));

  if (std::abs(angle) < sk_minAngle)
  {
    return sk_unitRot;
  }

  const glm::vec3 sphereAxis = glm::normalize(glm::cross(sphereStartPos, sphereNewPos));
  glm::vec3 worldAxis = glm::inverseTranspose(glm::mat3{camera.world_T_camera()}) * sphereAxis;

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
  const glm::mat3 w_T_c = inverseTranspose(glm::mat3{world_T_clip(camera)});

  return glm::quat_cast(glm::rotate(angle, w_T_c[2]));
}

glm::quat rotation3dAboutCameraPlane(
  const Camera& camera, const glm::vec2& ndcOldPos, const glm::vec2& ndcNewPos
)
{
  const glm::vec2 angles = glm::pi<float>() * (ndcNewPos - ndcOldPos);
  const glm::mat3 w_T_c = inverseTranspose(glm::mat3{world_T_clip(camera)});

  const glm::mat4 R_horiz = glm::rotate(-angles.y, w_T_c[0]);
  const glm::mat4 R_vert = glm::rotate(angles.x, w_T_c[1]);

  return glm::quat_cast(R_horiz * R_vert);
}

glm::vec3 translationInCameraPlane(
  const Camera& camera, const glm::vec2& ndcOldPos, const glm::vec2& ndcNewPos, float ndcZ
)
{
  // If the frame origin is behind the camera origin, then flip the
  // delta vector, so that we still translate in the correct direction
  const float flipSign = (ndcZ >= 1.0f) ? -1.0f : 1.0f;

  const glm::vec3 oldWorldPos = world_T_ndc(camera, glm::vec3{ndcOldPos, ndcZ});
  const glm::vec3 newWorldPos = world_T_ndc(camera, glm::vec3{ndcNewPos, ndcZ});

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
  const glm::vec3 oldWorldPos = world_T_ndc(camera, glm::vec3{ndcOldPos, ndcZ});
  const glm::vec3 newWorldPos = world_T_ndc(camera, glm::vec3{ndcNewPos, ndcZ});

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
  const glm::vec3 oldWorldPos = world_T_ndc(camera, glm::vec3{ndcOldPos, ndcZ});
  const glm::vec3 newWorldPos = world_T_ndc(camera, glm::vec3{ndcNewPos, ndcZ});

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
  const glm::mat4& slide_T_world,
  const glm::vec3& slideRotationCenter
)
{
  const glm::vec4 a = slide_T_world * world_T_clip(camera) * glm::vec4{ndcOldPos, ndcZ, 1.0f};
  const glm::vec4 b = slide_T_world * world_T_clip(camera) * glm::vec4{ndcNewPos, ndcZ, 1.0f};

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

  if (glm::any(glm::epsilonEqual(denom, sk_zero, glm::epsilon<float>())))
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

  const glm::vec3 worldLeftPos = world_T_ndc(camera, ndcLeftPos);
  const glm::vec3 worldRightPos = world_T_ndc(camera, ndcRightPos);
  const glm::vec3 worldBottomPos = world_T_ndc(camera, ndcBottomPos);
  const glm::vec3 worldTopPos = world_T_ndc(camera, ndcTopPos);

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
  const glm::vec3 oldWorldPos = world_T_ndc(camera, glm::vec3{ndcOldPos, ndcZ});
  const glm::vec3 newWorldPos = world_T_ndc(camera, glm::vec3{ndcNewPos, ndcZ});

  const glm::vec3 worldDeltaVec = newWorldPos - oldWorldPos;

  // Projection of worldDeltaVec along worldAxis:
  const glm::vec3 worldAxisNorm = glm::normalize(worldAxis);
  const glm::vec3 worldProjVec = glm::dot(worldAxisNorm, worldDeltaVec) * worldAxisNorm;

  // Return the vector rejection:
  return worldDeltaVec - worldProjVec;
}

glm::vec2 windowNdc_T_window(const Viewport& windowViewport, const glm::vec2& windowPixelPos)
{
  return glm::vec2(
    2.0f * (windowPixelPos.x - windowViewport.left()) / windowViewport.width() - 1.0f,
    2.0f * (windowPixelPos.y - windowViewport.bottom()) / windowViewport.height() - 1.0f
  );
}

glm::vec2 viewDevice_T_ndc(const Viewport& viewport, const glm::vec2& ndcPos)
{
  return viewport.devicePixelRatio() * window_T_windowClip(viewport, ndcPos);
}

glm::vec2 window_T_windowClip(const Viewport& viewport, const glm::vec2& ndcPos)
{
  return glm::vec2(
    (ndcPos.x + 1.0f) * viewport.width() / 2.0f + viewport.left(),
    (ndcPos.y + 1.0f) * viewport.height() / 2.0f + viewport.bottom()
  );
}

glm::vec2 viewport_T_windowClip(const Viewport& windowViewport, const glm::vec2& windowClipPos)
{
  return glm::vec2(
    (windowClipPos.x + 1.0f) * windowViewport.width() / 2.0f,
    (windowClipPos.y + 1.0f) * windowViewport.height() / 2.0f
  );
}

glm::vec2 windowClip_T_viewport(const Viewport& windowViewport, const glm::vec2& viewportPos)
{
  return glm::vec2(
    (2.0f * viewportPos.x / windowViewport.width() - 1.0f),
    (2.0f * viewportPos.y / windowViewport.height() - 1.0f)
  );
}

glm::mat4 window_T_windowClip(const Viewport& viewport)
{
  return glm::mat4{
    glm::vec4{viewport.width() / 2.0f, 0.0f, 0.0f, 0.0f},
    glm::vec4{0.0f, viewport.height() / 2.0f, 0.0f, 0.0f},
    glm::vec4{0.0f, 0.0f, 1.0f, 0.0f},
    glm::vec4{
      viewport.left() + viewport.width() / 2.0f,
      viewport.bottom() + viewport.height() / 2.0f,
      1.0f,
      1.0f
    }
  };
}

glm::mat4 viewport_T_windowClip(const Viewport& windowViewport)
{
  return glm::mat4{
    glm::vec4{windowViewport.width() / 2.0f, 0.0f, 0.0f, 0.0f},
    glm::vec4{0.0f, windowViewport.height() / 2.0f, 0.0f, 0.0f},
    glm::vec4{0.0f, 0.0f, 1.0f, 0.0f},
    glm::vec4{windowViewport.width() / 2.0f, windowViewport.height() / 2.0f, 1.0f, 1.0f}
  };
}

glm::vec2 window_T_mindow(float wholeWindowHeight, const glm::vec2& mousePos)
{
  return glm::vec2(mousePos.x, wholeWindowHeight - mousePos.y);
}

glm::mat4 window_T_mindow(float wholeWindowHeight)
{
  return glm::mat4{
    glm::vec4{1.0f, 0.0f, 0.0f, 0.0f},
    glm::vec4{0.0f, -1.0f, 0.0f, 0.0f},
    glm::vec4{0.0f, 0.0f, 1.0f, 0.0f},
    glm::vec4{0.0f, wholeWindowHeight, 0.0f, 1.0f}
  };
}

glm::mat4 mindow_T_window(float wholeWindowHeight)
{
  return window_T_mindow(wholeWindowHeight);
}

glm::vec2 miewport_T_viewport(float viewportHeight, const glm::vec2& viewPos)
{
  return glm::vec2(viewPos.x, viewportHeight - viewPos.y);
}

glm::vec2 viewport_T_miewport(float viewportHeight, const glm::vec2& viewPos)
{
  return glm::vec2(viewPos.x, viewportHeight - viewPos.y);
}

glm::mat4 miewport_T_viewport(float viewportHeight)
{
  return glm::mat4{
    glm::vec4{1.0f, 0.0f, 0.0f, 0.0f},
    glm::vec4{0.0f, -1.0f, 0.0f, 0.0f},
    glm::vec4{0.0f, 0.0f, 1.0f, 0.0f},
    glm::vec4{0.0f, viewportHeight, 0.0f, 1.0f}
  };
}

std::optional<glm::vec3> worldCameraPlaneIntersection(
  const Camera& camera, const glm::vec2& ndcRayPos, const glm::vec3& worldPlanePos
)
{
  static constexpr float sk_ndcNearPlane = -1.0f;

  const glm::vec3 worldPlaneNormal = worldDirection(camera, Directions::View::Back);
  const glm::vec3 worldRayPos = world_T_ndc(camera, glm::vec3{ndcRayPos, sk_ndcNearPlane});
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
  const auto [pullBackDistance, farDistance] = computePullbackAndFarDistances(camera, worldBoxSize);

  if (camera.isOrthographic())
  {
    const float fov = glm::compMax(worldBoxSize);
    camera.setDefaultFov(glm::vec2{fov, fov});
  }

  camera.setFarDistance(farDistance);
  setWorldTarget(camera, worldTarget, pullBackDistance);
}

void positionCameraForWorldTarget(
  Camera& camera, const glm::vec3& worldBoxSize, const glm::vec3& worldTarget
)
{
  const auto [pullBackDistance, farDistance] = computePullbackAndFarDistances(camera, worldBoxSize);
  camera.setFarDistance(farDistance);
  setWorldTarget(camera, worldTarget, pullBackDistance);
}

void orientCameraToWorldTargetNormalDirection(
  Camera& camera, const glm::vec3& targetWorldNormalDirection
)
{
  static const glm::vec3 cameraToVector{0, 0, 1};

  const glm::vec3 cameraFromVector = glm::inverseTranspose(glm::mat3{camera.camera_T_world()})
                                     * glm::normalize(targetWorldNormalDirection);

  applyViewTransformation(camera, math::fromToRotation(cameraFromVector, cameraToVector));
}

void setWorldForwardDirection(camera::Camera& camera, const glm::vec3& worldForwardDirection)
{
  static const CoordinateFrame IDENT;

  static constexpr float sk_lengthThresh = 1e-3f;
  static constexpr float sk_angleThresh_degrees = 45.0f;

  static const glm::vec3 sk_worldDesiredUp_superior = Directions::get(Directions::Anatomy::Superior);
  static const glm::vec3 sk_worldDesiredUp_anterior = Directions::get(Directions::Anatomy::Anterior);

  if (glm::length(worldForwardDirection) < sk_lengthThresh)
  {
    return;
  }

  const glm::vec3 worldBack = glm::normalize(-worldForwardDirection);

  // Select the desired up vector based on the worldBack direction:
  // If worldBack is parallel to the superior direction,
  // then worldRight = anterior X worldBack;
  // otherwise, worldRight = superior X worldBack.

  const glm::vec3 worldDesiredUp
    = (camera::areVectorsParallel(worldBack, sk_worldDesiredUp_superior, sk_angleThresh_degrees))
        ? sk_worldDesiredUp_anterior
        : sk_worldDesiredUp_superior;

  const glm::vec3 worldRight = glm::normalize(glm::cross(worldDesiredUp, worldBack));
  const glm::vec3 worldUp = glm::normalize(glm::cross(worldBack, worldRight));

  const glm::mat4 anatomy_T_camera{
    glm::vec4{worldRight, 0}, glm::vec4{worldUp, 0}, glm::vec4{worldBack, 0}, glm::vec4{0, 0, 0, 1}
  };

  camera.set_anatomy_T_start_provider([]() { return IDENT; });
  camera.set_camera_T_anatomy(glm::inverse(anatomy_T_camera));
}

std::pair<float, float> computePullbackAndFarDistances(
  const Camera& camera, const glm::vec3& worldBoxSize
)
{
  // Camera target is image bounding box center.
  // FOV at focal plane equals maximum reference space bounding box size.
  // Set Camera origin back by twice the bounding box diameter.

  const float fov = glm::compMax(worldBoxSize);
  const float diameter = glm::length(worldBoxSize);

  // Minimum distance to avoid clipping the image
  const float minDistance = glm::length(0.5f * worldBoxSize);

  float pullBackDistance = 0.0f;

  if (camera.isOrthographic())
  {
    pullBackDistance = 2.0f * minDistance;
  }
  else
  {
    pullBackDistance = std::max(0.5f * fov / std::tan(camera.angle()), minDistance);
  }

  const float farDistance = pullBackDistance + diameter;

  return {pullBackDistance, farDistance};
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
    std::bind(&world_T_ndc, std::ref(camera), std::placeholders::_1)
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

  for (uint32_t i = 0; i < 6; ++i)
  {
    planes[i] = math::makePlane(normals[i], p[i]);
  }

  return planes;
}

glm::vec4 world_T_view(
  const Viewport& viewport, const camera::Camera& camera, const glm::vec2& viewPos, float ndcZ
)
{
  /// @note Maybe replace ndcZ with focal distance in clip space?
  const glm::vec4 clipPos{windowNdc_T_window(viewport, viewPos), ndcZ, 1.0f};
  const glm::vec4 worldPos = camera.world_T_camera() * camera.camera_T_clip() * clipPos;
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

  const glm::vec4 worldViewO = world_T_view(viewport, camera, viewO, nearPlaneZ);
  const glm::vec4 worldViewX = world_T_view(viewport, camera, viewX, nearPlaneZ);
  const glm::vec4 worldViewY = world_T_view(viewport, camera, viewY, nearPlaneZ);

  return glm::vec2{glm::length(worldViewX - worldViewO), glm::length(worldViewY - worldViewO)};
}

// This version of the function is valid for both orthogonal and perspective projections
glm::vec2 worldPixelSizeAtWorldPosition(
  const Viewport& viewport, const camera::Camera& camera, const glm::vec3& worldPos
)
{
  static const glm::vec2 viewX(1.0f, 0.0f);
  static const glm::vec2 viewY(0.0f, 1.0f);

  const glm::vec3 ndcPos = ndc_T_world(camera, worldPos);

  const glm::vec2 viewPosO = window_T_windowClip(viewport, glm::vec2{ndcPos});
  const glm::vec2 viewPosX = viewPosO + viewX;
  const glm::vec2 viewPosY = viewPosO + viewY;

  const glm::vec4 worldViewO = world_T_view(viewport, camera, viewPosO, ndcPos.z);
  const glm::vec4 worldViewX = world_T_view(viewport, camera, viewPosX, ndcPos.z);
  const glm::vec4 worldViewY = world_T_view(viewport, camera, viewPosY, ndcPos.z);

  return glm::vec2{glm::length(worldViewX - worldViewO), glm::length(worldViewY - worldViewO)};
}

float computeSmallestWorldDepthOffset(const camera::Camera& camera, const glm::vec3& worldPos)
{
  // Small epsilon in NDC space. Using a float32 depth buffer, as we do,
  // this value should be just large enough to differentiate depths
  static const glm::vec3 smallestNdcOffset{0.0f, 0.0f, -1.0e-5};

  const glm::vec3 ndcPos = ndc_T_world(camera, worldPos);
  const glm::vec3 worldPosOffset = world_T_ndc(camera, ndcPos + smallestNdcOffset);

  return glm::length(worldPos - worldPosOffset);
}

glm::vec2 miewport_T_world(
  const Viewport& windowVP,
  const camera::Camera& camera,
  const glm::mat4& windowClip_T_viewClip,
  const glm::vec3& worldPos
)
{
  const glm::vec4 winClipPos = windowClip_T_viewClip * clip_T_world(camera)
                               * glm::vec4{worldPos, 1.0f};

  const glm::vec2 viewportPos = viewport_T_windowClip(windowVP, glm::vec2{winClipPos / winClipPos.w});

  return miewport_T_viewport(windowVP.height(), viewportPos);
}

glm::vec3 world_T_miewport(
  const Viewport& windowVP,
  const camera::Camera& camera,
  const glm::mat4& viewClip_T_windowClip,
  const glm::vec2& miewportPos
)
{
  static constexpr float sk_nearPlaneClip = -1.0f;

  const glm::vec2 viewportPos = viewport_T_miewport(windowVP.height(), miewportPos);
  const glm::vec2 winClipPos = windowClip_T_viewport(windowVP, viewportPos);

  const glm::vec4 worldPos = world_T_clip(camera) * viewClip_T_windowClip
                             * glm::vec4{winClipPos, sk_nearPlaneClip, 1.0f};

  return worldPos / worldPos.w;
}

glm::vec2 worldPixelSize(
  const Viewport& windowVP, const camera::Camera& camera, const glm::mat4& viewClip_T_windowClip
)
{
  static const glm::vec2 miewO(0.0f, 0.0f);
  static const glm::vec2 miewX(1.0f, 0.0f);
  static const glm::vec2 miewY(0.0f, 1.0f);

  const glm::vec3 worldO = world_T_miewport(windowVP, camera, viewClip_T_windowClip, miewO);
  const glm::vec3 worldX = world_T_miewport(windowVP, camera, viewClip_T_windowClip, miewX);
  const glm::vec3 worldY = world_T_miewport(windowVP, camera, viewClip_T_windowClip, miewY);

  return glm::vec2{glm::length(worldX - worldO), glm::length(worldY - worldO)};
}

glm::mat4 compute_windowClip_T_viewClip(const glm::vec4& windowClipViewport)
{
  const glm::vec3
    T{windowClipViewport[0] + 0.5f * windowClipViewport[2],
      windowClipViewport[1] + 0.5f * windowClipViewport[3],
      0.0f};

  const glm::vec3 S{0.5f * windowClipViewport[2], 0.5f * windowClipViewport[3], 1.0f};

  return glm::translate(T) * glm::scale(S);
}

glm::quat computeCameraRotationRelativeToWorld(const camera::Camera& camera)
{
  const glm::vec3 cameraX = cameraDirectionOfWorld(camera, Directions::Cartesian::X);
  const glm::vec3 cameraY = cameraDirectionOfWorld(camera, Directions::Cartesian::Y);
  const glm::vec3 cameraZ = cameraDirectionOfWorld(camera, Directions::Cartesian::Z);

  const glm::mat3 rotation_camera_T_world{cameraX, cameraY, cameraZ};
  return glm::quat_cast(rotation_camera_T_world);
}

FrameBounds computeMiewportFrameBounds(
  const glm::vec4& windowClipFrameViewport, const glm::vec4& windowViewport
)
{
  const glm::mat4 miewport_T_windowClip = camera::miewport_T_viewport(windowViewport[3])
                                          * camera::viewport_T_windowClip(windowViewport);

  const glm::vec4 winClipViewBL{windowClipFrameViewport[0], windowClipFrameViewport[1], 0.0f, 1.0f};

  const glm::vec4 winClipViewTR{
    windowClipFrameViewport[0] + windowClipFrameViewport[2],
    windowClipFrameViewport[1] + windowClipFrameViewport[3],
    0.0f,
    1.0f
  };

  const glm::vec2 miewportViewBL{miewport_T_windowClip * winClipViewBL};
  const glm::vec2 miewportViewTR{miewport_T_windowClip * winClipViewTR};

  return glm::vec4(
    miewportViewBL.x,                    // x offset
    miewportViewTR.y,                    // y offset
    miewportViewTR.x - miewportViewBL.x, // width
    miewportViewBL.y - miewportViewTR.y
  ); // height
}

FrameBounds computeMindowFrameBounds(
  const glm::vec4& windowClipFrameViewport, const glm::vec4& windowViewport, float wholeWindowHeight
)
{
  const glm::mat4 mindow_T_windowClip = camera::mindow_T_window(wholeWindowHeight)
                                        * camera::window_T_windowClip(windowViewport);

  const glm::vec4 winClipViewBL{windowClipFrameViewport[0], windowClipFrameViewport[1], 0.0f, 1.0f};

  const glm::vec4 winClipViewTR{
    windowClipFrameViewport[0] + windowClipFrameViewport[2],
    windowClipFrameViewport[1] + windowClipFrameViewport[3],
    0.0f,
    1.0f
  };

  const glm::vec2 mindowViewBL{mindow_T_windowClip * winClipViewBL};
  const glm::vec2 mindowViewTR{mindow_T_windowClip * winClipViewTR};

  return glm::vec4(
    mindowViewBL.x,                  // x offset
    mindowViewTR.y,                  // y offset
    mindowViewTR.x - mindowViewBL.x, // width
    mindowViewBL.y - mindowViewTR.y
  ); // height
}

bool looksAlongOrthogonalAxis(const camera::Camera& camera)
{
  const glm::vec3 frontDir = worldDirection(camera, Directions::View::Front);

  const float dotX = std::abs(glm::dot(frontDir, Directions::get(Directions::Cartesian::X)));
  const float dotY = std::abs(glm::dot(frontDir, Directions::get(Directions::Cartesian::Y)));
  const float dotZ = std::abs(glm::dot(frontDir, Directions::get(Directions::Cartesian::Z)));

  if (glm::epsilonEqual(dotX, 1.0f, sk_eps) || glm::epsilonEqual(dotY, 1.0f, sk_eps) || glm::epsilonEqual(dotZ, 1.0f, sk_eps))
  {
    return true;
  }

  return false;
}

bool areVectorsParallel(const glm::vec3& a, const glm::vec3& b, float angleThreshold_degrees)
{
  const float dotProdThreshold = 1.0f - std::cos(glm::radians(angleThreshold_degrees));

  if (std::abs(std::abs(glm::dot(a, b)) - 1.0f) > dotProdThreshold)
  {
    return false;
  }

  return true;
}

bool areViewDirectionsParallel(
  const camera::Camera& camera1,
  const camera::Camera& camera2,
  const Directions::View& dir,
  float angleThreshold_degrees
)
{
  return areVectorsParallel(
    worldDirection(camera1, dir), worldDirection(camera2, dir), angleThreshold_degrees
  );
}

} // namespace camera
