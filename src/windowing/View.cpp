#include "windowing/View.h"

#include "common/DataHelper.h"

#include "image/Image.h"

#include "logic/app/Data.h"
#include "logic/camera/CameraHelpers.h"
#include "logic/camera/CameraStartFrameType.h"
#include "logic/camera/MathUtility.h"
#include "logic/camera/OrthogonalProjection.h"
#include "logic/camera/PerspectiveProjection.h"

#include "rendering/utility/math/SliceIntersector.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_inverse.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/component_wise.hpp>
#include <glm/gtx/string_cast.hpp>
#include <glm/gtx/transform.hpp>

#include <spdlog/fmt/ostr.h>
#include <spdlog/spdlog.h>

#include <array>
#include <unordered_map>

namespace
{

static const glm::vec3 sk_origin{0.0f};

// Map from view type to projection type
static const std::unordered_map<ViewType, camera::ProjectionType>
  sk_viewTypeToDefaultProjectionTypeMap = {
    {ViewType::Axial, camera::ProjectionType::Orthographic},
    {ViewType::Coronal, camera::ProjectionType::Orthographic},
    {ViewType::Sagittal, camera::ProjectionType::Orthographic},
    {ViewType::Oblique, camera::ProjectionType::Orthographic},
    {ViewType::ThreeD, camera::ProjectionType::Perspective}
};

// Map from start frame type to rotation matrix.
// This rotation matrix maps camera Start frame to World space.
static const std::unordered_map<CameraStartFrameType, glm::quat>
  sk_cameraStartFrameTypeToDefaultAnatomicalRotationMap = {
    {CameraStartFrameType::Crosshairs_Axial_LAI,
     glm::quat_cast(glm::mat3{1, 0, 0, 0, -1, 0, 0, 0, -1})},
    {CameraStartFrameType::Crosshairs_Axial_RAS,
     glm::quat_cast(glm::mat3{-1, 0, 0, 0, -1, 0, 0, 0, 1})},
    {CameraStartFrameType::Crosshairs_Coronal_LSA,
     glm::quat_cast(glm::mat3{1, 0, 0, 0, 0, 1, 0, -1, 0})},
    {CameraStartFrameType::Crosshairs_Coronal_RSP,
     glm::quat_cast(glm::mat3{-1, 0, 0, 0, 0, 1, 0, 1, 0})},
    {CameraStartFrameType::Crosshairs_Sagittal_PSL,
     glm::quat_cast(glm::mat3{0, 1, 0, 0, 0, 1, 1, 0, 0})},
    {CameraStartFrameType::Crosshairs_Sagittal_ASR,
     glm::quat_cast(glm::mat3{0, -1, 0, 0, 0, 1, -1, 0, 0})},
};

// Map from view convention to the maps of view type to camera start frame type
static const std::unordered_map<ViewConvention, std::unordered_map<ViewType, CameraStartFrameType> >
  sk_viewConventionToStartFrameTypeMap = {
    {ViewConvention::Radiological,
     {{ViewType::Axial, CameraStartFrameType::Crosshairs_Axial_LAI},
      {ViewType::Coronal, CameraStartFrameType::Crosshairs_Coronal_LSA},
      {ViewType::Sagittal, CameraStartFrameType::Crosshairs_Sagittal_PSL},
      {ViewType::Oblique, CameraStartFrameType::Crosshairs_Axial_LAI},
      {ViewType::ThreeD, CameraStartFrameType::Crosshairs_Coronal_LSA}}},
    // Left/right are swapped in axial and coronal views
    {ViewConvention::Neurological,
     {{ViewType::Axial, CameraStartFrameType::Crosshairs_Axial_RAS},
      {ViewType::Coronal, CameraStartFrameType::Crosshairs_Coronal_RSP},
      {ViewType::Sagittal, CameraStartFrameType::Crosshairs_Sagittal_PSL},
      {ViewType::Oblique, CameraStartFrameType::Crosshairs_Axial_RAS},
      {ViewType::ThreeD, CameraStartFrameType::Crosshairs_Coronal_LSA}}}
};

camera::ViewRenderMode reconcileRenderModeForViewType(
  const ViewType& viewType, const camera::ViewRenderMode& currentRenderMode
)
{
  /// @todo Write this function properly by accounting for
  /// All2dViewRenderModes, All3dViewRenderModes, All2dNonMetricRenderModes, All3dNonMetricRenderModes

  if (ViewType::ThreeD == viewType)
  {
    // If switching to ViewType::ThreeD, then switch to ViewRenderMode::VolumeRender:
    return camera::ViewRenderMode::VolumeRender;
  }
  else if (camera::ViewRenderMode::VolumeRender == currentRenderMode)
  {
    // If NOT switching to ViewType::ThreeD and currently using ViewRenderMode::VolumeRender,
    // then switch to ViewRenderMode::Image:
    return camera::ViewRenderMode::Image;
  }

  return currentRenderMode;
}

} // namespace

