#ifndef CAMERA_HELPERS_H
#define CAMERA_HELPERS_H

#include "logic/camera/Projection.h"
#include "logic/utility/DirectionMaps.h"

#include <glm/fwd.hpp>

#include <array>
#include <memory>
#include <optional>

class CoordinateFrame;
class Viewport;

/**
 * Free functions defined for the Camera.
 */
namespace camera
{

class Camera;

/**
 * @brief Create a camera projection of a given type.
 * @param[in] type Type of camera projection
 * @return Unique pointer to camera projection
 */
std::unique_ptr<Projection> createCameraProjection(const ProjectionType& type);

/**
 * @brief Compute full model-view-projection transformation chain from World to OpenGL Clip space
 * for a given camera.
 */
glm::mat4 clip_O_world(const Camera& camera);

/**
 * @brief Compute inverse of full model-view-projection transformation chain from OpenGL Clip
 * to World space for a given camera.
 */
glm::mat4 world_O_clip(const Camera& camera);

/**
 * @brief Return the World-space origin position of a camera.
 */
glm::vec3 worldOrigin(const Camera& camera);

/**
 * @brief Return the Normalized World-space direction vector of a camera.
 */
glm::vec3 worldDirection(const Camera& camera, const Directions::View& dir);

/**
 * @brief Return the normalized World-space vector along a CoordinateFrame direction axis.
 * @todo Move this to another helper class for CoordinateFrame-specific logic
 */
glm::vec3 worldDirection(const CoordinateFrame&, const Directions::Cartesian& dir);

/**
 * @brief Return the normalized Camera-space vector of an anatomical direction.
 */
glm::vec3 cameraDirectionOfAnatomy(const Camera& camera, const Directions::Anatomy& dir);

/**
 * @brief World-space position of NDC point.
 * @param ndcPos NDC of point
 */
glm::vec3 world_O_ndc(const Camera&, const glm::vec3& ndcPos);

/**
 * @brief NDC of Camera-space point.
 * @param cameraPos Camera-space coordinates of point
 */
glm::vec3 ndc_O_camera(const Camera&, const glm::vec3& cameraPos);

glm::vec3 camera_O_ndc(const Camera&, const glm::vec3& ndcPos);

/**
 * @brief Camera-space position of World point.
 * @param worldPos World-space coordinates of point
 */
glm::vec3 camera_O_world(const Camera&, const glm::vec3& worldPos);

/**
 * @brief NDC position of world point.
 * @param worldPos World-space coordinates of point
 */
glm::vec3 ndc_O_world(const Camera&, const glm::vec3& worldPos);

/**
 * @brief World-space direction of ray emanating from NDC point
 * @param ndcPos NDC xy of ray origin
 */
glm::vec3 worldRayDirection(const Camera&, const glm::vec2& ndcRay);

/**
 * @brief Camera-space direction of ray emanating from NDC point
 * @param ndcPos NDC xy of ray origin
 */
glm::vec3 cameraRayDirection(const Camera&, const glm::vec2& ndcRay);

/**
 * @brief Apply a transformation to the camera relative to its start frame
 */
void applyViewTransformation(Camera&, const glm::mat4& m);

/**
 * @brief Reset the camera to its start frame orientation
 */
void resetViewTransformation(Camera&);

/**
 * @brief Reset the camera's zoom factor to default
 */
void resetZoom(Camera&);

/**
 * @brief Set the camera origin to a World position
 */
void setCameraOrigin(Camera&, const glm::vec3& worldPos);

/**
 * @brief Set the camera target to World position
 * @param worldPos World-space position
 * @param targetDistance Offset distance of camera backwards from position
 */
void setWorldTarget(Camera&, const glm::vec3& worldPos, float targetDistance);

void rotateAboutOrigin(Camera&, const glm::vec3& cameraVec, float angle);
void rotateAboutOrigin(Camera&, const Directions::View& dir, float angle);

void rotate(Camera&, const Directions::View& dir, float angle, const glm::vec3& cameraCenter);
void rotate(Camera&, const glm::vec3& cameraVec, float angle, const glm::vec3& cameraCenter);

void translateAboutCamera(Camera&, const Directions::View& dir, float distance);
void translateAboutCamera(Camera&, const glm::vec3& cameraVec);

void reflectFront(Camera&, const glm::vec3& cameraCenter);
void zoom(Camera&, float factor, const glm::vec2& cameraCenter);

void translateInOut(Camera&, const glm::vec2& ndcOldPos, const glm::vec2& ndcNewPos, float scale);

void panRelativeToWorldPosition(
  Camera&, const glm::vec2& ndcOldPos, const glm::vec2& ndcNewPos, const glm::vec3& worldPos
);

void rotateAboutCameraOrigin(Camera&, const glm::vec2& ndcOldPos, const glm::vec2& ndcNewPos);

void rotateAboutWorldPoint(
  Camera&, const glm::vec2& ndcOldPos, const glm::vec2& ndcNewPos, const glm::vec3& worldRotationPos
);

void rotateInPlane(
  Camera&, const glm::vec2& ndcOldPos, const glm::vec2& ndcNewPos, const glm::vec2& ndcRotationCenter
);

void rotateInPlane(Camera&, float angle, const glm::vec2& ndcRotationCenter);

void zoomNdc(
  Camera&, const glm::vec2& ndcOldPos, const glm::vec2& ndcNewPos, const glm::vec2& ndcCenterPos
);

void zoomNdc(Camera&, float magFactor, const glm::vec2& ndcCenterPos);

void zoomNdcDelta(Camera&, float delta, const glm::vec2& ndcCenterPos);

float ndcZofWorldPoint(const Camera&, const glm::vec3& worldPos);

float ndcZofWorldPoint_v2(const Camera&, const glm::vec3& worldPos);

float ndcZOfCameraDistance(const Camera&, const float cameraDistance);

float convertOpenGlDepthToNdc(float depth);

glm::vec3 sphere_O_ndc(const Camera&, const glm::vec2& ndcPos, const glm::vec3& worldSphereCenter);

glm::quat rotationAlongArc(
  const Camera&,
  const glm::vec2& ndcStartPos,
  const glm::vec2& ndcNewPos,
  const glm::vec3& worldSphereCenter
);

glm::quat rotation2dInCameraPlane(
  const Camera&,
  const glm::vec2& ndcOldPos,
  const glm::vec2& ndcNewPos,
  const glm::vec2& ndcRotationCenter = glm::vec2{0.0f, 0.0f}
);

glm::quat rotation3dAboutCameraPlane(
  const Camera&, const glm::vec2& ndcOldPos, const glm::vec2& ndcNewPos
);

glm::vec3 translationInCameraPlane(
  const Camera&, const glm::vec2& ndcOldPos, const glm::vec2& ndcNewPos, float ndcZ
);

glm::vec3 translationAboutCameraFrontBack(
  const Camera&, const glm::vec2& ndcOldPos, const glm::vec2& ndcNewPos, float scale
);

// Returns translation relative to the worldAxis
float axisTranslationAlongWorldAxis(
  const Camera&,
  const glm::vec2& ndcOldPos,
  const glm::vec2& ndcNewPos,
  float ndcZ,
  const glm::vec3& worldAxis
);

float rotationAngleAboutWorldAxis(
  const Camera&,
  const glm::vec2& ndcOldPos,
  const glm::vec2& ndcNewPos,
  float ndcZ,
  const glm::vec3& worldRotationAxis,
  const glm::vec3& worldRotationCenter
);

glm::vec2 scaleFactorsAboutWorldAxis(
  const Camera&,
  const glm::vec2& ndcOldPos,
  const glm::vec2& ndcNewPos,
  float ndcZ,
  const glm::mat4& slide_O_world,
  const glm::vec3& slideRotationCenter
);

glm::vec2 worldViewportDimensions(const Camera&, float ndcZ);

// Returns translation in World space
glm::vec3 worldTranslationPerpendicularToWorldAxis(
  const Camera&,
  const glm::vec2& ndcOldPos,
  const glm::vec2& ndcNewPos,
  float ndcZ,
  const glm::vec3& worldAxis
);

glm::vec4 ndc_O_view(const Viewport&, const glm::vec2& viewPos);
glm::vec2 ndc2d_O_view(const Viewport&, const glm::vec2& viewPos);

glm::vec2 viewDevice_O_ndc(const Viewport&, const glm::vec2& ndcPos);
glm::vec2 view_O_ndc(const Viewport&, const glm::vec2& ndcPos);
glm::vec2 view_O_mouse(const Viewport&, const glm::vec2& mousePos);

glm::vec4 ndc_O_mouse(const Viewport&, const glm::vec2& mousePos);
glm::vec2 ndc2d_O_mouse(const Viewport&, const glm::vec2& mousePos);

glm::mat4 get_ndc_O_view(const Viewport&);

/**
 * @brief Get intersection of ray with plane.
 * Ray is defined by point in NDC.
 * Plane normal is defined by camera's z axis.
 * @param ndcRayPos Origin point of ray in NDC
 * @param worldPlanePos World-space position of point on plane
 * @return World-space intersection of ray with plane if it is defined;
 * std::nullopt otherwise
 */
std::optional<glm::vec3> worldCameraPlaneIntersection(
  const Camera&, const glm::vec2& ndcRayPos, const glm::vec3& worldPlanePos
);

void positionCameraForWorldTargetAndFov(
  Camera&, const glm::vec3& worldBoxSize, const glm::vec3& worldTarget
);

/**
 * @brief Return the eight corners of the camera's view frustum
 * in World space coordinates. The frustum of a camera with orthographic
 * projection is a rectangular prism.
 *
 * [0] right, top, near
 * [1] left, top, near
 * [2] left, bottom, near
 * [3] right, bottom, near
 * [4] right, top, far
 * [5] left, top, far
 * [6] left, bottom, far
 * [7] right, bottom, far
 */
std::array<glm::vec3, 8> worldFrustumCorners(const Camera&);

/**
 * @brief worldFrustumPlanes
 * @return
 *
 * [0] right
 * [1] top
 * [2] left
 * [3] bottom
 * [4] near
 * [5] far
 */
std::array<glm::vec4, 6> worldFrustumPlanes(const Camera&);

/**
 * @brief Convert position in 2D View space to World space
 * @param[in] viewport Viewport
 * @param[in] camera Camera
 * @param[in] viewPos Position in 2D View space
 * @param[in] ndcZ Z depth of position in NDC (set to -1 if the perspective is orthogonal)
 * @return Position in World space
 */
glm::vec4 world_O_view(
  const Viewport& viewport, const camera::Camera& camera, const glm::vec2& viewPos, float ndcZ = -1.0f
);

/// @todo Make this function valid for perspective views, too!
/// Currently not valid for perspective projection.
glm::vec2 worldPixelSize(const Viewport& viewport, const camera::Camera& camera);

glm::vec2 worldPixelSizeAtWorldPosition(
  const Viewport& viewport, const camera::Camera& camera, const glm::vec3& worldPos
);

float computeSmallestWorldDepthOffset(const camera::Camera& camera, const glm::vec3& worldPos);

} // namespace camera

#endif // CAMERA_HELPERS_H
