#pragma once

#include "logic/camera/Camera.h"
#include "logic/camera/Projection.h"

#include "common/DirectionMaps.h"
#include "common/Types.h"

#include <glm/fwd.hpp>
#include <glm/vec4.hpp>

#include <array>
#include <memory>
#include <optional>
#include <utility>

class CoordinateFrame;
class Viewport;

/**
 * @brief Free camera math and interaction helper functions
 */
namespace helper
{

/**
 * @brief Create a camera projection of a given type
 * @param[in] type Type of camera projection
 * @return Unique pointer to camera projection
 * @throw Propagates exceptions from projection allocation
 */
std::unique_ptr<Projection> createCameraProjection(const ProjectionType& projectionType);

/**
 * @brief Compute full model-view-projection transformation chain from World to OpenGL Clip space
 * for a given camera
 * @param camera Camera that supplies view and projection transforms
 * @return Transform from World space to Clip space
 * @throw Propagates exceptions from camera transform access
 */
glm::mat4 clip_T_world(const Camera& camera);

/**
 * @brief Compute inverse of full model-view-projection transformation chain from OpenGL Clip
 * to World space for a given camera
 * @param camera Camera that supplies view and projection transforms
 * @return Transform from Clip space to World space
 * @throw Propagates exceptions from camera transform access
 */
glm::mat4 world_T_clip(const Camera& camera);

/**
 * @brief Return the World-space origin position of a camera
 * @param camera Camera to inspect
 * @return Camera eye position in World space
 * @throw Propagates exceptions from camera transform access
 */
glm::vec3 worldOrigin(const Camera& camera);

/**
 * @brief Return the Normalized World-space direction vector of a camera
 * @param camera Camera to inspect
 * @param dir View-axis direction to transform
 * @return Normalized direction in World space
 * @throw Propagates exceptions from camera transform access
 */
glm::vec3 worldDirection(const Camera& camera, const Directions::View& dir);

/**
 * @brief Return the normalized World-space vector along a CoordinateFrame direction axis
 * @param frame Coordinate frame to inspect
 * @param dir Cartesian axis to transform
 * @return Normalized direction in World space
 * @todo Move this to another helper class for CoordinateFrame-specific logic
 */
glm::vec3 worldDirection(const CoordinateFrame& frame, const Directions::Cartesian& dir);

/**
 * @brief Return the normalized Camera-space vector of an anatomical direction
 * @param camera Camera that defines Camera space
 * @param dir Anatomical direction to transform
 * @return Normalized direction in Camera space
 * @throw Propagates exceptions from camera transform access
 */
glm::vec3 cameraDirectionOfAnatomy(const Camera& camera, const Directions::Anatomy& dir);

/**
 * @brief Return the normalized Camera-space vector of a World direction
 * @param camera Camera that defines Camera space
 * @param dir Cartesian World direction to transform
 * @return Normalized direction in Camera space
 * @throw Propagates exceptions from camera transform access
 */
glm::vec3 cameraDirectionOfWorld(const Camera& camera, const Directions::Cartesian& dir);

/**
 * @brief World-space position of NDC point
 * @param camera Camera that defines the Clip-to-World transform
 * @param ndcPos NDC of point
 * @return Position in World space
 * @throw Propagates exceptions from camera transform access
 */
glm::vec3 world_T_ndc(const Camera& camera, const glm::vec3& ndcPos);

/**
 * @brief NDC of Camera-space point
 * @param camera Camera that defines the Camera-to-Clip transform
 * @param cameraPos Camera-space coordinates of point
 * @return Position in normalized device coordinates
 * @throw Propagates exceptions from projection access
 */
glm::vec3 ndc_T_camera(const Camera& camera, const glm::vec3& cameraPos);

/**
 * @brief Transform an NDC point into Camera space
 * @param camera Camera that defines the Clip-to-Camera transform
 * @param ndcPos Position in normalized device coordinates
 * @return Position in Camera space
 * @throw Propagates exceptions from projection access
 */
glm::vec3 camera_T_ndc(const Camera& camera, const glm::vec3& ndcPos);

/**
 * @brief Camera-space position of World point
 * @param camera Camera that defines World-to-Camera transform
 * @param worldPos World-space coordinates of point
 * @return Position in Camera space
 * @throw Propagates exceptions from camera transform access
 */
glm::vec3 camera_T_world(const Camera& camera, const glm::vec3& worldPos);

/**
 * @brief NDC position of world point
 * @param camera Camera that defines World-to-Clip transform
 * @param worldPos World-space coordinates of point
 * @return Position in normalized device coordinates
 * @throw Propagates exceptions from camera transform access
 */
glm::vec3 ndc_T_world(const Camera& camera, const glm::vec3& worldPos);

/**
 * @brief World-space direction of ray emanating from NDC point
 * @param camera Camera that defines the ray
 * @param ndcRay NDC xy of ray position
 * @return Normalized ray direction in World space
 * @throw Propagates exceptions from camera transform access
 */
glm::vec3 worldRayDirection(const Camera& camera, const glm::vec2& ndcRay);

/**
 * @brief Camera-space direction of ray emanating from NDC point
 * @param camera Camera that defines the ray
 * @param ndcRay NDC xy of ray position
 * @return Normalized ray direction in Camera space
 * @throw Propagates exceptions from projection access
 */
glm::vec3 cameraRayDirection(const Camera& camera, const glm::vec2& ndcRay);

/**
 * @brief Apply a transformation to the camera relative to its start frame
 * @param camera Camera to mutate
 * @param m Transform applied in Camera space
 */
void applyViewTransformation(Camera& camera, const glm::mat4& m);

/**
 * @brief Apply a rotation to the camera relative to its start frame
 * @param camera Camera to mutate
 * @param rotation Rotation to apply
 * @param worldRotationPos World-space point about which the rotation is applied
 */
void applyViewRotationAboutWorldPoint(Camera& camera, const glm::quat& rotation, const glm::vec3& worldRotationPos);

/**
 * @brief Reset the camera to its start frame orientation
 * @param camera Camera to mutate
 */
void resetViewTransformation(Camera& camera);

/**
 * @brief Reset the camera's zoom factor to default
 * @param camera Camera to mutate
 */
void resetZoom(Camera& camera);

/**
 * @brief Set the camera origin to a World position
 * @param camera Camera to mutate
 * @param worldPos Desired camera eye position in World space
 */
void setCameraOrigin(Camera& camera, const glm::vec3& worldPos);

/**
 * @brief Set the camera target to World position
 * @param camera Camera to mutate
 * @param worldPos World-space position
 * @param targetDistance Offset distance of camera backwards from position
 */
void setWorldTarget(Camera& camera, const glm::vec3& worldPos, const std::optional<float>& targetDistance);

/**
 * @brief Rotate the camera around its origin using a Camera-space axis
 * @param camera Camera to mutate
 * @param cameraVec Rotation axis in Camera space
 * @param angle Rotation angle in radians
 */
void rotateAboutOrigin(Camera& camera, const glm::vec3& cameraVec, float angleRadians);

/**
 * @brief Rotate the camera around its origin using a named view axis
 * @param camera Camera to mutate
 * @param dir View-axis direction
 * @param angle Rotation angle in radians
 */
void rotateAboutOrigin(Camera& camera, const Directions::View& dir, float angleRadians);

/**
 * @brief Rotate the camera around a Camera-space center using a named view axis
 * @param camera Camera to mutate
 * @param eyeAxis View-axis direction
 * @param angle Rotation angle in radians
 * @param cameraCenter Rotation center in Camera space
 */
void rotate(Camera& camera, const Directions::View& eyeAxis, float angleRadians, const glm::vec3& cameraCenter);

/**
 * @brief Rotate the camera around a Camera-space center using a Camera-space axis
 * @param camera Camera to mutate
 * @param cameraAxis Rotation axis in Camera space
 * @param angle Rotation angle in radians
 * @param cameraCenter Rotation center in Camera space
 */
void rotate(Camera& camera, const glm::vec3& cameraAxis, float angleRadians, const glm::vec3& cameraCenter);

/**
 * @brief Translate the camera along a named Camera-space view direction
 * @param camera Camera to mutate
 * @param dir View-axis direction
 * @param distance Translation distance
 */
void translateAboutCamera(Camera& camera, const Directions::View& dir, float distance);

/**
 * @brief Translate the camera by a Camera-space vector
 * @param camera Camera to mutate
 * @param cameraVec Translation vector in Camera space
 */
void translateAboutCamera(Camera& camera, const glm::vec3& cameraVec);

/**
 * @brief Reflect the camera front direction around a Camera-space center
 * @param camera Camera to mutate
 * @param cameraCenter Reflection center in Camera space
 */
void reflectFront(Camera& camera, const glm::vec3& cameraCenter);

/**
 * @brief Zoom the camera about an NDC center point
 * @param camera Camera to mutate
 * @param factor Multiplicative zoom factor
 * @param cameraCenter Center point in Camera space
 */
void zoom(Camera& camera, float factor, const glm::vec2& cameraCenterPos);

/**
 * @brief Translate the camera in or out using vertical NDC drag distance
 * @param camera Camera to mutate
 * @param ndcOldPos Previous pointer position in NDC
 * @param ndcNewPos Current pointer position in NDC
 * @param scale Translation scale
 */
void translateInOut(Camera& camera, const glm::vec2& ndcOldPos, const glm::vec2& ndcNewPos, float scale);

/**
 * @brief Pan the camera so a selected World-space point follows the pointer
 * @param camera Camera to mutate
 * @param ndcOldPos Previous pointer position in NDC
 * @param ndcNewPos Current pointer position in NDC
 * @param worldPos World-space point used as the panning depth
 */
void panRelativeToWorldPosition(
  Camera& camera,
  const glm::vec2& ndcOldPos,
  const glm::vec2& ndcNewPos,
  const glm::vec3& worldPos);

/**
 * @brief Rotate the camera about its eye from an NDC drag
 * @param camera Camera to mutate
 * @param ndcOldPos Previous pointer position in NDC
 * @param ndcNewPos Current pointer position in NDC
 */
void rotateAboutCameraOrigin(Camera& camera, const glm::vec2& ndcOldPos, const glm::vec2& ndcNewPos);

/**
 * @brief Rotate the camera around a World-space point from an NDC drag
 * @param camera Camera to mutate
 * @param ndcOldPos Previous pointer position in NDC
 * @param ndcNewPos Current pointer position in NDC
 * @param worldRotationPos Rotation center in World space
 */
void rotateAboutWorldPoint(
  Camera& camera,
  const glm::vec2& ndcOldPos,
  const glm::vec2& ndcNewPos,
  const glm::vec3& worldRotationPos);

/**
 * @brief Rotate the camera in the view plane from an NDC drag
 * @param camera Camera to mutate
 * @param ndcOldPos Previous pointer position in NDC
 * @param ndcNewPos Current pointer position in NDC
 * @param ndcRotationCenter Rotation center in NDC
 */
void rotateInPlane(
  Camera& camera,
  const glm::vec2& ndcOldPos,
  const glm::vec2& ndcNewPos,
  const glm::vec2& ndcRotationCenter);

/**
 * @brief Rotate the camera in the view plane by an angle
 * @param camera Camera to mutate
 * @param angle Rotation angle in radians
 * @param ndcRotationCenter Rotation center in NDC
 */
void rotateInPlane(Camera& camera, float angle, const glm::vec2& ndcRotationCenter);

/**
 * @brief Zoom the camera from an NDC drag around an NDC center
 * @param camera Camera to mutate
 * @param ndcOldPos Previous pointer position in NDC
 * @param ndcNewPos Current pointer position in NDC
 * @param ndcCenterPos Zoom center in NDC
 */
void zoomNdc(Camera& camera, const glm::vec2& ndcOldPos, const glm::vec2& ndcNewPos, const glm::vec2& ndcCenterPos);

/**
 * @brief Zoom the camera by a multiplicative factor around an NDC center
 * @param camera Camera to mutate
 * @param magFactor Multiplicative zoom factor
 * @param ndcCenterPos Zoom center in NDC
 */
void zoomNdc(Camera& camera, float factor, const glm::vec2& ndcCenterPos);

/**
 * @brief Zoom the camera by an additive drag or scroll delta around an NDC center
 * @param camera Camera to mutate
 * @param delta Zoom delta
 * @param ndcCenterPos Zoom center in NDC
 */
void zoomNdcDelta(Camera& camera, float delta, const glm::vec2& ndcCenterPos);

/**
 * @brief Compute NDC z for a World-space point
 * @param camera Camera that defines World-to-Clip transform
 * @param worldPos World-space point
 * @return NDC z coordinate
 * @throw Propagates exceptions from camera transform access
 */
float ndcZofWorldPoint(const Camera& camera, const glm::vec3& worldPos);

/**
 * @brief Compute NDC z for a World-space point using the alternate implementation
 * @param camera Camera that defines World-to-Clip transform
 * @param worldPoint World-space point
 * @return NDC z coordinate
 * @throw Propagates exceptions from camera transform access
 */
float ndcZofWorldPoint_v2(const Camera& camera, const glm::vec3& worldPoint);

/**
 * @brief Compute NDC z for a distance along the camera front axis
 * @param camera Camera that defines Camera-to-Clip transform
 * @param cameraDistance Distance from the camera origin
 * @return NDC z coordinate
 * @throw Propagates exceptions from projection access
 */
float ndcZOfCameraDistance(const Camera& camera, const float cameraDistance);

/**
 * @brief Convert OpenGL depth-buffer depth to normalized device z
 * @param depth OpenGL depth value in `[0, 1]`
 * @return NDC z value in `[-1, 1]`
 */
float convertOpenGlDepthToNdc(float depth);

/**
 * @brief Project an NDC point onto the virtual arcball sphere around a World-space center
 * @param camera Camera that defines the sphere view
 * @param ndcPos Pointer position in NDC
 * @param worldSphereCenter Sphere center in World space
 * @return Point on or near the virtual sphere in Camera space
 * @throw Propagates exceptions from camera transform access
 */
glm::vec3 sphere_T_ndc(const Camera& camera, const glm::vec2& ndcPos, const glm::vec3& worldSphereCenter);

/**
 * @brief Compute an arcball rotation between two NDC positions
 * @param camera Camera that defines the arcball
 * @param ndcStartPos Start pointer position in NDC
 * @param ndcNewPos Current pointer position in NDC
 * @param worldSphereCenter Arcball center in World space
 * @return Rotation quaternion in Camera space
 * @throw Propagates exceptions from camera transform access
 */
glm::quat rotationAlongArc(
  const Camera& camera,
  const glm::vec2& ndcStartPos,
  const glm::vec2& ndcNewPos,
  const glm::vec3& worldSphereCenter);

/**
 * @brief Compute a 2D in-plane camera rotation from an NDC drag
 * @param camera Camera that defines the view plane
 * @param ndcOldPos Previous pointer position in NDC
 * @param ndcNewPos Current pointer position in NDC
 * @param ndcRotationCenter Rotation center in NDC
 * @return Rotation quaternion in Camera space
 */
glm::quat rotation2dInCameraPlane(
  const Camera& camera,
  const glm::vec2& ndcOldPos,
  const glm::vec2& ndcNewPos,
  const glm::vec2& ndcRotationCenter = glm::vec2{0.0f, 0.0f});

/**
 * @brief Compute a snapped 2D in-plane camera rotation from an NDC drag
 * @param camera Camera that defines the view plane
 * @param ndcStartPos Start pointer position in NDC
 * @param ndcOldPos Previous pointer position in NDC
 * @param ndcNewPos Current pointer position in NDC
 * @param snapAngleDegrees Angle increment used for snapping
 * @param angleTolerance Tolerance around each snap angle
 * @param ndcRotationCenter Rotation center in NDC
 * @return Rotation quaternion in Camera space
 */
glm::quat rotation2dInCameraPlaneWithSnapping(
  const Camera& camera,
  const glm::vec2& ndcStartPos,
  const glm::vec2& ndcOldPos,
  const glm::vec2& ndcNewPos,
  float snapAngleDegrees,
  float angleTolerance,
  const glm::vec2& ndcRotationCenter = glm::vec2{0.0f, 0.0f});

/**
 * @brief Compute a camera-space 3D rotation from an NDC drag
 * @param camera Camera that defines the view plane
 * @param ndcOldPos Previous pointer position in NDC
 * @param ndcNewPos Current pointer position in NDC
 * @return Rotation quaternion in Camera space
 */
glm::quat rotation3dAboutCameraPlane(const Camera& camera, const glm::vec2& ndcOldPos, const glm::vec2& ndcNewPos);

/**
 * @brief Compute a Camera-space translation parallel to the view plane from an NDC drag
 * @param camera Camera that defines the view plane
 * @param ndcOldPos Previous pointer position in NDC
 * @param ndcNewPos Current pointer position in NDC
 * @param ndcZ Depth at which to measure the drag
 * @return Translation vector in Camera space
 * @throw Propagates exceptions from projection access
 */
glm::vec3
translationInCameraPlane(const Camera& camera, const glm::vec2& ndcOldPos, const glm::vec2& ndcNewPos, float ndcZ);

/**
 * @brief Compute a Camera-space front/back translation from an NDC drag
 * @param camera Camera that defines the view direction
 * @param ndcOldPos Previous pointer position in NDC
 * @param ndcNewPos Current pointer position in NDC
 * @param scale Translation scale
 * @return Translation vector in Camera space
 */
glm::vec3 translationAboutCameraFrontBack(
  const Camera& camera,
  const glm::vec2& ndcOldPos,
  const glm::vec2& ndcNewPos,
  float scale);

/**
 * @brief Compute drag translation along a World-space axis
 * @param camera Camera that defines the view plane
 * @param ndcOldPos Previous pointer position in NDC
 * @param ndcNewPos Current pointer position in NDC
 * @param ndcZ Depth at which to measure the drag
 * @param worldAxis Axis along which translation is measured
 * @return Signed translation distance along `worldAxis`
 * @throw Propagates exceptions from camera transform access
 */
float axisTranslationAlongWorldAxis(
  const Camera& camera,
  const glm::vec2& ndcOldPos,
  const glm::vec2& ndcNewPos,
  float ndcZ,
  const glm::vec3& worldAxis);

/**
 * @brief Compute drag rotation angle around a World-space axis
 * @param camera Camera that defines the view plane
 * @param ndcOldPos Previous pointer position in NDC
 * @param ndcNewPos Current pointer position in NDC
 * @param ndcZ Depth at which to measure the drag
 * @param worldRotationAxis Rotation axis in World space
 * @param worldRotationCenter Rotation center in World space
 * @return Signed rotation angle in radians
 * @throw Propagates exceptions from camera transform access
 */
float rotationAngleAboutWorldAxis(
  const Camera& camera,
  const glm::vec2& ndcOldPos,
  const glm::vec2& ndcNewPos,
  float ndcZ,
  const glm::vec3& worldRotationAxis,
  const glm::vec3& worldRotationCenter);

/**
 * @brief Compute scale factors from an NDC drag relative to a transformed slide frame
 * @param camera Camera that defines the view plane
 * @param ndcOldPos Previous pointer position in NDC
 * @param ndcNewPos Current pointer position in NDC
 * @param ndcZ Depth at which to measure the drag
 * @param slide_T_world Transform from World space to slide space
 * @param slideRotationCenter Rotation center in slide/world interaction space
 * @return Scale factors along slide axes
 * @throw Propagates exceptions from camera transform access
 */
glm::vec2 scaleFactorsAboutWorldAxis(
  const Camera& camera,
  const glm::vec2& ndcOldPos,
  const glm::vec2& ndcNewPos,
  float ndcZ,
  const glm::mat4& slide_T_world,
  const glm::vec3& slideRotationCenter);

/**
 * @brief Compute the World-space dimensions of the viewport at an NDC depth
 * @param camera Camera that defines the projection
 * @param ndcZ Depth at which to measure the viewport
 * @return Width and height in World units
 * @throw Propagates exceptions from camera transform access
 */
glm::vec2 worldViewportDimensions(const Camera& camera, float ndcZ);

/**
 * @brief Compute World-space translation perpendicular to a World-space axis
 * @param camera Camera that defines the view plane
 * @param ndcOldPos Previous pointer position in NDC
 * @param ndcNewPos Current pointer position in NDC
 * @param ndcZ Depth at which to measure the drag
 * @param worldAxis Axis that the returned translation is perpendicular to
 * @return Translation vector in World space
 * @throw Propagates exceptions from camera transform access
 */
glm::vec3 worldTranslationPerpendicularToWorldAxis(
  const Camera& camera,
  const glm::vec2& ndcOldPos,
  const glm::vec2& ndcNewPos,
  float ndcZ,
  const glm::vec3& worldAxis);

/**
 * @brief Transform position from Window Pixel space to 2D Window NDC
 * @param windowViewport Full window viewport in pixels
 * @param windowPixelPos Position in window pixel coordinates
 * @return Position in window NDC
 */
glm::vec2 windowNdc_T_window(const Viewport& windowViewport, const glm::vec2& windowPixelPos);

/**
 * @brief Transform NDC to viewport device coordinates
 * @param viewport Viewport that defines the device coordinate extents
 * @param ndcPos Position in NDC
 * @return Position in viewport device coordinates
 */
glm::vec2 viewDevice_T_ndc(const Viewport& viewport, const glm::vec2& ndcPos);

/**
 * @brief Transform window clip coordinates to window pixel coordinates
 * @param viewport Window viewport
 * @param ndcPos Position in window clip coordinates
 * @return Position in window pixel coordinates
 */
glm::vec2 window_T_windowClip(const Viewport& viewport, const glm::vec2& ndcPos);

/**
 * @brief Transform window clip coordinates to viewport coordinates
 * @param viewport Viewport frame
 * @param ndcPos Position in window clip coordinates
 * @return Position in viewport coordinates
 */
glm::vec2 viewport_T_windowClip(const Viewport& windowViewport, const glm::vec2& ndcPos);

/**
 * @brief Transform viewport coordinates to window clip coordinates
 * @param viewport Viewport frame
 * @param viewportPos Position in viewport coordinates
 * @return Position in window clip coordinates
 */
glm::vec2 windowClip_T_viewport(const Viewport& windowViewport, const glm::vec2& viewportPos);

/**
 * @brief Build the matrix from window clip coordinates to window pixel coordinates
 * @param viewport Window viewport
 * @return Window coordinate transform
 */
glm::mat4 window_T_windowClip(const Viewport& viewport);

/**
 * @brief Build the matrix from window clip coordinates to viewport coordinates
 * @param viewport Viewport frame
 * @return Viewport coordinate transform
 */
glm::mat4 viewport_T_windowClip(const Viewport& windowViewport);

/**
 * @brief Convert window coordinates to mindow coordinates by flipping y for mouse input
 * @param wholeWindowHeight Window height in pixels
 * @param mousePos Position in window coordinates
 * @return Position in mindow coordinates
 */
glm::vec2 window_T_mindow(float wholeWindowHeight, const glm::vec2& mousePos);

/**
 * @brief Build the transform from mindow coordinates to window coordinates
 * @param wholeWindowHeight Window height in pixels
 * @return Matrix from mindow to window coordinates
 */
glm::mat4 window_T_mindow(float wholeWindowHeight);

/**
 * @brief Build the transform from window coordinates to mindow coordinates
 * @param wholeWindowHeight Window height in pixels
 * @return Matrix from window to mindow coordinates
 */
glm::mat4 mindow_T_window(float wholeWindowHeight);

/**
 * @brief Convert viewport coordinates to miewport coordinates by flipping y for mouse input
 * @param viewportHeight Viewport height in pixels
 * @param viewPos Position in viewport coordinates
 * @return Position in miewport coordinates
 */
glm::vec2 miewport_T_viewport(float viewportHeight, const glm::vec2& viewPos);

/**
 * @brief Convert miewport coordinates to viewport coordinates
 * @param viewportHeight Viewport height in pixels
 * @param viewPos Position in miewport coordinates
 * @return Position in viewport coordinates
 */
glm::vec2 viewport_T_miewport(float viewportHeight, const glm::vec2& viewPos);

/**
 * @brief Build the transform from viewport coordinates to miewport coordinates
 * @param viewportHeight Viewport height in pixels
 * @return Matrix from viewport to miewport coordinates
 */
glm::mat4 miewport_T_viewport(float viewportHeight);

/**
 * @brief Get intersection of ray with plane
 * Ray is defined by point in NDC
 * Plane normal is defined by camera's z axis
 * @param ndcRayPos Origin point of ray in NDC
 * @param worldPlanePos World-space position of point on plane
 * @return World-space intersection of ray with plane if it is defined;
 * std::none otherwise
 * @throw Propagates exceptions from camera transform access
 */
std::optional<glm::vec3>
worldCameraPlaneIntersection(const Camera& camera, const glm::vec2& ndcRayPos, const glm::vec3& worldPlanePos);

/**
 * @brief Position the camera to look at a target in World space and adjust the camera such that
 * it fits a given AABB (defined in World space) in its field of view
 * @param[in] camera Camera to mutate
 * @param[in] worldBoxSize AABB (World space)
 * @param[in] worldTarget Target point (World space)
 */
void positionCameraForWorldTargetAndFov(Camera& camera, const glm::vec3& worldBoxSize, const glm::vec3& worldTarget);

/**
 * @brief Position the camera to look at a target in World space
 * @param camera Camera to mutate
 * @param worldBoxSize World-space size used to choose pullback and far distance
 * @param worldTarget Target point in World space
 */
void positionCameraForWorldTarget(Camera& camera, const glm::vec3& worldBoxSize, const glm::vec3& worldTarget);

/**
 * @brief Compute the World-space AABB extents projected onto the camera right and up axes
 * @param[in] camera Camera whose view-plane axes define the projection
 * @param[in] worldBoxSize Size of the World-space AABB
 * @return Horizontal and vertical field-of-view extents in World units
 * @throw Propagates exceptions from camera transform access
 */
glm::vec2 viewPlaneFovForWorldBox(const Camera& camera, const glm::vec3& worldBoxSize);

/**
 * @brief Orient the camera so its front direction matches a target World-space normal
 * @param camera Camera to mutate
 * @param targetWorldNormalDirection Desired camera front direction in World space
 */
void orientCameraToWorldTargetNormalDirection(Camera& camera, const glm::vec3& targetWorldNormalDirection);

/**
 * @brief Set the camera front direction in World space
 * @param camera Camera to mutate
 * @param worldForwardDirection Desired camera front direction in World space
 */
void setWorldForwardDirection(Camera& camera, const glm::vec3& worldForwardDirection);

/**
 * @brief Compute camera pullback and far-plane distances that fit a World-space box
 * @param camera Camera whose projection is used
 * @param worldBoxSize Size of the World-space AABB
 * @return Pullback distance and far clipping distance
 * @throw Propagates exceptions from projection access
 */
std::pair<float, float> computePullbackAndFarDistances(const Camera& camera, const glm::vec3& worldBoxSize);

/**
 * @brief Return the eight corners of the camera's view frustum
 * in World space coordinates. The frustum of a camera with orthographic
 * projection is a rectangular prism
 *
 * [0] right, top, near
 * [1] left, top, near
 * [2] left, bottom, near
 * [3] right, bottom, near
 * [4] right, top, far
 * [5] left, top, far
 * [6] left, bottom, far
 * [7] right, bottom, far
 * @param camera Camera whose frustum is sampled
 * @return Eight frustum corners in World space
 * @throw Propagates exceptions from camera transform access
 */
std::array<glm::vec3, 8> worldFrustumCorners(const Camera& camera);

/**
 * @brief Return the six camera frustum planes in World space
 *
 * [0] right
 * [1] top
 * [2] left
 * [3] bottom
 * [4] near
 * [5] far
 *
 * @param camera Camera whose frustum is sampled
 * @return Six frustum plane equations in World space
 * @throw Propagates exceptions from camera transform access
 */
std::array<glm::vec4, 6> worldFrustumPlanes(const Camera& camera);

/**
 * @brief Convert position in 2D View space to World space
 * @param[in] viewport Viewport
 * @param[in] camera Camera
 * @param[in] viewPos Position in 2D View space
 * @param[in] ndcZ Z depth of position in NDC (set to -1 if the perspective is orthogonal)
 * @return Position in World space
 * @throw Propagates exceptions from camera transform access
 */
glm::vec4 world_T_view(const Viewport& viewport, const Camera& camera, const glm::vec2& viewPos, float ndcZ = -1.0f);

/**
 * @brief Compute the World-space size of one screen pixel for an orthographic view
 * @param viewport Viewport used for pixel dimensions
 * @param camera Orthographic camera
 * @return Pixel size in World units
 * @throw Propagates exceptions from camera transform access
 *
 * @todo Make this function valid for perspective views, too
 */
glm::vec2 worldPixelSize(const Viewport& viewport, const Camera& camera);

/**
 * @brief Compute World-space pixel size at a specific World position
 * @param viewport Viewport used for pixel dimensions
 * @param camera Camera used for projection
 * @param worldPos World-space position at which to measure pixel size
 * @return Pixel size in World units
 * @throw Propagates exceptions from camera transform access
 */
glm::vec2 worldPixelSizeAtWorldPosition(const Viewport& viewport, const Camera& camera, const glm::vec3& worldPos);

/**
 * @brief Compute the smallest useful depth offset at a World-space point
 * @param camera Camera used for projection
 * @param worldPos World-space position
 * @return Small depth offset in World units
 * @throw Propagates exceptions from camera transform access
 */
float computeSmallestWorldDepthOffset(const Camera& camera, const glm::vec3& worldPos);

/**
 * @brief Convert a World-space position to the view's miewport space
 * @param windowVP Window viewport
 * @param camera View camera
 * @param windowClip_T_viewClip Transform from View clip space to Window clip space
 * @param worldPos Position in World space
 * @return Position in miewport coordinates
 * @throw Propagates exceptions from camera transform access
 */
glm::vec2 miewport_T_world(
  const Viewport& windowVP,
  const Camera& camera,
  const glm::mat4& windowClip_T_viewClip,
  const glm::vec3& worldPos);

/**
 * @brief Convert a miewport position to World space
 * @param windowVP Window viewport
 * @param camera View camera
 * @param viewClip_T_windowClip Transform from Window clip space to View clip space
 * @param miewportPos Position in miewport coordinates
 * @return Position in World space
 * @throw Propagates exceptions from camera transform access
 */
glm::vec3 world_T_miewport(
  const Viewport& windowVP,
  const Camera& camera,
  const glm::mat4& viewClip_T_windowClip,
  const glm::vec2& miewportPos);

/**
 * @brief Compute World-space pixel size for a view embedded in a window
 * @param windowVP Window viewport
 * @param camera View camera
 * @param viewClip_T_windowClip Transform from Window clip space to View clip space
 * @return Pixel size in World units
 * @throw Propagates exceptions from camera transform access
 */
glm::vec2 worldPixelSize(const Viewport& windowVP, const Camera& camera, const glm::mat4& viewClip_T_windowClip);

/**
 * @brief Compute camera rotation relative to World axes
 * @param camera Camera to inspect
 * @return Rotation quaternion from Camera space to World space
 * @throw Propagates exceptions from camera transform access
 */
glm::quat computeCameraRotationRelativeToWorld(const Camera& camera);

/**
 * @brief Compute the min and max coordinates of a frame
 * @param winClipFrameViewport Viewport of the frame defined in window Clip space
 * @param windowViewport Viewport of the window
 * @return Min and max coordinates of the frame in window space
 */
FrameBounds computeMiewportFrameBounds(const glm::vec4& windowClipFrameViewport, const glm::vec4& windowViewport);

/**
 * @brief Compute the mindow bounds of a frame in a window
 * @param windowClipFrameViewport Viewport of the frame defined in window Clip space
 * @param windowViewport Viewport of the window
 * @param wholeWindowHeight Whole window height in pixels
 * @return Min and max coordinates of the frame in mindow space
 */
FrameBounds computeMindowFrameBounds(
  const glm::vec4& windowClipFrameViewport,
  const glm::vec4& windowViewport,
  float wholeWindowHeight);

/**
 * @brief Check whether a camera front direction is close to a World orthogonal axis
 * @param camera Camera to inspect
 * @return True when the camera looks along a principal axis
 * @throw Propagates exceptions from camera transform access
 */
bool looksAlongOrthogonalAxis(const Camera& camera);

/**
 * @brief Check whether two vectors are parallel within an angular threshold
 * @param a First vector
 * @param b Second vector
 * @param angleThreshold_degrees In [0, 90]
 * @return True when the vectors are parallel within the threshold
 */
bool areVectorsParallel(const glm::vec3& a, const glm::vec3& b, float angleThreshold_degrees);

/**
 * @brief Check whether two cameras have parallel named view directions
 * @param camera1 First camera
 * @param camera2 Second camera
 * @param dir View direction to compare
 * @param angleThreshold_degrees Parallel tolerance in degrees
 * @return True when the two view directions are parallel within the threshold
 * @throw Propagates exceptions from camera transform access
 */
bool areViewDirectionsParallel(
  const Camera& camera1,
  const Camera& camera2,
  const Directions::View& dir,
  float angleThreshold_degrees);

} // namespace helper
