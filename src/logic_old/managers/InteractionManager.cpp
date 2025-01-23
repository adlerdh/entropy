#include "logic/managers/InteractionManager.h"

#include "logic/camera/Camera.h"
#include "logic/camera/CameraHelpers.h"
#include "logic/camera/OrthogonalProjection.h"
#include "logic/camera/PerspectiveProjection.h"

#include "logic/camera/Camera.h"
#include "logic/interaction/CameraInteractionHandler.h"
#include "logic/interaction/CrosshairsInteractionHandler.h"
#include "logic/interaction/InteractionPack.h"
#include "logic/interaction/RefImageInteractionHandler.h"
#include "logic/interaction/SlideInteractionHandler.h"
#include "logic/interaction/StackInteractionHandler.h"
#include "logic/interaction/WindowLevelInteractionHandler.h"

#include "rendering/utility/math/MathUtility.h"

#include "common/HZeeException.hpp"
#include "common/Utility.hpp"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_inverse.hpp>
#include <glm/gtc/quaternion.hpp>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/component_wise.hpp>
#include <glm/gtx/string_cast.hpp>
#include <glm/gtx/transform.hpp>

#include <cmath>
#include <iostream>
#include <sstream>
#include <utility>

namespace
{

static const glm::mat4 sk_ident{1.0f};

/**
 * @brief Create a set of interaction objects (aka. an "interaction pack")
 * for a particular view type.
 *
 * @param[in] viewType View type
 * @param[in] cameraProjection Camera projection
 * @param[in] cameraStartFrameProvider Functional returning the camera's start frame of reference
 *
 * @return Unique pointer to the interaction pack
 */
std::unique_ptr<InteractionPack> createInteractionPack(
  const gui::ViewType& viewType,
  std::unique_ptr<camera::Projection> cameraProjection,
  GetterType<CoordinateFrame> cameraStartFrameProvider
)
{
  auto camera
    = std::make_unique<camera::Camera>(std::move(cameraProjection), cameraStartFrameProvider);

  auto cameraHandler = std::make_unique<CameraInteractionHandler>();
  auto crosshairsHandler = std::make_unique<CrosshairsInteractionHandler>();
  auto refImageHandler = std::make_unique<RefImageInteractionHandler>();
  auto stackHandler = std::make_unique<SlideStackInteractionHandler>();
  auto slideHandler = std::make_unique<SlideInteractionHandler>();
  auto windowLevelHandler = std::make_unique<WindowLevelInteractionHandler>();

  return std::make_unique<InteractionPack>(
    viewType,
    std::move(camera),
    std::move(cameraHandler),
    std::move(crosshairsHandler),
    std::move(refImageHandler),
    std::move(stackHandler),
    std::move(slideHandler),
    std::move(windowLevelHandler)
  );

  /// @todo connect minFocalPlaneFov with image diameter
}

} // namespace

/*********************** Start statics ***********************/

/// @todo Put all this in configuration files

const std::unordered_map<gui::ViewType, CrosshairsType>
  InteractionManager::smk_viewTypeToDefaultCrosshairsTypeMap{
    {gui::ViewType::Image_Axial, CrosshairsType::RefImage},
    {gui::ViewType::Image_Coronal, CrosshairsType::RefImage},
    {gui::ViewType::Image_Sagittal, CrosshairsType::RefImage},
    {gui::ViewType::Image_3D, CrosshairsType::RefImage},

    {gui::ViewType::Image_Big3D, CrosshairsType::RefImage},

    {gui::ViewType::Stack_StackSide1, CrosshairsType::SlideStack},
    {gui::ViewType::Stack_StackSide2, CrosshairsType::SlideStack},
    {gui::ViewType::Stack_ActiveSlide, CrosshairsType::SlideStack},

    {gui::ViewType::Stack_3D, CrosshairsType::SlideStack},

    {gui::ViewType::Reg_ActiveSlide, CrosshairsType::SlideStack},
    {gui::ViewType::Reg_RefImageAtSlide, CrosshairsType::SlideStack}
  };

