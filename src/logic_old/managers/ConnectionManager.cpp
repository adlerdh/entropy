#include "logic/managers/ConnectionManager.h"

#include "common/CoordinateFrame.h"
#include "common/HZeeException.hpp"

#include "logic/managers/ActionManager.h"
#include "logic/managers/AssemblyManager.h"
#include "logic/managers/DataManager.h"
#include "logic/managers/GuiManager.h"
#include "logic/managers/InteractionManager.h"
#include "logic/managers/LayoutManager.h"
#include "logic/managers/TransformationManager.h"

#include "logic/camera/Camera.h"
#include "logic/camera/CameraHelpers.h"
#include "logic/interaction/CameraInteractionHandler.h"
#include "logic/interaction/CrosshairsInteractionHandler.h"
#include "logic/interaction/InteractionPack.h"
#include "logic/interaction/RefImageInteractionHandler.h"
#include "logic/interaction/SlideInteractionHandler.h"
#include "logic/interaction/StackInteractionHandler.h"
#include "logic/interaction/WindowLevelInteractionHandler.h"

#include "logic/ui/ImageDataUiMapper.h"
#include "logic/ui/ParcellationDataUiMapper.h"
#include "logic/ui/SlideStackDataUiMapper.h"

#include "logic/data/DataHelper.h"
#include "logic/records/ImageRecord.h"
#include "logic/records/SlideRecord.h"

#include "gui/view/ViewWidget.h"

#include "rendering/common/DrawableScaling.h"
#include "rendering/interfaces/IRenderer.h"
#include "rendering/utility/math/MathUtility.h"

#include "slideio/SlideHelper.h"

#include <glm/glm.hpp>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/component_wise.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/gtx/transform.hpp>

#include <boost/range/adaptor/map.hpp>
#include <boost/range/algorithm/copy.hpp>
#include <boost/signals2.hpp>

#include <iostream>
#include <optional>
#include <set>
#include <string>
#include <unordered_map>

namespace
{

// Collections of view types that synchronize their zoom factors
static const std::set<std::set<gui::ViewType> > sk_viewTypesThatSynchZoom = {
  // These synchronize to each other:
  {gui::ViewType::Image_Axial, gui::ViewType::Image_Coronal, gui::ViewType::Image_Sagittal},

  // These synchronize to each other:
  {gui::ViewType::Stack_StackSide1, gui::ViewType::Stack_StackSide2},

  // These synchronize to each other:
  {gui::ViewType::Reg_ActiveSlide, gui::ViewType::Reg_RefImageAtSlide}
};

} // namespace

struct ConnectionManager::Impl
{
  Impl(
    ActionManager& actionManager,
    AssemblyManager& assemblyManager,
    DataManager& dataManager,
    GuiManager& guiManager,
    InteractionManager& interactionManager,
    LayoutManager& layoutManager,
    TransformationManager& txManager,
    ImageDataUiMapper& imageUiMapper,
    ParcellationDataUiMapper& parcelUiMapper,
    SlideStackDataUiMapper& slideStackUiMapper,
    ViewWidgetProviderType viewWidgetProvider,
    SceneTypeProviderType sceneTypeProvider,
    GetterType<view_type_range_t> viewUidAndTypeRangeProvider,
    ViewsOfTypeProviderType viewsOfTypeProvider,
    InteractionPackProviderType interactionPackProvider
  );

  ActionManager& m_actionManager;
  AssemblyManager& m_assemblyManager;
  DataManager& m_dataManager;
  GuiManager& m_guiManager;
  InteractionManager& m_interactionManager;
  LayoutManager& m_layoutManager;
  TransformationManager& m_txManager;

  ImageDataUiMapper& m_imageUiMapper;
  ParcellationDataUiMapper& m_parcelUiMapper;
  SlideStackDataUiMapper& m_slideStackUiMapper;

  ViewWidgetProviderType m_viewWidgetProvider;
  SceneTypeProviderType m_sceneTypeProvider;
  GetterType<view_type_range_t> m_viewTypeRangeProvider;
  ViewsOfTypeProviderType m_viewsOfTypeProvider;
  InteractionPackProviderType m_interactionPackProvider;

  /// Signal that an image's window and level settings have changed
  boost::signals2::signal<void(const UID& imageUid)> m_signalImageWindowLevelChanged;

  /// Signal that an image's transformation from Subject to World space has changed
  boost::signals2::signal<void(const UID& imageUid)> m_signalImageTransformationChanged;

  /// Signal that the slide stack frame has chnaged
  boost::signals2::signal<void(const CoordinateFrame& slideStackFrame)>
    m_signalSlideStackFrameChanged;

  void createActionConnections();
  void createAssemblyConnections();
  void createInteractionConnections();
  void createRendererUpdateConnections();
  void createUiMapperConnections();

  // Callbacks:

  void handleCrosshairsChanged(const CoordinateFrame& crosshairs);
  void handleCrosshairsChangeDone(const CoordinateFrame& crosshairs);

  void handleStackFrameChanged(const CoordinateFrame& stackFrame);
  void handleStackFrameChangeDone(const CoordinateFrame& stackFrame);
};

ConnectionManager::ConnectionManager(
  ActionManager& actionManager,
  AssemblyManager& assemblyManager,
  DataManager& dataManager,
  GuiManager& guiManager,
  InteractionManager& interactionManager,
  LayoutManager& layoutManager,
  TransformationManager& txManager,
  ImageDataUiMapper& imageUiMapper,
  ParcellationDataUiMapper& parcelUiMapper,
  SlideStackDataUiMapper& slideStackUiMapper,
  ViewWidgetProviderType viewWidgetProvider,
  SceneTypeProviderType sceneTypeProvider,
  GetterType<view_type_range_t> viewUidAndTypeRangeProvider,
  ViewsOfTypeProviderType viewsOfTypeProvider,
  InteractionPackProviderType interactionPackProvider
)
  : m_impl(std::make_unique<Impl>(
    actionManager,
    assemblyManager,
    dataManager,
    guiManager,
    interactionManager,
    layoutManager,
    txManager,
    imageUiMapper,
    parcelUiMapper,
    slideStackUiMapper,
    viewWidgetProvider,
    sceneTypeProvider,
    viewUidAndTypeRangeProvider,
    viewsOfTypeProvider,
    interactionPackProvider
  ))
{
  if (!viewWidgetProvider)
  {
    throw_debug("viewWidgetProvider NULL");
  }
}

