#include "logic/managers/GuiManager.h"

#include "common/CoordinateFrame.h"
#include "common/HZeeException.hpp"

#include "gui/ActionsContainer.h"
#include "gui/MainWindow.h"
#include "gui/docks/RefFrameEditorDock.h"
#include "gui/docks/SlideStackEditorDock.h"
#include "gui/toolbars/ToolBarCreation.h"
#include "gui/view/GLWidget.h"
#include "gui/view/ViewWidget.h"

#include "logic/interaction/CrosshairsInteractionHandler.h"
#include "logic/interaction/InteractionPack.h"

#include "rendering/interfaces/IRenderer.h"
#include "rendering/renderers/DepthPeelRenderer.h"

#include <sstream>

namespace
{

/**
 * @brief Create the Dual-Depth Peel renderer for a given view
 * @param viewUid View UID
 * @param shaderActivator Function that activates shader programs
 * @param uniformsProvider Function returning the uniforms
 * @return Unique pointer to the renderer
 */
std::unique_ptr<DepthPeelRenderer> createDdpRenderer(
  const UID& viewUid,
  ShaderProgramActivatorType shaderActivator,
  UniformsProviderType uniformsProvider,
  GetterType<IDrawable*> rootProvider,
  GetterType<IDrawable*> overlayProvider
)
{
  std::ostringstream name;
  name << "DdpRenderer_" << viewUid << std::ends;

  auto renderer = std::make_unique<DepthPeelRenderer>(
    name.str(), shaderActivator, uniformsProvider, rootProvider, overlayProvider
  );

  // Maximum number of dual depth peeling iterations. Three iterations enables
  // 100% pixel perfect rendering of six transparent layers.
  static constexpr uint sk_maxPeels = 3;
  renderer->setMaxNumberOfPeels(sk_maxPeels);

  // Override the maximum depth peel limit by using occlusion queries.
  // Using an occlusion ratio of 0.0 means that as many peels are
  // performed as necessary in order to render the scene transparency correctly.
  renderer->setOcclusionRatio(0.0f);

  return renderer;
}

bool isScene2d(const SceneType& sceneType)
{
  switch (sceneType)
  {
  case SceneType::ReferenceImage2d:
  case SceneType::SlideStack2d:
  case SceneType::Registration_Image2d:
  case SceneType::Registration_Slide2d:
  {
    return true;
  }
  case SceneType::ReferenceImage3d:
  case SceneType::SlideStack3d:
  case SceneType::None:
  {
    return false;
  }
  }
}

} // namespace

GuiManager::GuiManager(
  GetterType<view_type_range_t> viewUidAndTypeRangeProvider,
  ViewWidgetToLayoutSetterType viewWidgetSetter,
  InteractionPackProviderType packProvider,
  RootDrawableProviderType rootDrawableProvider,
  RootDrawableProviderType overlayDrawableProvider,
  SceneTypeProviderType sceneTypeProvider,
  ShaderProgramActivatorType shaderActivator,
  UniformsProviderType uniformsProvider
)
  : m_viewUidAndTypeProvider(viewUidAndTypeRangeProvider)
  , m_viewWidgetSetter(viewWidgetSetter)
  , m_interactionPackProvider(packProvider)
  , m_rootDrawableProvider(rootDrawableProvider)
  , m_overlayDrawableProvider(overlayDrawableProvider)
  , m_sceneTypeProvider(sceneTypeProvider)
  ,

  m_viewScrollBarsAndSliderParamsProvider(nullptr)
  ,

  m_viewScrollBarValuesBroadcaster(nullptr)
  , m_viewSliceSliderValueBroadcaster(nullptr)
  ,

  m_viewLayoutTabChangedBroadcaster(nullptr)
  ,

  m_shaderActivator(shaderActivator)
  , m_uniformsProvider(uniformsProvider)
  ,

  m_interactionModeSetter(nullptr)
  , m_crosshairsToActiveSlideAligner(nullptr)
  , m_crosshairsToSlideStackAligner(nullptr)
  , m_crosshairsToAnatomicalPlanesAligner(nullptr)
  , m_allViewsResetter(nullptr)
  , m_projectSaver(nullptr)
  , m_slideStackViews3dModeSetter(nullptr)
  ,

  m_cameraQuerier(nullptr)
  , m_crosshairsQuerier(nullptr)
  , m_interactionHandlerQuerier(nullptr)
  ,

  m_actionsContainer(nullptr)
  ,

  m_mainWindow(std::make_unique<gui::MainWindow>(nullptr))
  , m_refImageEditorDock(new gui::RefFrameEditorDock(m_mainWindow.get()))
  , m_slideStackEditorDock(new gui::SlideStackEditorDock(m_mainWindow.get()))
  , m_viewWidgets()
{
  if (!m_viewUidAndTypeProvider || !m_interactionPackProvider || !m_viewWidgetSetter)
  {
    throw_debug("Null providers")
  }

  if (!m_mainWindow)
  {
    throw_debug("MainWindow could not be created")
  }
}