const std::unordered_map<gui::ViewType, camera::CameraType>
  InteractionManager::smk_viewTypeToDefaultCameraTypeMap = {
    {gui::ViewType::Image_Axial, camera::CameraType::Axial},
    {gui::ViewType::Image_Coronal, camera::CameraType::Coronal},
    {gui::ViewType::Image_Sagittal, camera::CameraType::Sagittal},
    {gui::ViewType::Image_3D, camera::CameraType::Main3D},

    {gui::ViewType::Image_Big3D, camera::CameraType::Big3D},

    {gui::ViewType::Stack_StackSide1, camera::CameraType::StackSide1},
    {gui::ViewType::Stack_StackSide2, camera::CameraType::StackSide2},
    {gui::ViewType::Stack_ActiveSlide, camera::CameraType::SlideActive_TopToBottomSlide},

    {gui::ViewType::Stack_3D, camera::CameraType::Stack3D},

    {gui::ViewType::Reg_ActiveSlide, camera::CameraType::SlideActive_TopToBottomSlide},
    {gui::ViewType::Reg_RefImageAtSlide, camera::CameraType::SlideActive_TopToBottomSlide}
};

const std::unordered_map<camera::CameraType, camera::ProjectionType>
  InteractionManager::smk_cameraTypeToProjectionTypeMap = {
    {camera::CameraType::Axial, camera::ProjectionType::Orthographic},
    {camera::CameraType::Coronal, camera::ProjectionType::Orthographic},
    {camera::CameraType::Sagittal, camera::ProjectionType::Orthographic},
    {camera::CameraType::Main3D, camera::ProjectionType::Perspective},

    {camera::CameraType::Big3D, camera::ProjectionType::Perspective},

    {camera::CameraType::StackSide1, camera::ProjectionType::Orthographic},
    {camera::CameraType::StackSide2, camera::ProjectionType::Orthographic},
    {camera::CameraType::SlideActive_TopToBottomSlide, camera::ProjectionType::Orthographic},
    {camera::CameraType::SlideActive_BottomToTopSlide, camera::ProjectionType::Orthographic},

    {camera::CameraType::Stack3D, camera::ProjectionType::Perspective}
};

const std::unordered_map<camera::CameraType, CameraStartFrameType>
  InteractionManager::smk_cameraTypeToDefaultStartFrameTypeMap = {
    {camera::CameraType::Axial, CameraStartFrameType::Crosshairs_Axial_LAI},
    {camera::CameraType::Coronal, CameraStartFrameType::Crosshairs_Coronal_LSA},
    {camera::CameraType::Sagittal, CameraStartFrameType::Crosshairs_Sagittal_PSL},
    {camera::CameraType::Main3D, CameraStartFrameType::Crosshairs_Coronal_LSA},

    {camera::CameraType::Big3D, CameraStartFrameType::Crosshairs_Coronal_LSA},

    {camera::CameraType::StackSide1, CameraStartFrameType::SlideStack_FacingNegX},
    {camera::CameraType::StackSide2, CameraStartFrameType::SlideStack_FacingNegY},
    {camera::CameraType::SlideActive_TopToBottomSlide, CameraStartFrameType::SlideStack_FacingNegZ},
    {camera::CameraType::SlideActive_BottomToTopSlide, CameraStartFrameType::SlideStack_FacingPosZ},

    {camera::CameraType::Stack3D, CameraStartFrameType::SlideStack_FacingNegZ}
};

const std::unordered_map<camera::CameraType, LinkedFrameType>
  InteractionManager::smk_cameraStartFrameTypeToDefaultLinkedStartFrameTypeMap{
    {camera::CameraType::Axial, LinkedFrameType::Crosshairs},
    {camera::CameraType::Coronal, LinkedFrameType::Crosshairs},
    {camera::CameraType::Sagittal, LinkedFrameType::Crosshairs},
    {camera::CameraType::Main3D, LinkedFrameType::None},
    {camera::CameraType::Big3D, LinkedFrameType::None},
    {camera::CameraType::StackSide1, LinkedFrameType::SlideStack},
    {camera::CameraType::StackSide2, LinkedFrameType::SlideStack},
    {camera::CameraType::SlideActive_TopToBottomSlide, LinkedFrameType::SlideStack},
    {camera::CameraType::SlideActive_BottomToTopSlide, LinkedFrameType::SlideStack},
    {camera::CameraType::Stack3D, LinkedFrameType::SlideStack}
  };