ConnectionManager::~ConnectionManager() = default;

void ConnectionManager::createConnections()
{
  if (m_impl)
  {
    m_impl->createActionConnections();
    m_impl->createAssemblyConnections();
    m_impl->createInteractionConnections();
    m_impl->createRendererUpdateConnections();
    m_impl->createUiMapperConnections();
  }
}

ConnectionManager::Impl::Impl(
  ActionManager& actionManager,
  AssemblyManager& assemblyManager,
  DataManager& dataManager,
  GuiManager& guiManager,
  InteractionManager& interactionManager,
  LayoutManager& layoutManager,
  TransformationManager& txManager,
  ImageDataUiMapper& imageUiMapper,
  ParcellationDataUiMapper& parcelUiMapper,
  SlideStackDataUiMapper& slideStackUiMapper,
  ViewWidgetProviderType viewWidgetProvider,
  SceneTypeProviderType sceneTypeProvider,
  GetterType<view_type_range_t> viewUidAndTypeRangeProvider,
  ViewsOfTypeProviderType viewsOfTypeProvider,
  InteractionPackProviderType interactionPackProvider
)
  : m_actionManager(actionManager)
  , m_assemblyManager(assemblyManager)
  , m_dataManager(dataManager)
  , m_guiManager(guiManager)
  , m_interactionManager(interactionManager)
  , m_layoutManager(layoutManager)
  , m_txManager(txManager)
  ,

  m_imageUiMapper(imageUiMapper)
  , m_parcelUiMapper(parcelUiMapper)
  , m_slideStackUiMapper(slideStackUiMapper)
  ,

  m_viewWidgetProvider(viewWidgetProvider)
  , m_sceneTypeProvider(sceneTypeProvider)
  , m_viewTypeRangeProvider(viewUidAndTypeRangeProvider)
  , m_viewsOfTypeProvider(viewsOfTypeProvider)
  , m_interactionPackProvider(interactionPackProvider)
{
}

void ConnectionManager::Impl::createActionConnections()
{
  auto slideStackFrameProvider = [this](void)
  { return m_txManager.getSlideStackFrame(TransformationState::Staged); };

  auto crosshairsFrameProvider = [this](void)
  { return m_txManager.getCrosshairsFrame(TransformationState::Staged); };

  m_actionManager.setSlideStackFrameProvider(slideStackFrameProvider);
  m_actionManager.setCrosshairsFrameProvider(crosshairsFrameProvider);

  m_actionManager.setCrosshairsFrameChangedBroadcaster([this](const CoordinateFrame& frame)
                                                       { this->handleCrosshairsChanged(frame); });

  m_actionManager.setCrosshairsFrameChangeDoneBroadcaster(
    [this](const CoordinateFrame& frame) { this->handleCrosshairsChangeDone(frame); }
  );
}

void ConnectionManager::Impl::createUiMapperConnections()
{
  // Connect signal that image window/level has changed to slot that updates the UI:
  auto slot_updateUiFromImageWindowLevelChange = [this](const UID& imageUid)
  { m_imageUiMapper.slot_updateUiFromImageWindowLevelChange(imageUid); };

  // Connect signal that image transformation has changed to slot that updates the UI:
  auto slot_updateUiFromImageTransformationChange = [this](const UID& imageUid)
  { m_imageUiMapper.slot_updateUiFromImageTransformationChange(imageUid); };

  // Connect signal that slide stack frame has changed to slot that updates UI:
  auto slot_updateUiFromSlideStackFrameChange = [this](const CoordinateFrame& /*stackFrame*/)
  { m_slideStackUiMapper.updateUiFromSlideStackFrameChange(); };

  m_signalImageWindowLevelChanged.connect(slot_updateUiFromImageWindowLevelChange);
  m_signalImageTransformationChanged.connect(slot_updateUiFromImageTransformationChange);
  m_signalSlideStackFrameChanged.connect(slot_updateUiFromSlideStackFrameChange);

  /// @todo Put into class function
  auto slideStackFrameProvider = [this](void)
  { return m_txManager.getSlideStackFrame(TransformationState::Staged); };

  auto centerCrosshairsOnSlide = [this](const UID& slideUid)
  { m_actionManager.centerCrosshairsOnSlide(slideUid); };

  m_slideStackUiMapper.setSlideStackFrameProvider(slideStackFrameProvider);

  m_slideStackUiMapper.setSlideStackFrameChangeDoneBroadcaster(
    [this](const CoordinateFrame& frame) { this->handleStackFrameChangeDone(frame); }
  );

  m_slideStackUiMapper.setCrosshairsToSlideCenterMover(centerCrosshairsOnSlide);
}