GuiManager::~GuiManager() = default;

void GuiManager::initializeGL()
{
  if (!m_viewUidAndTypeProvider || !m_viewWidgetSetter || !m_interactionPackProvider || !m_rootDrawableProvider || !m_overlayDrawableProvider)
  {
    return;
  }

  createViewWidgets();

  auto tabChangedHandler = [this](int tabIndex)
  {
    if (m_viewLayoutTabChangedBroadcaster)
    {
      m_viewLayoutTabChangedBroadcaster(tabIndex);
    }
  };

  if (m_mainWindow)
  {
    m_mainWindow->setViewLayoutTabChangedPublisher(tabChangedHandler);
  }
  else
  {
    throw_debug("MainWindow is null")
  }
}

void GuiManager::setupMainWindow()
{
  using std::placeholders::_1;

  m_actionsContainer = std::make_unique<gui::ActionsContainer>(
    m_interactionModeSetter,
    m_crosshairsToActiveSlideAligner,
    m_crosshairsToSlideStackAligner,
    m_crosshairsToAnatomicalPlanesAligner,
    m_slideStackViews3dModeSetter,
    m_allViewsResetter,
    m_projectSaver,
    m_refImageEditorDock->toggleViewAction(),
    m_slideStackEditorDock->toggleViewAction(),
    m_mainWindow.get()
  );

  if (!m_actionsContainer)
  {
    throw_debug("Actions container is null")
  }

  QToolBar* pointerToolBar = gui::createPointerToolBar(*m_actionsContainer);

  if (!pointerToolBar)
  {
    throw_debug("Pointer toolbar is null")
  }

  // Add docks and toolbars to main window:
  if (m_mainWindow)
  {
    m_mainWindow->addDockWidget(Qt::RightDockWidgetArea, m_refImageEditorDock);
    m_mainWindow->addDockWidget(Qt::RightDockWidgetArea, m_slideStackEditorDock);

    m_mainWindow->addToolBar(Qt::ToolBarArea::TopToolBarArea, pointerToolBar);

    m_refImageEditorDock->setVisible(true);
    m_slideStackEditorDock->setVisible(false);
  }
  else
  {
    throw_debug("Main window is null")
  }
}

void GuiManager::showMainWindow()
{
  if (m_mainWindow)
  {
    m_mainWindow->showMaximized();
    //        m_mainWindow->showNormal();
  }
  else
  {
    throw_debug("MainWindow is null")
  }
}

gui::ViewWidget* GuiManager::getViewWidget(const UID& viewUid)
{
  auto it = m_viewWidgets.find(viewUid);
  if (std::end(m_viewWidgets) != it)
  {
    return it->second;
  }
  return nullptr;
}

void GuiManager::setInteractionModeSetter(SetterType<InteractionModeType> setter)
{
  m_interactionModeSetter = setter;
}

void GuiManager::setCrosshairsToActiveSlideAligner(CrosshairsAlignerType aligner)
{
  m_crosshairsToActiveSlideAligner = aligner;
}

void GuiManager::setCrosshairsToSlideStackFrameAligner(CrosshairsAlignerType aligner)
{
  m_crosshairsToSlideStackAligner = aligner;
}

void GuiManager::setCrosshairsToAnatomicalPlanesAligner(CrosshairsAlignerType aligner)
{
  m_crosshairsToAnatomicalPlanesAligner = aligner;
}

void GuiManager::setSlideStackView3dModeSetter(SetterType<bool> setter)
{
  m_slideStackViews3dModeSetter = setter;
}