const std::unordered_map<CameraStartFrameType, glm::quat>
  InteractionManager::smk_cameraStartFrameTypeToDefaultAnatomicalRotationMap = {
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

    {CameraStartFrameType::SlideStack_FacingNegX,
     glm::quat_cast(glm::mat3{0, 1, 0, 0, 0, 1, 1, 0, 0})},
    {CameraStartFrameType::SlideStack_FacingNegY,
     glm::quat_cast(glm::mat3{-1, 0, 0, 0, 0, 1, 0, 1, 0})},
    {CameraStartFrameType::SlideStack_FacingNegZ,
     glm::quat_cast(glm::mat3{1, 0, 0, 0, 1, 0, 0, 0, 1})},
    {CameraStartFrameType::SlideStack_FacingPosZ,
     glm::quat_cast(glm::mat3{1, 0, 0, 0, -1, 0, 0, 0, -1})}
};

CrosshairsType InteractionManager::getDefaultCrosshairsType(const gui::ViewType& viewType)
{
  return smk_viewTypeToDefaultCrosshairsTypeMap.at(viewType);
}

camera::CameraType InteractionManager::getDefaultCameraType(const gui::ViewType& viewType)
{
  return smk_viewTypeToDefaultCameraTypeMap.at(viewType);
}

camera::ProjectionType InteractionManager::getProjectionType(const camera::CameraType& cameraType)
{
  return smk_cameraTypeToProjectionTypeMap.at(cameraType);
}

CameraStartFrameType InteractionManager::getDefaultCameraStartFrameType(
  const camera::CameraType& cameraType
)
{
  return smk_cameraTypeToDefaultStartFrameTypeMap.at(cameraType);
}

LinkedFrameType InteractionManager::getDefaultLinkedStartFrameType(
  const camera::CameraType& cameraType
)
{
  return smk_cameraStartFrameTypeToDefaultLinkedStartFrameTypeMap.at(cameraType);
}

glm::quat InteractionManager::getDefaultAnatomicalRotation(const CameraStartFrameType& startFrameType
)
{
  return smk_cameraStartFrameTypeToDefaultAnatomicalRotationMap.at(startFrameType);
}

/*********************** End statics ***********************/

InteractionManager::InteractionManager(
  GetterType<view_type_range_t> viewUidAndTypeRangeProvider,
  GetterType<CoordinateFrame> refImageCrosshairsProvider,
  GetterType<CoordinateFrame> slideStackCrosshairsProvider,
  GetterType<CoordinateFrame> slideStackFrameProvider
)
  : m_viewTypeRangeProvider(viewUidAndTypeRangeProvider)
  ,

  m_refSpaceAABBoxProvider(nullptr)
  , m_slideStackAABBoxProvider(nullptr)
  ,

  m_crosshairsFrameProvider(refImageCrosshairsProvider)
  , m_slideStackCrosshairsFrameProvider(slideStackCrosshairsProvider)
  , m_slideStackFrameProvider(slideStackFrameProvider)
  ,

  m_interactionPacks()
  ,

  m_viewTypeToCameraTypeMap(smk_viewTypeToDefaultCameraTypeMap)
{
  initialize();
}

InteractionManager::~InteractionManager() = default;

void InteractionManager::setRefSpaceAABBoxProvider(GetterType<AABB<float> > provider)
{
  m_refSpaceAABBoxProvider = provider;
}

void InteractionManager::setSlideStackAABBoxProvider(GetterType<std::optional<AABB<float> > > provider
)
{
  m_slideStackAABBoxProvider = provider;
}

void InteractionManager::initialize()
{
  for (const auto& viewUidAndType : m_viewTypeRangeProvider())
  {
    addInteractionPackForView(viewUidAndType.first, viewUidAndType.second);
  }
}

void InteractionManager::addInteractionPackForView(const UID& viewUid, const gui::ViewType& viewType)
{
  const auto cameraType = getCameraType(viewType);
  auto cameraProj = createCameraProjection(getProjectionType(cameraType));
  auto startFrameProvider = [viewType, this]() { return computeStartFrame(viewType); };

  auto pack = createInteractionPack(viewType, std::move(cameraProj), startFrameProvider);
  m_interactionPacks.emplace(viewUid, std::move(pack));
}

