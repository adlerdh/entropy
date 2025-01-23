#include "logic/data/DataHelper.h"

#include "logic/camera/Camera.h"
#include "logic/camera/CameraHelpers.h"
#include "logic/managers/DataManager.h"

#include "rendering/utility/math/MathUtility.h"
#include "slideio/SlideHelper.h"

#include <glm/glm.hpp>

#include <limits>

namespace
{

// The default reference space is centered at the World origin and has side length 200.0 units
static constexpr float X = 100.0f;
static const AABB<float> sk_defaultRefSpaceAABBox{glm::vec3{-X}, glm::vec3{X}};

// The default voxel scale is 1.0 units
static constexpr float sk_defaultRefSpaceVoxelScale = 1.0f;

static constexpr float sk_defaultSliceScrollDistance = sk_defaultRefSpaceVoxelScale;

// Amount by which to pad the AABB of the slide stack and images in order to create
// the reference space (0.0 means that no padding is applied)
static constexpr float sk_refSpacePadFraction = 0.02f;

static constexpr float sk_defaultSlideStackHeight = 10.0f;

static constexpr float sk_pageToSingleStepRatio = 10.0f;

static constexpr bool sk_disabledSlider = false;

static const gui::ViewSliderParams
  sk_defaultViewSliderParams{0.0, 1.0, 1.0, 1.0, 0.0, sk_disabledSlider};

} // namespace