void GuiManager::setCameraQuerier(QuerierType<camera::Camera*, UID> querier)
{
  m_cameraQuerier = querier;
}

void GuiManager::setCrosshairsQuerier(QuerierType<CoordinateFrame, gui::ViewType> querier)
{
  m_crosshairsQuerier = querier;
}

void GuiManager::setInteractionHandlerQuerier(QuerierType<IInteractionHandler*, UID> querier)
{
  m_interactionHandlerQuerier = querier;
}

void GuiManager::setViewScrollBarsAndSliderParamsProvider(
  ScrollBarsAndSliderParamsProviderType provider
)
{
  m_viewScrollBarsAndSliderParamsProvider = provider;

  // Set the new provider in all of the view widgets
  for (auto& v : m_viewWidgets)
  {
    if (auto widget = v.second)
    {
      widget->setScrollBarsAndSliderParamsProvider(m_viewScrollBarsAndSliderParamsProvider);
    }
  }
}

void GuiManager::setViewScrollBarValuesBroadcaster(ViewScrollBarValuesBroadcasterType broadcaster)
{
  m_viewScrollBarValuesBroadcaster = broadcaster;

  // Set the new notifier in all of the view widgets
  for (auto& v : m_viewWidgets)
  {
    if (auto widget = v.second)
    {
      widget->setScrollBarValuesBroadcaster(m_viewScrollBarValuesBroadcaster);
    }
  }
}

void GuiManager::setViewSliceSliderValueBroadcaster(ViewSliceSliderValueBroadcasterType broadcaster)
{
  m_viewSliceSliderValueBroadcaster = broadcaster;

  // Set the new notifier in all of the view widgets
  for (auto& v : m_viewWidgets)
  {
    if (auto widget = v.second)
    {
      widget->setSliceSliderValueBroadcaster(m_viewSliceSliderValueBroadcaster);
    }
  }
}

void GuiManager::setViewLayoutTabChangedBroadcaster(SetterType<int> broadcaster)
{
  m_viewLayoutTabChangedBroadcaster = broadcaster;
}

void GuiManager::setAllViewsResetter(AllViewsResetterType resetter)
{
  m_allViewsResetter = resetter;
}

void GuiManager::setProjectSaver(ProjectSaverType saver)
{
  m_projectSaver = saver;
}

void GuiManager::setImageLoader(ImageLoaderType loader)
{
  if (m_mainWindow)
  {
    m_mainWindow->setImageLoader(loader);
  }
  else
  {
    throw_debug("MainWindow is null")
  }
}

void GuiManager::setParcellationLoader(ImageLoaderType loader)
{
  if (m_mainWindow)
  {
    m_mainWindow->setParcellationLoader(loader);
  }
  else
  {
    throw_debug("MainWindow is null")
  }
}