void InteractionManager::setupCamerasForAABBox(const AABB<float>& worldRefAABB, float voxelSize)
{
  const glm::vec3 bboxSize = math::computeAABBoxSize(worldRefAABB);
  const glm::vec3 bboxCenter = math::computeAABBoxCenter(worldRefAABB);

  for (auto& pack : m_interactionPacks)
  {
    if (pack.second)
    {
      if (auto camera = pack.second->getCamera())
      {
        positionCameraForWorldTargetAndFov(*camera, bboxSize, bboxCenter);

        // This is the defautl near distance, but it is updated every time the camera moves
        // based on whether the camera is inside the AABB of the scene or not.
        camera->setNearDistance(1.0f * voxelSize);
      }
    }
  }
}

void InteractionManager::setCameraNearDistance(float distance)
{
  for (auto& pack : m_interactionPacks)
  {
    if (pack.second)
    {
      if (auto camera = pack.second->getCamera())
      {
        camera->setNearDistance(distance);
      }
    }
  }
}

void InteractionManager::setInteractionModeType(const InteractionModeType& mode)
{
  switch (mode)
  {
  case InteractionModeType::CrosshairsPointer:
  {
    setCrosshairsInteractionMode(CrosshairsInteractionMode::Move);
    break;
  }
  case InteractionModeType::CameraRotate:
  {
    setCameraInteractionMode(CameraInteractionMode::Rotate);
    break;
  }
  case InteractionModeType::CameraTranslate:
  {
    setCameraInteractionMode(CameraInteractionMode::Translate);
    break;
  }
  case InteractionModeType::CameraZoom:
  {
    setCameraInteractionMode(CameraInteractionMode::Zoom);
    break;
  }
  case InteractionModeType::RefImageWindowLevel:
  {
    setWindowLevelInteractionMode(WindowLevelInteractionMode::Default);
    break;
  }
  case InteractionModeType::RefImageRotate:
  {
    setRefImageInteractionMode(RefImageInteractionMode::Rotate);
    break;
  }
  case InteractionModeType::RefImageTranslate:
  {
    setRefImageInteractionMode(RefImageInteractionMode::Translate);
    break;
  }
  case InteractionModeType::StackRotate:
  {
    setStackInteractionMode(StackInteractionMode::Rotate);
    break;
  }
  case InteractionModeType::StackTranslate:
  {
    setStackInteractionMode(StackInteractionMode::Translate);
    break;
  }
  case InteractionModeType::SlideRotate:
  {
    setSlideInteractionMode(SlideInteractionMode::Rotate);
    break;
  }
  case InteractionModeType::SlideTranslate:
  {
    setSlideInteractionMode(SlideInteractionMode::Translate);
    break;
  }
  case InteractionModeType::SlideStretch:
  {
    setSlideInteractionMode(SlideInteractionMode::Stretch);
    break;
  }
  }
}

camera::Camera* InteractionManager::getCamera(const UID& viewUid)
{
  if (auto pack = getInteractionPack(viewUid))
  {
    return pack->getCamera();
  }
  else
  {
    return nullptr;
  }
}

IInteractionHandler* InteractionManager::getActiveInteractionHandler(const UID& viewUid)
{
  if (auto pack = getInteractionPack(viewUid))
  {
    return pack->getActiveHandler();
  }
  else
  {
    return nullptr;
  }
}

void InteractionManager::setCameraInteractionMode(const CameraInteractionMode& mode)
{
  for (auto& c : m_interactionPacks)
  {
    if (c.second)
    {
      c.second->setActiveHandlerType(InteractionHandlerType::Camera);

      if (auto handler = c.second->getCameraHandler())
      {
        handler->setMode(mode);
      }
    }
  }
}

void InteractionManager::setCrosshairsInteractionMode(const CrosshairsInteractionMode& mode)
{
  for (auto& c : m_interactionPacks)
  {
    if (c.second)
    {
      c.second->setActiveHandlerType(InteractionHandlerType::Crosshairs);

      if (auto handler = c.second->getCrosshairsHandler())
      {
        handler->setMode(mode);
      }
    }
  }
}