void ConnectionManager::Impl::createAssemblyConnections()
{
  using std::placeholders::_1;

  // Function returning the transformation from Slide Stack to World sapce
  auto slideStackFrameToWorldProvider = [this](void)
  { return m_txManager.getSlideStackFrame(TransformationState::Staged).world_O_frame(); };

  // Function returning the transformation from a label mesh's local modeling coordinates
  // to World space
  auto labelMeshToWorldTxQuerier = [this](const UID& labelMeshUid) -> std::optional<glm::mat4>
  {
    // 1) If there is an active image, use its world_O_subject transformation.
    if (auto activeImage = m_dataManager.activeImageRecord().lock())
    {
      if (auto r = activeImage->cpuData())
      {
        return r->transformations().world_O_subject();
      }
    }

    // 2) Otherwise, use world_O_subject of the parcellation image corresponding to this mesh.
    if (auto parcelUid = m_dataManager.parcellationUid_of_labelMesh(labelMeshUid))
    {
      auto parcelRecord = m_dataManager.parcellationRecord(*parcelUid).lock();
      if (parcelRecord && parcelRecord->cpuData())
      {
        return parcelRecord->cpuData()->transformations().world_O_subject();
      }
    }

    return std::nullopt;
  };

  // Function returning the transformation from an isosurface mesh's local modeling coordinates
  // to World space
  auto isoMeshToWorldTxQuerier = [this](const UID& isoMeshUid) -> std::optional<glm::mat4>
  {
    // Return world_O_subject of the mesh's associated reference image
    if (auto imageUid = m_dataManager.imageUid_of_isoMesh(isoMeshUid))
    {
      auto imageRecord = m_dataManager.imageRecord(*imageUid).lock();
      if (imageRecord && imageRecord->cpuData())
      {
        return imageRecord->cpuData()->transformations().world_O_subject();
      }
    }

    return std::nullopt;
  };

  // Function returning the matrix transformation from active Subject to World space.
  // Returns std::nullopt if there is no active image.
  auto activeSubjectToWorldProvider = [this]() -> std::optional<glm::mat4>
  {
    // Return world_O_subject of the active image
    if (auto imageRecord = m_dataManager.activeImageRecord().lock())
    {
      if (imageRecord->cpuData())
      {
        return imageRecord->cpuData()->transformations().world_O_subject();
      }
    }

    return std::nullopt;
  };

  auto getRefImageRecordFromLmGroup = [this](const UID& lmGroupUid) -> imageio::ImageCpuRecord*
  {
    if (const auto imageUid = m_dataManager.imageUid_of_landmarkGroup(lmGroupUid))
    {
      if (auto imageRecord = m_dataManager.imageRecord(*imageUid).lock())
      {
        return imageRecord->cpuData();
      }
    }
    return nullptr;
  };

  auto getSlideRecordFromLmGroup = [this](const UID& lmGroupUid) -> slideio::SlideCpuRecord*
  {
    if (const auto slideUid = m_dataManager.slideUid_of_landmarkGroup(lmGroupUid))
    {
      if (auto slideRecord = m_dataManager.slideRecord(*slideUid).lock())
      {
        return slideRecord->cpuData();
      }
    }
    return nullptr;
  };

  auto getSlideRecordFromAnnotation = [this](const UID& annotUid) -> slideio::SlideCpuRecord*
  {
    if (const auto slideUid = m_dataManager.slideUid_of_annotation(annotUid))
    {
      if (auto slideRecord = m_dataManager.slideRecord(*slideUid).lock())
      {
        return slideRecord->cpuData();
      }
    }
    return nullptr;
  };

  // Function that returns the world_O_subject transformation for the reference image
  // associated with a given landmark group.
  auto refImageLmGroupToWorldTxQuerier = [getRefImageRecordFromLmGroup](const UID& lmGroupUid
                                         ) -> std::optional<std::pair<glm::mat4, glm::mat4> >
  {
    if (const auto* image = getRefImageRecordFromLmGroup(lmGroupUid))
    {
      return std::make_pair(
        image->transformations().world_O_subject(), image->transformations().world_O_subject()
      );
    }
    return std::nullopt;
  };

  // Function that returns the world_O_slide transformation for the slide
  // associated with a given landmark group.
  auto slideLmGroupToWorldTxQuerier = [this, getSlideRecordFromLmGroup](const UID& slideLmGroupUid
                                      ) -> std::optional<std::pair<glm::mat4, glm::mat4> >
  {
    if (auto* slide = getSlideRecordFromLmGroup(slideLmGroupUid))
    {
      // world_O_slide = world_O_slideStack * slideStack_O_slide
      const auto world_O_frame = m_txManager.getSlideStackFrame(TransformationState::Staged)
                                   .world_O_frame();

      return std::make_pair(
        world_O_frame * slideio::stack_O_slide(*slide),
        world_O_frame * slideio::stack_O_slide_rigid(*slide)
      );
    }

    return std::nullopt;
  };

  // Function that returns the world_O_slide transformation for the slide
  // associated with a given annotation.
  auto slideAnnotationToWorldTxQuerier = [this, getSlideRecordFromAnnotation](const UID& annotUid
                                         ) -> std::optional<std::pair<glm::mat4, glm::mat4> >
  {
    if (auto* slide = getSlideRecordFromAnnotation(annotUid))
    {
      // world_O_slide = world_O_slideStack * slideStack_O_slide
      const auto world_O_frame = m_txManager.getSlideStackFrame(TransformationState::Staged)
                                   .world_O_frame();

      return std::make_pair(
        world_O_frame * slideio::stack_O_slide(*slide),
        world_O_frame * slideio::stack_O_slide_rigid(*slide)
      );
    }

    return std::nullopt;
  };

  auto getRefImageLmScaling =
    [/*getRefImageRecordFromLmGroup*/](const UID& /*lmGroupUid*/) -> DrawableScaling
  {
    // Scale radius to 5 pixels by default
    static const AxisScaling S{5.0f, ScalingMode::FixedInViewPixels};

    DrawableScaling scaling{S, S, S};

    //        if ( const auto* image = getRefImageRecordFromLmGroup( lmGroupUid ) )
    //        {
    //            const float x = static_cast<float>( glm::compMin( image->header().m_spacing ) );
    //            scaling[0] = AxisScaling{ x, ScalingMode::FixedInPhysicalWorld };
    //            scaling[1] = AxisScaling{ x, ScalingMode::FixedInPhysicalWorld };
    //            scaling[2] = AxisScaling{ x, ScalingMode::FixedInPhysicalWorld };
    //        }

    return scaling;
  };

  auto getSlideLmScaling = [getSlideRecordFromLmGroup](const UID& lmGroupUid) -> DrawableScaling
  {
    // Scale radius 5 pixels by default in x and y axes.
    static const AxisScaling S{5.0f, ScalingMode::FixedInViewPixels};

    DrawableScaling scaling{S, S, S};

    // If the slide exists, then use its thickness for z axis scaling.
    if (const auto* slide = getSlideRecordFromLmGroup(lmGroupUid))
    {
      scaling[2] = AxisScaling{slide->header().thickness(), ScalingMode::FixedInPhysicalWorld};
    }

    return scaling;
  };

  // Get thickness of slide associated with an annotation.
  // Returns std::nullopt if the slide doesn't exist.
  auto getSlideThickness = [this](const UID& annotUid) -> std::optional<float>
  {
    auto slideUid = m_dataManager.slideUid_of_annotation(annotUid);
    if (!slideUid)
    {
      return std::nullopt;
    }

    auto slide = m_dataManager.slideRecord(*slideUid).lock();
    if (!slide || !slide->cpuData())
    {
      return std::nullopt;
    }

    return slide->cpuData()->header().thickness();
  };

  // Set function returning the positive extent of the slide stack:
  m_assemblyManager.setSlideStackHeightProvider(
    std::bind(&data::slideStackPositiveExtent, std::ref(m_dataManager))
  );

  // Set function that queries whether a given slide is active or not:
  m_assemblyManager.setActiveSlideQuerier(
    std::bind(&data::isSlideActive, std::ref(m_dataManager), _1)
  );

  // Set function that queries the transformation from a reference image landmark group to World space
  m_assemblyManager.setRefImageLandmarkGroupToWorldTxQuerier(refImageLmGroupToWorldTxQuerier);

  // Set function that queries the transformation from a slide landmark group to World space
  m_assemblyManager.setSlideLandmarkGroupToWorldTxQuerier(slideLmGroupToWorldTxQuerier);

  // Set function that queries the transformation from a slide annotation to World space
  m_assemblyManager.setSlideAnnotationToWorldTxQuerier(slideAnnotationToWorldTxQuerier);

  // Set function that queries landmark scaling information:
  m_assemblyManager.setRefImageLandmarkGroupScalingQuerier(getRefImageLmScaling);

  // Set function that queries landmark scaling information:
  m_assemblyManager.setSlideLandmarkGroupScalingQuerier(getSlideLmScaling);

  // Set function that queries slide thickness from annotation:
  m_assemblyManager.setSlideAnnotationThicknessQuerier(getSlideThickness);

  m_assemblyManager.setSlideStackToWorldTxProvider(slideStackFrameToWorldProvider);
  m_assemblyManager.setActiveSubjectToWorldProvider(activeSubjectToWorldProvider);

  m_assemblyManager.setLabelMeshSubjectToWorldTxQuerier(labelMeshToWorldTxQuerier);
  m_assemblyManager.setIsoSurfaceMeshSubjectToWorldTxQuerier(isoMeshToWorldTxQuerier);
}