void GuiManager::setSlideLoader(SlideLoaderType loader)
{
  if (m_mainWindow)
  {
    m_mainWindow->setSlideLoader(loader);
  }
  else
  {
    throw_debug("MainWindow is null")
  }
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void GuiManager::setImageSelectionsPublisher(gui::ImageSelections_msgFromUi_PublisherType publisher)
{
  if (m_refImageEditorDock)
  {
    m_refImageEditorDock->setImageSelectionsPublisher(publisher);
  }
}

void GuiManager::setImageSelectionsResponder(gui::ImageSelections_msgToUi_ResponderType responder)
{
  if (m_refImageEditorDock)
  {
    m_refImageEditorDock->setImageSelectionsResponder(responder);
  }
}

void GuiManager::sendImageSelectionsToUi(const gui::ImageSelections_msgToUi& msg)
{
  if (m_refImageEditorDock)
  {
    m_refImageEditorDock->setImageSelections(msg);
  }
}

void GuiManager::sendImageColorMapsToUi(const gui::ImageColorMaps_msgToUi& msg)
{
  if (m_refImageEditorDock)
  {
    m_refImageEditorDock->setImageColorMaps(msg);
  }
}

void GuiManager::setImageColorMapsResponder(gui::ImageColorMaps_msgToUi_ResponderType responder)
{
  if (m_refImageEditorDock)
  {
    m_refImageEditorDock->setImageColorMapsResponder(responder);
  }
}

void GuiManager::setImagePropertiesPartialPublisher(
  gui::ImagePropertiesPartial_msgFromUi_PublisherType publisher
)
{
  if (m_refImageEditorDock)
  {
    m_refImageEditorDock->setImagePropertiesPartialPublisher(publisher);
  }
}

void GuiManager::sendImagePropertiesPartialToUi(const gui::ImagePropertiesPartial_msgToUi& msg)
{
  if (m_refImageEditorDock)
  {
    m_refImageEditorDock->setImagePropertiesPartial(msg);
  }
}

void GuiManager::sendImagePropertiesCompleteToUi(const gui::ImagePropertiesComplete_msgToUi& msg)
{
  if (m_refImageEditorDock)
  {
    m_refImageEditorDock->setImagePropertiesComplete(msg);
  }
}

void GuiManager::setImagePropertiesCompleteResponder(
  gui::ImagePropertiesComplete_msgToUi_ResponderType responder
)
{
  if (m_refImageEditorDock)
  {
    m_refImageEditorDock->setImagePropertiesCompleteResponder(responder);
  }
}

void GuiManager::setImageHeaderResponder(gui::ImageHeader_msgToUi_ResponderType responder)
{
  if (m_refImageEditorDock)
  {
    m_refImageEditorDock->setImageHeaderResponder(responder);
  }
}

void GuiManager::setImageTransformationPublisher(
  gui::ImageTransformation_msgFromUi_PublisherType publisher
)
{
  if (m_refImageEditorDock)
  {
    m_refImageEditorDock->setImageTransformationPublisher(publisher);
  }
}

void GuiManager::setImageTransformationResponder(
  gui::ImageTransformation_msgToUi_ResponderType responder
)
{
  if (m_refImageEditorDock)
  {
    m_refImageEditorDock->setImageTransformationResponder(responder);
  }
}

void GuiManager::sendImageTransformationToUi(const gui::ImageTransformation_msgToUi& msg)
{
  if (m_refImageEditorDock)
  {
    m_refImageEditorDock->setImageTransformation(msg);
  }
}

void GuiManager::setParcellationSelectionsPublisher(
  gui::ParcellationSelection_msgFromUi_PublisherType publisher
)
{
  if (m_refImageEditorDock)
  {
    m_refImageEditorDock->setParcellationSelectionsPublisher(publisher);
  }
}

void GuiManager::setParcellationSelectionsResponder(
  gui::ParcellationSelections_msgToUi_ResponderType responder
)
{
  if (m_refImageEditorDock)
  {
    m_refImageEditorDock->setParcellationSelectionsResponder(responder);
  }
}

void GuiManager::sendParcellationSelectionsToUi(const gui::ParcellationSelections_msgToUi& msg)
{
  if (m_refImageEditorDock)
  {
    m_refImageEditorDock->setParcellationSelections(msg);
  }
}

void GuiManager::setParcellationPropertiesPartialPublisher(
  gui::ParcellationPropertiesPartial_msgFromUi_PublisherType publisher
)
{
  if (m_refImageEditorDock)
  {
    m_refImageEditorDock->setParcellationPropertiesPartialPublisher(publisher);
  }
}

void GuiManager::sendParcellationPropertiesPartialToUi(
  const gui::ParcellationPropertiesPartial_msgToUi& msg
)
{
  if (m_refImageEditorDock)
  {
    m_refImageEditorDock->setParcellationPropertiesPartial(msg);
  }
}

void GuiManager::sendParcellationPropertiesCompleteToUi(
  const gui::ParcellationPropertiesComplete_msgToUi& msg
)
{
  if (m_refImageEditorDock)
  {
    m_refImageEditorDock->setParcellationPropertiesComplete(msg);
  }
}

void GuiManager::setParcellationHeaderResponder(
  gui::ParcellationHeader_msgToUi_ResponderType responder
)
{
  if (m_refImageEditorDock)
  {
    m_refImageEditorDock->setParcellationHeaderResponder(responder);
  }
}

void GuiManager::setParcellationPropertiesCompleteResponder(
  gui::ParcellationPropertiesComplete_msgToUi_ResponderType responder
)
{
  if (m_refImageEditorDock)
  {
    m_refImageEditorDock->setParcellationPropertiesCompleteResponder(responder);
  }
}

void GuiManager::setParcellationLabelsPartialPublisher(
  gui::ParcellationLabelsPartial_msgFromUi_PublisherType publisher
)
{
  if (m_refImageEditorDock)
  {
    m_refImageEditorDock->setParcellationLabelsPartialPublisher(publisher);
  }
}

void GuiManager::sendParcellationLabelsCompleteToUi(const gui::ParcellationLabelsComplete_msgToUi& msg
)
{
  if (m_refImageEditorDock)
  {
    m_refImageEditorDock->setParcellationLabelsComplete(msg);
  }
}

void GuiManager::setParcellationLabelsCompleteResponder(
  gui::ParcellationLabelsComplete_msgToUi_ResponderType responder
)
{
  if (m_refImageEditorDock)
  {
    m_refImageEditorDock->setParcellationLabelsCompleteResponder(responder);
  }
}

void GuiManager::setSlideStackPartialPublisher(
  gui::SlideStackPartial_msgFromUi_PublisherType publisher
)
{
  if (m_slideStackEditorDock)
  {
    m_slideStackEditorDock->setSlideStackPartialPublisher(publisher);
  }
}

void GuiManager::setSlideStackOrderPublisher(gui::SlideStackOrder_msgFromUi_PublisherType publisher)
{
  if (m_slideStackEditorDock)
  {
    m_slideStackEditorDock->setSlideStackOrderPublisher(publisher);
  }
}

void GuiManager::setActiveSlidePublisher(gui::ActiveSlide_msgFromUi_PublisherType publisher)
{
  if (m_slideStackEditorDock)
  {
    m_slideStackEditorDock->setActiveSlidePublisher(publisher);
  }
}

void GuiManager::setSlideCommonPropertiesPartialPublisher(
  gui::SlideCommonPropertiesPartial_msgFromUi_PublisherType publisher
)
{
  if (m_slideStackEditorDock)
  {
    m_slideStackEditorDock->setSlideCommonPropertiesPartialPublisher(publisher);
  }
}

void GuiManager::setSlideHeaderPartialPublisher(
  gui::SlideHeaderPartial_msgFromUi_PublisherType publisher
)
{
  if (m_slideStackEditorDock)
  {
    m_slideStackEditorDock->setSlideHeaderPartialPublisher(publisher);
  }
}

void GuiManager::setSlideViewDataPartialPublisher(
  gui::SlideViewDataPartial_msgFromUi_PublisherType publisher
)
{
  if (m_slideStackEditorDock)
  {
    m_slideStackEditorDock->setSlideViewDataPartialPublisher(publisher);
  }
}

void GuiManager::setSlideTxDataPartialPublisher(
  gui::SlideTxDataPartial_msgFromUi_PublisherType publisher
)
{
  if (m_slideStackEditorDock)
  {
    m_slideStackEditorDock->setSlideTxDataPartialPublisher(publisher);
  }
}

void GuiManager::setMoveToSlidePublisher(gui::MoveToSlide_msgFromUi_PublisherType publisher)
{
  if (m_slideStackEditorDock)
  {
    m_slideStackEditorDock->setMoveToSlidePublisher(publisher);
  }
}

void GuiManager::sendSlideStackPartialToUi(const gui::SlideStackPartial_msgToUi& msg)
{
  if (m_slideStackEditorDock)
  {
    m_slideStackEditorDock->setSlideStackPartial(msg);
  }
}

void GuiManager::sendSlideStackCompleteToUi(const gui::SlideStackComplete_msgToUi& msg)
{
  if (m_slideStackEditorDock)
  {
    m_slideStackEditorDock->setSlideStackComplete(msg);
  }
}

void GuiManager::sendActiveSlideToUi(const gui::ActiveSlide_msgToUi& msg)
{
  if (m_slideStackEditorDock)
  {
    m_slideStackEditorDock->setActiveSlide(msg);
  }
}

void GuiManager::sendSlideCommonPropertiesPartialToUi(
  const gui::SlideCommonPropertiesPartial_msgToUi& msg
)
{
  if (m_slideStackEditorDock)
  {
    m_slideStackEditorDock->setSlideCommonPropertiesPartial(msg);
  }
}

void GuiManager::sendSlideCommonPropertiesCompleteToUi(
  const gui::SlideCommonPropertiesComplete_msgToUi& msg
)
{
  if (m_slideStackEditorDock)
  {
    m_slideStackEditorDock->setCommonSlidePropertiesComplete(msg);
  }
}

void GuiManager::setSlideHeaderCompleteToUi(const gui::SlideHeaderComplete_msgToUi& msg)
{
  if (m_slideStackEditorDock)
  {
    m_slideStackEditorDock->setSlideHeaderComplete(msg);
  }
}

void GuiManager::setSlideViewDataCompleteToUi(const gui::SlideViewDataComplete_msgToUi& msg)
{
  if (m_slideStackEditorDock)
  {
    m_slideStackEditorDock->setSlideViewDataComplete(msg);
  }
}

void GuiManager::setSlideViewDataPartialToUi(const gui::SlideViewDataPartial_msgToUi& msg)
{
  if (m_slideStackEditorDock)
  {
    m_slideStackEditorDock->setSlideViewDataPartial(msg);
  }
}

void GuiManager::setSlideTxDataCompleteToUi(const gui::SlideTxDataComplete_msgToUi& msg)
{
  if (m_slideStackEditorDock)
  {
    m_slideStackEditorDock->setSlideTxDataComplete(msg);
  }
}

void GuiManager::setSlideTxDataPartialToUi(const gui::SlideTxDataPartial_msgToUi& msg)
{
  if (m_slideStackEditorDock)
  {
    m_slideStackEditorDock->setSlideTxDataPartial(msg);
  }
}

void GuiManager::setSlideStackCompleteResponder(
  gui::SlideStackComplete_msgToUi_ResponderType responder
)
{
  if (m_slideStackEditorDock)
  {
    m_slideStackEditorDock->setSlideStackCompleteResponder(responder);
  }
}

void GuiManager::setActiveSlideResponder(gui::ActiveSlide_msgToUi_ResponderType responder)
{
  if (m_slideStackEditorDock)
  {
    m_slideStackEditorDock->setActiveSlideResponder(responder);
  }
}

void GuiManager::setSlideCommonPropertiesCompleteResponder(
  gui::SlideCommonPropertiesComplete_msgToUi_ResponderType responder
)
{
  if (m_slideStackEditorDock)
  {
    m_slideStackEditorDock->setSlideCommonPropertiesCompleteResponder(responder);
  }
}

void GuiManager::setSlideHeaderCompleteResponder(
  gui::SlideHeaderComplete_msgToUi_ResponderType responder
)
{
  if (m_slideStackEditorDock)
  {
    m_slideStackEditorDock->setSlideHeaderCompleteResponder(responder);
  }
}

void GuiManager::setSlideViewDataCompleteResponder(
  gui::SlideViewDataComplete_msgToUi_ResponderType responder
)
{
  if (m_slideStackEditorDock)
  {
    m_slideStackEditorDock->setSlideViewDataCompleteResponder(responder);
  }
}

void GuiManager::setSlideTxDataCompleteResponder(
  gui::SlideTxDataComplete_msgToUi_ResponderType responder
)
{
  if (m_slideStackEditorDock)
  {
    m_slideStackEditorDock->setSlideTxDataCompleteResponder(responder);
  }
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void GuiManager::setWorldPositionStatusText(const std::string& status)
{
  if (m_mainWindow)
  {
    m_mainWindow->setWorldPositionStatusText(status);
  }
  else
  {
    throw_debug("MainWindow is null")
  }
}

void GuiManager::setImageValueStatusText(const std::string& status)
{
  if (m_mainWindow)
  {
    m_mainWindow->setImageValueStatusText(status);
  }
  else
  {
    throw_debug("MainWindow is null")
  }
}

void GuiManager::setLabelValueStatusText(const std::string& status)
{
  if (m_mainWindow)
  {
    m_mainWindow->setLabelValueStatusText(status);
  }
  else
  {
    throw_debug("MainWindow is null")
  }
}

void GuiManager::clearTabWidget()
{
  if (m_mainWindow)
  {
    m_mainWindow->clearViewLayoutTabs();
  }
  else
  {
    throw_debug("MainWindow is null")
  }
}

void GuiManager::insertViewLayoutTab(int index, QWidget* tabWidget, const std::string& name)
{
  if (m_mainWindow)
  {
    m_mainWindow->insertViewLayoutTab(index, tabWidget, name);
  }
  else
  {
    throw_debug("MainWindow is null")
  }
}

void GuiManager::updateViewWidget(gui::ViewWidget* widget)
{
  if (widget)
  {
    widget->renderUpdate();
  }
}

void GuiManager::updateViewWidget(const UID& viewUid)
{
  updateViewWidget(getViewWidget(viewUid));
}

void GuiManager::updateAllViewWidgets()
{
  for (const auto& v : m_viewWidgets)
  {
    updateViewWidget(v.second);
  }
}

void GuiManager::updateAllDockWidgets()
{
  if (m_refImageEditorDock)
  {
    m_refImageEditorDock->refresh();
  }

  if (m_slideStackEditorDock)
  {
    m_slideStackEditorDock->refresh();
  }
}

void GuiManager::createViewWidgets()
{
  /// @todo put gestures to grab in interaction manager
  QList<Qt::GestureType> gesturesToGrab;
  gesturesToGrab << Qt::PinchGesture;

  if (!m_viewUidAndTypeProvider || !m_interactionPackProvider || !m_rootDrawableProvider || !m_overlayDrawableProvider || !m_viewWidgetSetter)
  {
    return;
  }

  m_viewWidgets.clear();

  for (const auto& viewUidAndType : m_viewUidAndTypeProvider())
  {
    const auto& viewUid = viewUidAndType.first;
    const auto& viewType = viewUidAndType.second;

    auto sceneType = m_sceneTypeProvider(viewType);

    auto actionPack = m_interactionPackProvider(viewUid);
    if (!actionPack)
    {
      continue;
    }

    std::ostringstream name;
    name << "GLWidget_" << viewUid << std::ends;

    // Function returning the scene root for this view type. This adds flexibility:
    // the root can change based on view type.
    auto rootProvider = [this, &viewType](void) -> IDrawable*
    {
      if (auto root = m_rootDrawableProvider(viewType).lock())
      {
        return root.get();
      }
      else
      {
        return nullptr;
      }
    };

    // Function returning the overlay root for this view type.
    auto overlayProvider = [this, &viewType](void) -> IDrawable*
    {
      if (auto root = m_overlayDrawableProvider(viewType).lock())
      {
        return root.get();
      }
      else
      {
        return nullptr;
      }
    };

    auto renderer = createDdpRenderer(
      viewUid, m_shaderActivator, m_uniformsProvider, rootProvider, overlayProvider
    );

    if (!renderer)
    {
      std::ostringstream ss;
      ss << "Error constructing renderer " << name.str() << std::ends;
      throw_debug(ss.str());
    }

    auto cameraProvider = [this, viewUid](void) { return m_cameraQuerier(viewUid); };

    auto crosshairsProvider = [this, viewType](void) { return m_crosshairsQuerier(viewType); };

    auto interactionHandlerProvider = [this, viewUid](void) -> IInteractionHandler*
    {
      if (m_interactionHandlerQuerier)
      {
        return m_interactionHandlerQuerier(viewUid);
      }
      else
      {
        return nullptr;
      }
    };

    // The widget will eventually be added to a QLayout. At that time,
    // it will be parented. For now, we are assigning a null parent.
    auto glWidget = new gui::GLWidget(
      name.str(),
      std::move(renderer),
      cameraProvider,
      interactionHandlerProvider,
      crosshairsProvider,
      gesturesToGrab,
      nullptr
    );

    // Enable the color border for 2D scenes, which are defined by a single
    // view direction vector. 3D scenes have a perspective view frustum
    // and no single view direction
    glWidget->setEnableColorBorder(isScene2d(sceneType));

    // Create the view widget. Note that Qt widgets need to be allocated using
    // 'operator new'. Also assign the function that the widget uses to notify
    // the app of a changed slice slider value.
    auto viewWidget = new gui::ViewWidget(
      viewUid, glWidget, m_viewScrollBarValuesBroadcaster, m_viewSliceSliderValueBroadcaster
    );

    m_viewWidgets.insert({viewUid, viewWidget}); // Store the view widget
    m_viewWidgetSetter(viewWidget);              // Set the ViewWidget in the GUI's layout
  }
}