View::View(
  glm::vec4 winClipViewport,
  ViewOffsetSetting offsetSetting,
  ViewType viewType,
  camera::ViewRenderMode renderMode,
  camera::IntensityProjectionMode ipMode,
  UiControls uiControls,
  std::function<ViewConvention()> viewConventionProvider,
  std::optional<uuids::uuid> cameraRotationSyncGroupUid,
  std::optional<uuids::uuid> cameraTranslationSyncGroup,
  std::optional<uuids::uuid> cameraZoomSyncGroup
)
  : ControlFrame(winClipViewport, viewType, renderMode, ipMode, uiControls)
  ,

  m_offset(std::move(offsetSetting))
  ,

  m_projectionType(sk_viewTypeToDefaultProjectionTypeMap.at(m_viewType))
  , m_camera(m_projectionType)
  ,

  m_viewConventionProvider(viewConventionProvider)
  ,

  m_cameraRotationSyncGroupUid(cameraRotationSyncGroupUid)
  , m_cameraTranslationSyncGroupUid(cameraTranslationSyncGroup)
  , m_cameraZoomSyncGroupUid(cameraZoomSyncGroup)
  ,

  m_clipPlaneDepth(0.0f)
{
  m_camera.set_anatomy_T_start_provider(
    [this]()
    {
      return CoordinateFrame(
        sk_origin,
        sk_cameraStartFrameTypeToDefaultAnatomicalRotationMap.at(
          sk_viewConventionToStartFrameTypeMap.at(m_viewConventionProvider()).at(m_viewType)
        )
      );
    }
  );
}

glm::vec3 View::updateImageSlice(const AppData& appData, const glm::vec3& worldCrosshairs)
{
  static constexpr size_t k_maxNumWarnings = 10;
  static size_t warnCount = 0;

  const glm::vec3 worldCameraOrigin = camera::worldOrigin(m_camera);
  const glm::vec3 worldCameraFront = camera::worldDirection(m_camera, Directions::View::Front);

  // Compute the depth of the view plane in camera Clip space, because it is needed for the
  // coordinates of the quad that is textured with the image.

  // Apply this view's offset from the crosshairs position in order to calculate the view plane position.
  const float offsetDist = data::computeViewOffsetDistance(appData, m_offset, worldCameraFront);

  const glm::vec3 worldPlanePos = worldCrosshairs + offsetDist * worldCameraFront;
  const glm::vec4 worldViewPlane = math::makePlane(-worldCameraFront, worldPlanePos);

  // Compute the World-space distance between the camera origin and the view plane
  float worldCameraToPlaneDistance;

  if (math::vectorPlaneIntersection(
        worldCameraOrigin, worldCameraFront, worldViewPlane, worldCameraToPlaneDistance
      ))
  {
    camera::setWorldTarget(
      m_camera, worldCameraOrigin + worldCameraToPlaneDistance * worldCameraFront, std::nullopt
    );

    warnCount = 0; // Reset warning counter
  }
  else
  {
    if (warnCount++ < k_maxNumWarnings)
    {
      spdlog::warn(
        "Camera (front direction = {}) is parallel with the view (plane = {})",
        glm::to_string(worldCameraFront),
        glm::to_string(worldViewPlane)
      );
    }
    else if (k_maxNumWarnings == warnCount)
    {
      spdlog::warn("Halting warning about camera front direction.");
    }

    return worldCrosshairs;
  }

  const glm::vec4 clipPlanePos = camera::clip_T_world(m_camera) * glm::vec4{worldPlanePos, 1.0f};
  m_clipPlaneDepth = clipPlanePos.z / clipPlanePos.w;

  return worldPlanePos;
}