void ConnectionManager::Impl::createInteractionConnections()
{
  using std::placeholders::_1;
  using std::placeholders::_2;

  // Note: Views update with committed transformation state
  static const TransformationState STAGED = TransformationState::Staged;
  static const TransformationState COMMITTED = TransformationState::Committed;

  m_guiManager.setInteractionModeSetter(
    std::bind(&InteractionManager::setInteractionModeType, &m_interactionManager, _1)
  );
  m_guiManager.setCrosshairsToActiveSlideAligner(
    std::bind(&ActionManager::alignCrosshairsToActiveSlide, &m_actionManager)
  );
  m_guiManager.setCrosshairsToSlideStackFrameAligner(
    std::bind(&ActionManager::alignCrosshairsToSlideStackFrame, &m_actionManager)
  );
  m_guiManager.setCrosshairsToAnatomicalPlanesAligner(
    std::bind(&ActionManager::alignCrosshairsToSubjectXyzPlanes, &m_actionManager)
  );
  m_guiManager.setAllViewsResetter(std::bind(&ActionManager::resetViews, &m_actionManager));

  m_guiManager.setProjectSaver([this](const std::optional<std::string>& fileName)
                               { m_actionManager.saveProject(fileName); });

  m_guiManager.setImageLoader(std::bind(&ActionManager::loadImage, &m_actionManager, _1, _2));
  m_guiManager.setParcellationLoader(
    std::bind(&ActionManager::loadParcellation, &m_actionManager, _1, _2)
  );
  m_guiManager.setSlideLoader(std::bind(&ActionManager::loadSlide, &m_actionManager, _1, _2));

  /// @todo Tool button for this? It's already in the dock
  m_guiManager.setSlideStackView3dModeSetter(nullptr);

  auto getRefSpaceAABBox = [this]()
  {
    const auto world_O_slideStack = m_txManager.getSlideStackFrame(COMMITTED).world_O_frame();
    return data::refSpaceAABBox(m_dataManager, world_O_slideStack);
  };

  auto getSlideStackAABBox = [this]()
  {
    const auto world_O_slideStack = m_txManager.getSlideStackFrame(COMMITTED).world_O_frame();
    return slideio::slideStackAABBoxInWorld(m_dataManager.slideRecords(), world_O_slideStack);
  };

  m_interactionManager.setRefSpaceAABBoxProvider(getRefSpaceAABBox);
  m_interactionManager.setSlideStackAABBoxProvider(getSlideStackAABBox);

  auto crosshairsOriginProvider = [this](const TransformationState& state)
  {
    // Return the committed frame, since cameras are affected:
    return m_txManager.getCrosshairsFrame(state).worldOrigin();
  };

  auto slideStackFrameProvider = [this](const TransformationState& state)
  { return m_txManager.getSlideStackFrame(state); };

  auto viewScrollBarsAndSliderParamsProvider =
    [this, crosshairsOriginProvider, slideStackFrameProvider](const UID& viewUid
    ) -> std::tuple<gui::ViewSliderParams, gui::ViewSliderParams, gui::ViewSliderParams>
  {
    if (!m_interactionPackProvider)
    {
      const auto p = data::defaultViewSliderParams();
      return std::make_tuple(p, p, p);
    }

    if (auto pack = m_interactionPackProvider(viewUid))
    {
      auto camera = pack->getCamera();
      if (!camera)
      {
        const auto p = data::defaultViewSliderParams();
        return std::make_tuple(p, p, p);
      }

      const auto worldCrosshairsOrigin = crosshairsOriginProvider(STAGED);
      const auto world_O_stackFrame = slideStackFrameProvider(STAGED).world_O_frame();

      const auto scrollbarParams = data::viewScrollBarParams(
        m_dataManager, worldCrosshairsOrigin, world_O_stackFrame, *camera
      );

      const auto sliderParams = data::viewSliceSliderParams(
        m_dataManager, worldCrosshairsOrigin, world_O_stackFrame, *camera
      );

      return std::make_tuple(scrollbarParams.first, scrollbarParams.second, sliderParams);
    }

    const auto p = data::defaultViewSliderParams();
    return std::make_tuple(p, p, p);
  };

  auto crosshairsFrameProvider = [this](const TransformationState& state)
  { return m_txManager.getCrosshairsFrame(state); };

  // Function updating the crosshairs position from the new value of a view's slice slider
  auto updateCrosshairsPositionFromSliceSlider =
    [this,
     crosshairsFrameProvider,
     slideStackFrameProvider](const UID& viewUid, double newSliderValue)
  {
    if (!m_interactionPackProvider)
    {
      return;
    }

    if (auto pack = m_interactionPackProvider(viewUid))
    {
      auto camera = pack->getCamera();
      if (!camera)
      {
        return;
      }

      auto crosshairsFrame = crosshairsFrameProvider(COMMITTED);

      const glm::vec3 currentWorldPos = crosshairsFrame.worldOrigin();
      const glm::vec2 currentNdcPos{ndc_O_world(*camera, currentWorldPos)};
      const glm::vec3 worldFrontDir = worldRayDirection(*camera, currentNdcPos);

      const auto world_O_stackFrame = slideStackFrameProvider(COMMITTED).world_O_frame();

      const auto sliderParams
        = data::viewSliceSliderParams(m_dataManager, currentWorldPos, world_O_stackFrame, *camera);

      const float sliderDelta = static_cast<float>(newSliderValue - sliderParams.m_value);

      // Set the crosshairs position and request an update of the view.
      crosshairsFrame.setWorldOrigin(currentWorldPos + sliderDelta * worldFrontDir);
      this->handleCrosshairsChangeDone(crosshairsFrame);
    }
  };

  // Functional that translates a view camera in x and y and that updates the view rendering
  auto updateCameraPositionFromScrollBars = [this](const UID& viewUid, double x, double y)
  {
    if (!m_interactionPackProvider)
    {
      return;
    }

    if (auto pack = m_interactionPackProvider(viewUid))
    {
      if (auto camera = pack->getCamera())
      {
        const glm::vec3 delta{x, y, 0};
        translateAboutCamera(*camera, delta);
        m_guiManager.updateViewWidget(viewUid);
      }
    }
  };

  auto crosshairsQuerier = [this](const gui::ViewType& viewType)
  {
    auto crosshairsType = m_interactionManager.getCrosshairsType(viewType);

    switch (crosshairsType)
    {
    case CrosshairsType::RefImage:
    {
      return m_txManager.getCrosshairsFrame(TransformationState::Staged);
    }
    case CrosshairsType::SlideStack:
    {
      return m_txManager.getSlideStackCrosshairsFrame(TransformationState::Staged);
    }
    }
  };

  auto handleLayoutTabChanged = [this](int tabIndex)
  {
    // Center crosshairs on active slide, if they are not currently in the active slide
    const auto& layoutData = m_layoutManager.getLayoutTabData(tabIndex);

    if (layoutData.m_centersCrosshairs)
    {
      if (auto activeSlideUid = m_dataManager.activeSlideUid())
      {
        m_actionManager.centerCrosshairsOnSlide(*activeSlideUid);
      }
    }

    // Update views when layout tab index changes:
    m_guiManager.updateAllViewWidgets();
  };

  auto cameraQuerier = [this](const UID& viewUid)
  { return m_interactionManager.getCamera(viewUid); };

  auto interactionHandlerQuerier = [this](const UID& viewUid)
  { return m_interactionManager.getActiveInteractionHandler(viewUid); };

  m_guiManager.setViewScrollBarsAndSliderParamsProvider(viewScrollBarsAndSliderParamsProvider);
  m_guiManager.setViewScrollBarValuesBroadcaster(updateCameraPositionFromScrollBars);
  m_guiManager.setViewSliceSliderValueBroadcaster(updateCrosshairsPositionFromSliceSlider);

  m_guiManager.setViewLayoutTabChangedBroadcaster(handleLayoutTabChanged);

  m_guiManager.setCameraQuerier(cameraQuerier);
  m_guiManager.setCrosshairsQuerier(crosshairsQuerier);
  m_guiManager.setInteractionHandlerQuerier(interactionHandlerQuerier);

  if (!m_viewTypeRangeProvider)
  {
    std::cerr << "Null view type range provider: "
              << "Unable to iterate over views." << std::endl;
    return;
  }

  auto refSpaceAABBoxCenterProvider =
    [this, slideStackFrameProvider](const TransformationState& state)
  {
    return math::computeAABBoxCenter(
      data::refSpaceAABBox(m_dataManager, slideStackFrameProvider(state).world_O_frame())
    );
  };

  auto refSpaceAABBoxSizeProvider = [this, slideStackFrameProvider](const TransformationState& state)
  {
    return math::computeAABBoxSize(
      data::refSpaceAABBox(m_dataManager, slideStackFrameProvider(state).world_O_frame())
    );
  };

  auto refSpaceVoxelScaleProvider = [this](void)
  { return data::refSpaceVoxelScale(m_dataManager); };

  auto activeImageSubjectToWorldFrameProvider = [this](void)
  { return data::getActiveImageSubjectToWorldFrame(m_dataManager); };

  auto activeImageSubjectToWorldFrameBroadcaster = [this](const CoordinateFrame& world_O_subject)
  {
    data::setActiveImageSubjectToWorldFrame(m_dataManager, world_O_subject);

    if (auto activeImageUid = m_dataManager.activeImageUid())
    {
      m_signalImageTransformationChanged(*activeImageUid);
    }

    m_actionManager.updateWorldPositionStatus();
    m_guiManager.updateAllViewWidgets(); // Update required
  };

  auto scrollDistanceProvider = [this](const glm::vec3& worldCameraFront)
  { return data::refSpaceSliceScrollDistance(m_dataManager, worldCameraFront); };

  auto getPointPickingMode = [](const SceneType& sceneType) -> CrosshairsPointPickingMode
  {
    // For 2D scenes, we can analytically compute the point of intersection with the view plane.
    // For 3D scenes, use the depth buffer for computing the point of intersection with objects.

    switch (sceneType)
    {
    case SceneType::ReferenceImage2d:
    case SceneType::SlideStack2d:
    case SceneType::Registration_Image2d:
    case SceneType::Registration_Slide2d:
    case SceneType::None:
    {
      return CrosshairsPointPickingMode::PlanarPicking;
    }
    case SceneType::ReferenceImage3d:
    case SceneType::SlideStack3d:
    {
      return CrosshairsPointPickingMode::DepthPicking;
    }
    }
  };

  // Functional returning the point picked in a 2D scene. This function analytically computes
  // the intersection of the view's camera plane with the ray emanating from the point picked.
  auto pointPicker2d =
    [crosshairsFrameProvider](const camera::Camera& camera, const glm::vec2& ndcPos)
  {
    static constexpr float sk_nearPlaneZ = -1.0f;

    const auto crosshairs = crosshairsFrameProvider(STAGED);
    const auto worldIntersection
      = worldCameraPlaneIntersection(camera, ndcPos, crosshairs.worldOrigin());

    if (worldIntersection)
    {
      return ndcZofWorldPoint(camera, *worldIntersection);
    }

    return sk_nearPlaneZ; // No intersection
  };

  // Functional returning the point picked in a 3D scene.
  // This function uses the depth buffer-based point picker of IRenderer.
  auto pointPicker3d = [this](const UID& viewUid, const glm::vec2& ndcPos)
  {
    // Object ID of 0 indicates no intersection
    static constexpr std::pair<uint16_t, float> sk_nearPlane(0, -1.0f);

    if (auto widget = m_viewWidgetProvider(viewUid))
    {
      if (auto renderer = widget->getRenderer())
      {
        return renderer->pickObjectIdAndNdcDepth(ndcPos);
      }
    }

    return sk_nearPlane; // No intersection
  };

  // Functional that returns const pointer to the active image CPU record.
  // Returns nullptr if the active record cannot be queried.
  auto getActiveImageCpuRecord = [this]() -> const imageio::ImageCpuRecord*
  {
    if (auto imageRecord = m_dataManager.activeImageRecord().lock())
    {
      return imageRecord->cpuData();
    }
    return nullptr;
  };

  // Functional that sets active image window/level and that evokes a signal that the
  // image window/level has changed.
  auto activeImageWindowLevelChangedBroadcaster = [this](double window, double level)
  {
    auto imageRecord = m_dataManager.activeImageRecord().lock();
    if (!imageRecord)
    {
      return;
    }

    if (auto r = imageRecord->cpuData())
    {
      r->setWindowWidth(0, window);
      r->setLevel(0, level);
      m_signalImageWindowLevelChanged(imageRecord->uid());

      m_guiManager.updateAllViewWidgets();
    }
  };

  // Functional that returns the active slide record as a weak pointer
  auto getActiveSlideRecord = [this]() { return m_dataManager.activeSlideRecord(); };

  // Function that handles object picking
  auto objectPickingHandler = [](uint16_t /*objectId*/)
  {
    //        std::cout << "ID = " << objectId << std::endl;
  };

  // Functional for broadcasting updated slide transformations.
  // The argument is a map from slide UID to updated slide transformation.
  /// @todo Pull this logic out into different place
  auto slideTxsBroadcaster =
    [this, crosshairsFrameProvider, slideStackFrameProvider](
      const std::map<UID, slideio::SlideTransformation>& slideTxs, const gui::ViewType& viewType
    )
  {
    /// @todo Make this a user option
    const bool sk_fixedCrosshairs
      = (gui::ViewType::Stack_ActiveSlide == viewType || gui::ViewType::Stack_StackSide1 == viewType || gui::ViewType::Stack_StackSide2 == viewType || gui::ViewType::Reg_ActiveSlide == viewType);

    std::list<UID> slideUids;
    boost::copy(slideTxs | boost::adaptors::map_keys, std::back_inserter(slideUids));

    auto activeSlideRecord = m_dataManager.activeSlideRecord().lock();
    if (!activeSlideRecord || !activeSlideRecord->cpuData())
    {
      return;
    }

    for (const auto& tx : slideTxs)
    {
      if (sk_fixedCrosshairs && (tx.first == activeSlideRecord->uid()))
      {
        // Fix the crosshairs at position relative to active slide being transformed:

        const glm::mat4 OLD_stack_O_slide = slideio::stack_O_slide(*activeSlideRecord->cpuData());
        {
          activeSlideRecord->cpuData()->setTransformation(tx.second);
        }
        const glm::mat4 NEW_stack_O_slide = slideio::stack_O_slide(*activeSlideRecord->cpuData());

        // Get existing crosshairs
        auto crosshairsFrame = crosshairsFrameProvider(COMMITTED);

        const glm::mat4 stack_O_world = slideStackFrameProvider(COMMITTED).frame_O_world();
        const glm::mat4 world_O_stack = glm::inverse(stack_O_world);

        // Compute crosshairs origin in Slide space prior to transformation
        const glm::vec4 worldOrigin{crosshairsFrame.worldOrigin(), 1.0f};
        const glm::mat4 activeSlide_O_slideStack = glm::inverse(OLD_stack_O_slide);
        const glm::vec4 OLD_slideOrigin = activeSlide_O_slideStack * stack_O_world * worldOrigin;

        // Compute crosshairs origin in Slide space after transformation
        const glm::vec4 NEW_worldOrigin = world_O_stack * NEW_stack_O_slide * OLD_slideOrigin;

        crosshairsFrame.setWorldOrigin(glm::vec3{NEW_worldOrigin} / NEW_worldOrigin.w);
        this->handleCrosshairsChangeDone(crosshairsFrame);
      }
      else
      {
        activeSlideRecord->cpuData()->setTransformation(tx.second);
      }
    }

    m_assemblyManager.updatedSlideTransformations(slideUids);
  };

  auto adjustCameraNearDistance =
    [this, getRefSpaceAABBox, refSpaceVoxelScaleProvider](const UID& viewUid)
  {
    auto* camera = m_interactionManager.getCamera(viewUid);
    if (!camera)
    {
      return;
    }

    const float voxelScale = refSpaceVoxelScaleProvider();

    /// @todo This should be done every render, not only when the camera moves
    if (math::isInside(getRefSpaceAABBox(), camera::worldOrigin(*camera)))
    {
      // Set a closer near plane if the camera is "inside" the scene in order to avoid
      // clipping on objects in the scene
      camera->setNearDistance(1.0f * voxelScale);
    }
    else
    {
      // Set a more distant near plane if the camera is "outside" the scene in order to
      // gain depth buffer precision
      camera->setNearDistance(20.0f * voxelScale);
    }
  };

  for (auto& view : m_viewTypeRangeProvider())
  {
    const auto& viewUid = view.first;
    const auto& viewType = view.second;
    const auto sceneType = m_sceneTypeProvider(viewType);

    auto pack = m_interactionPackProvider(viewUid);
    if (!pack)
    {
      continue;
    }

    auto camera = pack->getCamera();
    if (!camera)
    {
      continue;
    }

    if (auto handler = pack->getCameraHandler())
    {
      auto cameraProvider = [this, viewUid]() { return m_interactionManager.getCamera(viewUid); };

      auto cameraMovedBroadcaster =
        [adjustCameraNearDistance, viewUid](const glm::vec3& /*worldCameraOrigin*/)
      { adjustCameraNearDistance(viewUid); };

      handler->setCameraProvider(cameraProvider);

      handler->setCrosshairsOriginProvider([crosshairsOriginProvider]()
                                           { return crosshairsOriginProvider(COMMITTED); });

      handler->setRefSpaceAABBoxCenterProvider([refSpaceAABBoxCenterProvider]()
                                               { return refSpaceAABBoxCenterProvider(COMMITTED); });

      handler->setRefSpaceAABBoxSizeProvider([refSpaceAABBoxSizeProvider]()
                                             { return refSpaceAABBoxSizeProvider(COMMITTED); });

      handler->setRefSpaceVoxelScaleProvider(refSpaceVoxelScaleProvider);

      handler->setWorldCameraPositionBroadcaster(cameraMovedBroadcaster);
    }

    if (auto handler = pack->getCrosshairsHandler())
    {
      handler->setPointPickingMode(getPointPickingMode(sceneType));

      handler->setPlanarPointPicker([pointPicker2d, camera](const glm::vec2& ndcPos)
                                    { return pointPicker2d(*camera, ndcPos); });

      handler->setDepthPointPicker([pointPicker3d, viewUid](const glm::vec2& ndcPos)
                                   { return pointPicker3d(viewUid, ndcPos); });

      handler->setScrollDistanceProvider(scrollDistanceProvider);

      handler->setCrosshairsFrameProvider([crosshairsFrameProvider]()
                                          { return crosshairsFrameProvider(STAGED); });

      handler->setCrosshairsFrameChangedBroadcaster([this](const CoordinateFrame& frame)
                                                    { this->handleCrosshairsChanged(frame); });

      handler->setCrosshairsFrameChangeDoneBroadcaster([this](const CoordinateFrame& frame)
                                                       { this->handleCrosshairsChangeDone(frame); }
      );

      handler->setObjectIdBroadcaster(objectPickingHandler);

      // Disable crosshairs rotation for views that show Slide Stack crosshairs
      switch (m_interactionManager.getCrosshairsType(viewType))
      {
      case CrosshairsType::RefImage:
      {
        handler->setRotationModeEnabled(true);
        break;
      }
      case CrosshairsType::SlideStack:
      {
        handler->setRotationModeEnabled(false);
        break;
      }
      }
    }

    if (auto handler = pack->getWindowLevelHandler())
    {
      handler->setActiveImageCpuRecordRequester(getActiveImageCpuRecord);
      handler->setActiveImageWindowLevelBroadcaster(activeImageWindowLevelChangedBroadcaster);
    }

    if (auto handler = pack->getRefImageHandler())
    {
      handler->setCrosshairsOriginProvider([crosshairsOriginProvider]()
                                           { return crosshairsOriginProvider(COMMITTED); });

      handler->setImageFrameProvider(activeImageSubjectToWorldFrameProvider);
      handler->setImageFrameChangedBroadcaster(activeImageSubjectToWorldFrameBroadcaster);
      handler->setImageFrameChangeDoneBroadcaster(activeImageSubjectToWorldFrameBroadcaster);
      handler->setImageVoxelScaleProvider(refSpaceVoxelScaleProvider);
    }

    if (auto handler = pack->getStackHandler())
    {
      handler->setSlideStackFrameProvider([slideStackFrameProvider]()
                                          { return slideStackFrameProvider(STAGED); });

      handler->setSlideStackFrameChangedBroadcaster([this](const CoordinateFrame& frame)
                                                    { this->handleStackFrameChanged(frame); });

      handler->setSlideStackFrameChangeDoneBroadcaster([this](const CoordinateFrame& frame)
                                                       { this->handleStackFrameChangeDone(frame); }
      );

      handler->setRefImageVoxelScaleProvider(refSpaceVoxelScaleProvider);
    }

    if (auto handler = pack->getSlideHandler())
    {
      handler->setSlideStackFrameProvider([slideStackFrameProvider]()
                                          { return slideStackFrameProvider(STAGED); });

      handler->setActiveSlideRecordProvider(getActiveSlideRecord);
      handler->setSlideTxsChangedBroadcaster(std::bind(slideTxsBroadcaster, _1, viewType));
    }
  }
}