void InteractionManager::setRefImageInteractionMode(const RefImageInteractionMode& mode)
{
  for (auto& c : m_interactionPacks)
  {
    if (c.second)
    {
      c.second->setActiveHandlerType(InteractionHandlerType::RefImageTransform);

      if (auto handler = c.second->getRefImageHandler())
      {
        handler->setMode(mode);
      }
    }
  }
}

void InteractionManager::setStackInteractionMode(const StackInteractionMode& mode)
{
  for (auto& c : m_interactionPacks)
  {
    if (c.second)
    {
      c.second->setActiveHandlerType(InteractionHandlerType::StackTransform);

      if (auto handler = c.second->getStackHandler())
      {
        handler->setMode(mode);
      }
    }
  }
}

void InteractionManager::setSlideInteractionMode(const SlideInteractionMode& mode)
{
  for (auto& c : m_interactionPacks)
  {
    if (c.second)
    {
      c.second->setActiveHandlerType(InteractionHandlerType::SlideTransform);

      if (auto handler = c.second->getSlideHandler())
      {
        handler->setMode(mode);
      }
    }
  }
}

void InteractionManager::setWindowLevelInteractionMode(const WindowLevelInteractionMode& mode)
{
  for (auto& c : m_interactionPacks)
  {
    if (c.second)
    {
      c.second->setActiveHandlerType(InteractionHandlerType::WindowLevel);

      if (auto handler = c.second->getWindowLevelHandler())
      {
        handler->setMode(mode);
      }
    }
  }
}

InteractionPack* InteractionManager::getInteractionPack(const UID& viewUid)
{
  const auto pack = m_interactionPacks.find(viewUid);

  if (std::end(m_interactionPacks) == pack)
  {
    std::ostringstream ss;
    ss << "View " << viewUid << " not found" << std::ends;
    std::cerr << ss.str() << std::endl;
    return nullptr;
  }

  return pack->second.get();
}

void InteractionManager::alignCamerasToFrames()
{
  if (!m_refSpaceAABBoxProvider || !m_slideStackAABBoxProvider)
  {
    return;
  }

  for (auto& pack : m_interactionPacks)
  {
    if (!pack.second)
    {
      continue;
    }

    auto camera = pack.second->getCamera();
    if (!camera)
    {
      continue;
    }

    const auto viewType = pack.second->getViewType();
    const auto cameraType = getCameraType(viewType);
    const auto linkedFrameType = getDefaultLinkedStartFrameType(cameraType);

    // AABB in World space that is used to position the camera
    std::optional<AABB<float> > worldAABBox;

    switch (linkedFrameType)
    {
    case LinkedFrameType::Crosshairs:
    {
      worldAABBox = m_refSpaceAABBoxProvider();
      break;
    }
    case LinkedFrameType::SlideStack:
    {
      //            worldAABBox = m_slideStackAABBoxProvider();

      // Use the AABB of the ref space for views linked to the Slide Stack frame, too.
      // Otherwise it gets confusing for users.
      worldAABBox = m_refSpaceAABBoxProvider();
      break;
    }
    case LinkedFrameType::None:
    {
      // This view's camera is not linked to any frame. Since we need something,
      // let's just use the reference space to define the AABBox for the view.
      worldAABBox = m_refSpaceAABBoxProvider();
      break;
    }
    }

    if (!worldAABBox)
    {
      // Double check that the AABB is defined. It is undefined for Slide Stack
      // views when there are no slides in the stack.
      continue;
    }

    const glm::vec3 worldAABBoxCenter = (worldAABBox->first + worldAABBox->second) / 2.0f;
    const glm::vec3 worldAABBoxSize = worldAABBox->second - worldAABBox->first;

    // Reset the camera transformations to match its start frame
    /// @todo Necessary?
    resetViewTransformation(*camera);

    // Make sure that the camera is positioned to look at the AABB center (the target)
    // and to view the entire AABB of the slide stack in its FOV
    positionCameraForWorldTargetAndFov(*camera, worldAABBoxSize, worldAABBoxCenter);
  }
}