namespace data
{

AABB<float> refSpaceAABBox(DataManager& dataManager, const glm::mat4& world_O_slideStack)
{
  const auto imageAABB = activeRefImageAABBox(dataManager);
  const auto stackAABB
    = slideio::slideStackAABBoxInWorld(dataManager.slideRecords(), world_O_slideStack);

  AABB<float> box;

  if (imageAABB && stackAABB)
  {
    box = math::computeBoundingAABBox(*imageAABB, *stackAABB);
  }
  else if (imageAABB)
  {
    box = *imageAABB;
  }
  else if (stackAABB)
  {
    box = *stackAABB;
  }
  else
  {
    box = sk_defaultRefSpaceAABBox;
  }

  const glm::vec3 pad = sk_refSpacePadFraction * math::computeAABBoxSize(box);
  box.first -= pad;
  box.second += pad;

  return box;
}

std::optional<AABB<float> > activeRefImageAABBox(DataManager& dataManager)
{
  auto record = dataManager.activeImageRecord().lock();
  if (record && record->cpuData())
  {
    const auto aabb = record->cpuData()->header().m_boundingBoxMinMaxCorners;
    return std::make_pair(glm::vec3{aabb.first}, glm::vec3{aabb.second});
  }

  return std::nullopt;
}

std::optional<CoordinateFrame> getActiveImageSubjectToWorldFrame(DataManager& dataManager)
{
  auto record = dataManager.activeImageRecord().lock();
  if (record && record->cpuData())
  {
    const auto& T = record->cpuData()->transformations();
    glm::vec3 origin = T.getWorldSubjectOrigin();
    glm::quat rotation = T.getSubjectToWorldRotation();
    return CoordinateFrame(std::move(origin), std::move(rotation));
  }

  return std::nullopt;
}

void setActiveImageSubjectToWorldFrame(
  DataManager& dataManager, const CoordinateFrame& world_O_subject
)
{
  auto record = dataManager.activeImageRecord().lock();
  if (record && record->cpuData())
  {
    record->cpuData()->setWorldSubjectOrigin(world_O_subject.worldOrigin());
    record->cpuData()->setSubjectToWorldRotation(world_O_subject.world_O_frame_rotation());
  }
}

float refSpaceVoxelScale(DataManager& dataManager)
{
  // Define reference space voxel scale as the diagonal length of voxels in the active image
  if (auto record = dataManager.activeImageRecord().lock())
  {
    if (auto r = record->cpuData())
    {
      return static_cast<float>(glm::length(r->header().m_spacing));
    }
  }

  return sk_defaultRefSpaceVoxelScale;
}

float refSpaceSliceScrollDistance(DataManager& dataManager, const glm::vec3& worldCameraFrontDir)
{
  auto activeImage = dataManager.activeImageRecord().lock();
  if (!activeImage)
  {
    return sk_defaultSliceScrollDistance;
  }

  auto record = activeImage->cpuData();
  if (!record)
  {
    return sk_defaultSliceScrollDistance;
  }

  // Scroll in image Pixel space along the camera's front direction:
  const glm::mat3 pixel_O_world_IT{record->transformations().pixel_O_world_invTranspose()};
  const glm::vec3 pixelDir = glm::abs(glm::normalize(pixel_O_world_IT * worldCameraFrontDir));

  // Scroll distance is proportional to spacing of image along the view direction
  return std::abs(glm::dot(glm::vec3{record->header().m_spacing}, pixelDir));
}

float slideStackPositiveExtent(DataManager& dataManager)
{
  return slideio::slideStackPositiveExtent(dataManager.slideRecords());
}

bool isSlideActive(DataManager& dataManager, const UID& slideUid)
{
  if (const auto activeUid = dataManager.activeSlideUid())
  {
    return (slideUid == *activeUid);
  }
  return false;
}

gui::ViewSliderParams defaultViewSliderParams()
{
  return sk_defaultViewSliderParams;
}

std::pair<gui::ViewSliderParams, gui::ViewSliderParams> viewScrollBarParams(
  DataManager& dataManager,
  const glm::vec3& worldCrosshairsOrigin,
  const glm::mat4& world_O_slideStack,
  const camera::Camera& camera
)
{
  // The planes are ordered as follows:
  // [0] right, [1] top, [2] left, [3] bottom, [4] near, [5] far.
  // The plane normal vectors point outwards from the frustum.
  static constexpr uint RIGHT = 0;
  static constexpr uint TOP = 1;
  static constexpr uint LEFT = 2;
  static constexpr uint BOTTOM = 3;

  const auto worldPlanes = worldFrustumPlanes(camera);

  // All eight corners of the reference space AABB:
  const auto worldAABBCorners = math::makeAABBoxCorners(
    refSpaceAABBox(dataManager, world_O_slideStack)
  );

  // Compute the AABB corners that are farthest out w.r.t. the right, top, left,
  // and bottom planes of the view frustum. Also compute the distances from
  // these points to their respective planes.
  glm::vec3 rightmost, topmost, leftmost, bottommost;
  float rightDist, topDist, leftDist, bottomDist;

  std::tie(std::ignore, std::ignore, rightmost, rightDist)
    = math::computeNearAndFarAABBoxCorners(worldAABBCorners, worldPlanes[RIGHT]);

  std::tie(std::ignore, std::ignore, topmost, topDist)
    = math::computeNearAndFarAABBoxCorners(worldAABBCorners, worldPlanes[TOP]);

  std::tie(std::ignore, std::ignore, leftmost, leftDist)
    = math::computeNearAndFarAABBoxCorners(worldAABBCorners, worldPlanes[LEFT]);

  std::tie(std::ignore, std::ignore, bottommost, bottomDist)
    = math::computeNearAndFarAABBoxCorners(worldAABBCorners, worldPlanes[BOTTOM]);

  // min/max values of the horizontal scroll bars
  const double xMin = static_cast<double>(std::min(-leftDist, 0.0f));
  const double xMax = static_cast<double>(std::max(rightDist, 0.0f));

  // min/max values of the vertical scroll bars
  const double yMin = static_cast<double>(std::min(-bottomDist, 0.0f));
  const double yMax = static_cast<double>(std::max(topDist, 0.0f));

  // NDC z coordinate at which to compute view frustum's FOV
  float ndcZ;

  if (camera.isOrthographic())
  {
    // Irrelevant for orthographic projections, since the FOV is constant
    // at all camera depths
    ndcZ = -1.0f;
  }
  else
  {
    // For perspective projections, use the depth of the crosshairs origin
    const glm::vec3 cameraPos = camera_O_world(camera, worldCrosshairsOrigin);

    if (cameraPos.z >= 0.0f)
    {
      ndcZ = -1.0f;
    }
    else
    {
      ndcZ = glm::clamp(ndc_O_world(camera, worldCrosshairsOrigin).z, -1.0f, 1.0f);
    }
  }

  const glm::vec3 ndcFarR{1.0f, 0.0f, ndcZ};
  const glm::vec3 ndcFarT{0.0f, 1.0f, ndcZ};
  const glm::vec3 ndcFarL{-1.0f, 0.0f, ndcZ};
  const glm::vec3 ndcFarB{0.0f, -1.0f, ndcZ};

  const glm::vec3 cameraFarR = camera_O_ndc(camera, ndcFarR);
  const glm::vec3 cameraFarT = camera_O_ndc(camera, ndcFarT);
  const glm::vec3 cameraFarL = camera_O_ndc(camera, ndcFarL);
  const glm::vec3 cameraFarB = camera_O_ndc(camera, ndcFarB);

  // Note: We could just double the distance to frustum center,
  // if the frustum is symmetric. This works more generally.
  const double xPageStep = static_cast<double>(glm::distance(cameraFarL, cameraFarR));
  const double yPageStep = static_cast<double>(glm::distance(cameraFarB, cameraFarT));

  const double sk_singleStep = static_cast<double>(refSpaceVoxelScale(dataManager));

  static constexpr bool xEnabled = true;
  static constexpr bool yEnabled = true;

  // Scroll bar values are always zero
  static constexpr double sk_value = 0.0;

  const gui::ViewSliderParams xParams{xMin, xMax, sk_singleStep, xPageStep, sk_value, xEnabled};
  const gui::ViewSliderParams yParams{yMin, yMax, sk_singleStep, yPageStep, sk_value, yEnabled};

  return {xParams, yParams};
}

gui::ViewSliderParams viewSliceSliderParams(
  DataManager& dataManager,
  const glm::vec3& worldCrosshairsOrigin,
  const glm::mat4& world_O_slideStack,
  const camera::Camera& camera
)
{
  const auto worldAABBCorners = math::makeAABBoxCorners(
    refSpaceAABBox(dataManager, world_O_slideStack)
  );

  // Define a plane at the current world crosshairs position that is facing the
  // front direction of the view camera frustum

  // For an orthographic camera, this is equivalent to
  // worldDirection( *pack->m_camera, Directions::View::Front )
  const glm::vec3 worldFrontDir
    = worldRayDirection(camera, glm::vec2{ndc_O_world(camera, worldCrosshairsOrigin)});

  const glm::vec4 crosshairsPlane = math::makePlane(worldFrontDir, worldCrosshairsOrigin);

  // Near and far AABB corners w.r.t. the camera:
  glm::vec3 nearCorner, farCorner;
  std::tie(nearCorner, std::ignore, farCorner, std::ignore)
    = math::computeNearAndFarAABBoxCorners(worldAABBCorners, crosshairsPlane);

  // Distance between near and far corners of AABB w.r.t. the camera front direction
  const float nearFarDistance = glm::dot(worldFrontDir, farCorner - nearCorner);

  // Distance from the world position to the near corner along the front direction
  const float worldPosDistance = glm::dot(worldFrontDir, worldCrosshairsOrigin - nearCorner);

  if (nearFarDistance < 0.0f)
  {
    // The near-far corner distance must not be negative
    return sk_defaultViewSliderParams;
  }

  // Scroll step size along the (normalized) front direction.
  const float singleStep = refSpaceSliceScrollDistance(dataManager, worldFrontDir);
  const float pageStep = sk_pageToSingleStepRatio * singleStep;

  // The slider value is equal to the clamped world position distance
  const float value = glm::clamp(worldPosDistance, 0.0f, nearFarDistance);

  static constexpr double sk_sliderMinValue = 0.0;
  static constexpr bool sk_enabledSlider = true;

  return gui::ViewSliderParams{
    sk_sliderMinValue,
    static_cast<double>(nearFarDistance),
    static_cast<double>(singleStep),
    static_cast<double>(pageStep),
    static_cast<double>(value),
    sk_enabledSlider
  };
}

} // namespace data