void ConnectionManager::Impl::createRendererUpdateConnections()
{
  using std::placeholders::_1;
  using std::placeholders::_2;

  if (!m_viewTypeRangeProvider)
  {
    std::cerr << "Null view type range provider: "
              << "Unable to iterate over views." << std::endl;
    return;
  }

  if (!m_viewWidgetProvider)
  {
    std::cerr << "Null view widget provider: "
              << "Unable to iterate over views." << std::endl;
    return;
  }

  if (!m_viewsOfTypeProvider)
  {
    return;
  }

  // Map from each view to the set of views to which it synchronizes zoom
  // -key: view UID
  // -value: set of views to which the view synchronizes zoom
  std::unordered_map<UID, std::unordered_set<UID> > zoomSynchMap;

  for (const auto& view : m_viewTypeRangeProvider())
  {
    const auto& viewUid = view.first;
    const auto& viewType = view.second;

    // Create set of views to which the view synchronizes zoom
    std::unordered_set<UID> synchedViews;

    for (const std::set<gui::ViewType>& typeSet : sk_viewTypesThatSynchZoom)
    {
      if (typeSet.count(viewType) > 0)
      {
        for (const auto& synchedViewType : typeSet)
        {
          // UIDs of synched view types.
          // Remove the view itself, since it need not synch to itself
          std::list<UID> synchedViewUids = m_viewsOfTypeProvider(synchedViewType);
          synchedViewUids.remove(viewUid);

          std::copy(
            std::begin(synchedViewUids),
            std::end(synchedViewUids),
            std::inserter(synchedViews, std::end(synchedViews))
          );
        }
      }
    }

    zoomSynchMap[viewUid] = synchedViews;
  }

  /// @todo Put this long callback into ActionManager

  auto zoomSynchronizer = [this, zoomSynchMap](
                            const UID& signalingViewUid,
                            float absoluteZoomValue,
                            const std::optional<glm::vec3>& worldCenterPos
                          )
  {
    auto it = zoomSynchMap.find(signalingViewUid);

    if (std::end(zoomSynchMap) == it)
    {
      return;
    }

    // Set of views to which signalingViewUid synchs:
    const std::unordered_set<UID> synchedViewUids = it->second;

    for (const auto& synchedViewUid : synchedViewUids)
    {
      if (synchedViewUid == signalingViewUid)
      {
        // Do not synchronize zoom for the view itself
        continue;
      }

      if (!m_interactionPackProvider)
      {
        continue;
      }

      auto pack = m_interactionPackProvider(synchedViewUid);
      if (!pack)
      {
        continue;
      }

      auto camera = pack->getCamera();
      if (!camera)
      {
        continue;
      }

      if (camera->isOrthographic() && worldCenterPos)
      {
        // Compute and zoom to the relative zoom factor
        const float relativeZoomFactor = absoluteZoomValue / camera->getZoom();
        const glm::vec2 ndcCenterPos{camera::ndc_O_world(*camera, *worldCenterPos)};
        camera::zoomNdc(*camera, relativeZoomFactor, ndcCenterPos);
      }
      else
      {
        // If this is a perspective camera or there is no worldCenterPos to
        // zoom to, then just set the zoom value:
        camera->setZoom(absoluteZoomValue);
      }

      m_guiManager.updateViewWidget(synchedViewUid);
    }
  };

  for (const auto& view : m_viewTypeRangeProvider())
  {
    const auto& viewUid = view.first;

    auto widget = m_viewWidgetProvider(viewUid);
    if (!widget)
    {
      continue;
    }

    auto pack = m_interactionPackProvider(viewUid);
    if (!pack)
    {
      continue;
    }

    auto myViewUpdater = [this, &viewUid]() { m_guiManager.updateViewWidget(viewUid); };
    auto allViewsUpdater = std::bind(&GuiManager::updateAllViewWidgets, &m_guiManager);
    auto myZoomSynchronizer = std::bind(zoomSynchronizer, viewUid, _1, _2);

    if (auto handler = pack->getCameraHandler())
    {
      handler->setAllViewsUpdater(nullptr);
      handler->setMyViewUpdater(myViewUpdater);
      handler->setZoomSynchronizer(myZoomSynchronizer);
    }

    if (auto handler = pack->getCrosshairsHandler())
    {
      handler->setAllViewsUpdater(allViewsUpdater);
      handler->setMyViewUpdater(nullptr);
    }

    if (auto handler = pack->getRefImageHandler())
    {
      handler->setAllViewsUpdater(allViewsUpdater);
      handler->setMyViewUpdater(nullptr);
    }

    if (auto handler = pack->getStackHandler())
    {
      handler->setAllViewsUpdater(allViewsUpdater);
      handler->setMyViewUpdater(nullptr);
    }

    if (auto handler = pack->getSlideHandler())
    {
      handler->setAllViewsUpdater(allViewsUpdater);
      handler->setMyViewUpdater(nullptr);
    }

    if (auto handler = pack->getWindowLevelHandler())
    {
      handler->setAllViewsUpdater(allViewsUpdater);
      handler->setMyViewUpdater(nullptr);
    }

    m_assemblyManager.setAllViewsUpdater(allViewsUpdater);
  }
}