void InteractionManager::resetCameras()
{
  for (auto& pack : m_interactionPacks)
  {
    if (!pack.second)
    {
      continue;
    }

    if (auto camera = pack.second->getCamera())
    {
      resetViewTransformation(*camera);
      resetZoom(*camera);
    }
  }

  alignCamerasToFrames();
}

CoordinateFrame InteractionManager::computeStartFrame(const gui::ViewType& viewType) const
{
  static const glm::vec3 sk_origin{0.0f};

  const auto cameraType = getCameraType(viewType);
  const auto linkedFrameType = getDefaultLinkedStartFrameType(cameraType);

  CoordinateFrame anatomicalFrame(
    sk_origin, getDefaultAnatomicalRotation(getDefaultCameraStartFrameType(cameraType))
  );

  CoordinateFrame baseFrame;

  switch (linkedFrameType)
  {
  case LinkedFrameType::SlideStack:
  {
    if (m_slideStackFrameProvider)
    {
      baseFrame = m_slideStackFrameProvider();
      //            baseFrame.setWorldOrigin( m_crosshairsFrameProvider().worldOrigin() );
      //            baseFrame.setFrameToWorldRotation( m_slideStackFrameProvider().world_O_frame_rotation() );
    }
    break;
  }
  case LinkedFrameType::Crosshairs:
  case LinkedFrameType::None:
  {
    break;
  }
  }

  // Note: The transformation "world_O_frame" for this frame really maps the linked frame
  // (e.g. crosshairs or slide stack frame) to World space, i.e. world_O_linkedFrame.
  return baseFrame + anatomicalFrame;
}

void InteractionManager::applyExtraToCameras(
  const LinkedFrameType& linkedFrameType, const glm::mat4& extra
)
{
  for (auto& pack : m_interactionPacks)
  {
    if (!pack.second)
    {
      continue;
    }

    // Apply transformation only to cameras linked to the specified frame type
    const auto cameraType = getCameraType(pack.second->getViewType());
    if (linkedFrameType != getDefaultLinkedStartFrameType(cameraType))
    {
      continue;
    }

    if (auto camera = pack.second->getCamera())
    {
      camera->set_frameA_O_world(camera->frameA_O_world() * extra);
    }
  }
}

void InteractionManager::setActiveSlideViewDirection(
  const InteractionManager::ActiveSlideViewDirection& dir
)
{
  switch (dir)
  {
  case ActiveSlideViewDirection::TopToBottomSlide:
  {
    m_viewTypeToCameraTypeMap[gui::ViewType::Stack_ActiveSlide]
      = camera::CameraType::SlideActive_TopToBottomSlide;
    break;
  }
  case ActiveSlideViewDirection::BottomToTopSlide:
  {
    m_viewTypeToCameraTypeMap[gui::ViewType::Stack_ActiveSlide]
      = camera::CameraType::SlideActive_BottomToTopSlide;
    break;
  }
  }
}

InteractionManager::ActiveSlideViewDirection InteractionManager::getActiveSlideViewDirection() const
{
  auto cameraType = m_viewTypeToCameraTypeMap.at(gui::ViewType::Stack_ActiveSlide);

  if (camera::CameraType::SlideActive_TopToBottomSlide == cameraType)
  {
    return InteractionManager::ActiveSlideViewDirection::TopToBottomSlide;
  }
  else if (camera::CameraType::SlideActive_BottomToTopSlide == cameraType)
  {
    return InteractionManager::ActiveSlideViewDirection::BottomToTopSlide;
  }
  else
  {
    throw("Invalid view direction for Active Slide");
  }
}

camera::CameraType InteractionManager::getCameraType(const gui::ViewType& viewType) const
{
  auto it = m_viewTypeToCameraTypeMap.find(viewType);
  if (std::end(m_viewTypeToCameraTypeMap) != it)
  {
    return it->second;
  }
  else
  {
    throw_debug("Unable to find camera for view type");
  }
}

CrosshairsType InteractionManager::getCrosshairsType(const gui::ViewType& viewType) const
{
  auto it = smk_viewTypeToDefaultCrosshairsTypeMap.find(viewType);
  if (std::end(smk_viewTypeToDefaultCrosshairsTypeMap) != it)
  {
    return it->second;
  }
  else
  {
    throw_debug("Unable to find crosshairs type for view type");
  }
}