std::optional<intersection::IntersectionVerticesVec4> View::computeImageSliceIntersection(
  const Image* image, const CoordinateFrame& crosshairs
) const
{
  if (!image)
    return std::nullopt;

  // Compute the intersections in Pixel space by transforming the camera and crosshairs frame
  // from World to Pixel space. Pixel space is needed, because the corners form an AABB in that space.
  const glm::mat4 world_T_pixel = image->transformations().worldDef_T_subject()
                                  * image->transformations().subject_T_pixel();

  const glm::mat4 pixel_T_world = glm::inverse(world_T_pixel);

  // Object for intersecting the view plane with the 3D images
  SliceIntersector sliceIntersector;
  sliceIntersector.setPositioningMethod(intersection::PositioningMethod::FrameOrigin, std::nullopt);
  sliceIntersector.setAlignmentMethod(intersection::AlignmentMethod::CameraZ);

  std::optional<intersection::IntersectionVertices> pixelIntersectionPositions
    = sliceIntersector
        .computePlaneIntersections(
          pixel_T_world * m_camera.world_T_camera(),
          pixel_T_world * crosshairs.world_T_frame(),
          image->header().pixelBBoxCorners()
        )
        .first;

  if (!pixelIntersectionPositions)
  {
    // No slice intersection to render
    return std::nullopt;
  }

  // Convert Subject intersection positions to World space
  intersection::IntersectionVerticesVec4 worldIntersectionPositions;

  for (uint32_t i = 0; i < SliceIntersector::s_numVertices; ++i)
  {
    worldIntersectionPositions[i] = world_T_pixel
                                    * glm::vec4{(*pixelIntersectionPositions)[i], 1.0f};
  }

  return worldIntersectionPositions;
}

void View::setViewType(const ViewType& newViewType)
{
  if (newViewType == m_viewType)
  {
    return;
  }

  const auto newProjType = sk_viewTypeToDefaultProjectionTypeMap.at(newViewType);

  if (m_projectionType != newProjType)
  {
    spdlog::debug(
      "Changing camera projection from {} to {}",
      camera::typeString(m_projectionType),
      camera::typeString(newProjType)
    );

    std::unique_ptr<camera::Projection> projection;

    switch (newProjType)
    {
    case camera::ProjectionType::Orthographic:
    {
      projection = std::make_unique<camera::OrthographicProjection>();
      break;
    }
    case camera::ProjectionType::Perspective:
    {
      projection = std::make_unique<camera::PerspectiveProjection>();
      break;
    }
    }

    // Transfer the current projection parameters to the new projection:
    projection->setAspectRatio(m_camera.projection()->aspectRatio());
    projection->setDefaultFov(m_camera.projection()->defaultFov());
    projection->setFarDistance(m_camera.projection()->farDistance());
    projection->setNearDistance(m_camera.projection()->nearDistance());
    projection->setZoom(m_camera.projection()->getZoom());

    m_camera.setProjection(std::move(projection));
  }

  // Since different view types have different allowable render modes, the render mode must be
  // reconciled with the change in view type:
  m_renderMode = reconcileRenderModeForViewType(newViewType, m_renderMode);

  CoordinateFrame anatomy_T_start;

  if (ViewType::Oblique == newViewType)
  {
    // Transitioning to an Oblique view type from an Orthogonal view type:
    // The new anatomy_T_start frame is set to the (old) Orthogonal view type's anatomy_T_start frame.
    const auto& rotationMap = sk_cameraStartFrameTypeToDefaultAnatomicalRotationMap.at(
      sk_viewConventionToStartFrameTypeMap.at(m_viewConventionProvider()).at(m_viewType)
    );

    anatomy_T_start = CoordinateFrame(sk_origin, rotationMap);

    /// @todo Set anatomy_T_start equal to anatomy_T_start for the rotationSyncGroup of this view instead!!
  }
  else
  {
    // Transitioning to an Orthogonal view type:
    const auto& rotationMap = sk_cameraStartFrameTypeToDefaultAnatomicalRotationMap.at(
      sk_viewConventionToStartFrameTypeMap.at(m_viewConventionProvider()).at(newViewType)
    );

    anatomy_T_start = CoordinateFrame(sk_origin, rotationMap);

    if (ViewType::Oblique == m_viewType)
    {
      // Transitioning to an Orthogonal view type from an Oblique view type.
      // Reset the manually applied view transformations, because view might have rotations applied.
      camera::resetViewTransformation(m_camera);
    }
  }

  m_camera.set_anatomy_T_start_provider([anatomy_T_start]() { return anatomy_T_start; });

  m_viewType = newViewType;
}

std::optional<uuids::uuid> View::cameraRotationSyncGroupUid() const
{
  return m_cameraRotationSyncGroupUid;
}

std::optional<uuids::uuid> View::cameraTranslationSyncGroupUid() const
{
  return m_cameraTranslationSyncGroupUid;
}

std::optional<uuids::uuid> View::cameraZoomSyncGroupUid() const
{
  return m_cameraZoomSyncGroupUid;
}

float View::clipPlaneDepth() const
{
  return m_clipPlaneDepth;
}

const ViewOffsetSetting& View::offsetSetting() const
{
  return m_offset;
}

const camera::Camera& View::camera() const
{
  return m_camera;
}
camera::Camera& View::camera()
{
  return m_camera;
}