void ConnectionManager::Impl::handleCrosshairsChanged(const CoordinateFrame& crosshairs)
{
  m_txManager.stageCrosshairsFrame(crosshairs);
  m_actionManager.updateWorldPositionStatus();

  // Need to update views, since change is not handled by an InteractionHandler
  m_guiManager.updateAllViewWidgets();
}

void ConnectionManager::Impl::handleCrosshairsChangeDone(const CoordinateFrame& crosshairs)
{
  // Get the last committed frame:
  CoordinateFrame oldFrame = m_txManager.getCrosshairsFrame(TransformationState::Committed);

  // Stage and commit the new frame:
  m_txManager.stageCrosshairsFrame(crosshairs);
  m_txManager.commitCrosshairsFrame();
  m_actionManager.updateWorldPositionStatus();

  // Transformation that will rotate the view cameras about the crosshairs origin by the delta
  // between the old and new crosshairs rotations:
  const glm::mat4 extra = glm::translate(crosshairs.worldOrigin())
                          * glm::toMat4(
                            oldFrame.world_O_frame_rotation()
                            * glm::inverse(crosshairs.world_O_frame_rotation())
                          )
                          * glm::translate(-crosshairs.worldOrigin());

  m_interactionManager.applyExtraToCameras(LinkedFrameType::Crosshairs, extra);

  // Need to update views, since change is not handled by an InteractionHandler
  m_guiManager.updateAllViewWidgets();
}

void ConnectionManager::Impl::handleStackFrameChanged(const CoordinateFrame& stackFrame)
{
  m_txManager.stageSlideStackFrame(stackFrame);
  m_signalSlideStackFrameChanged(stackFrame); // Signal necessary to update UI
  m_guiManager.updateAllViewWidgets();
};

void ConnectionManager::Impl::handleStackFrameChangeDone(const CoordinateFrame& stackFrame)
{
  m_txManager.stageSlideStackFrame(stackFrame);
  m_txManager.commitSlideStackFrame();
  m_signalSlideStackFrameChanged(stackFrame); // Signal necessary to update UI
  m_guiManager.updateAllViewWidgets();

  // This can be used to align cameras to slide stack frame.
  // It happens automatically for Slide Stack views.
  // m_interactionManager.alignCamerasToFrames();
};
