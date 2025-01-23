#ifndef GUI_MANAGER_H
#define GUI_MANAGER_H

#include "gui/docks/PublicSlideTypes.h"
#include "gui/docks/PublicTypes.h"
#include "gui/layout/ViewType.h"
#include "gui/layout/ViewTypeRange.h"
#include "gui/messages/image/ImageColorMapData.h"
#include "gui/messages/image/ImageHeaderData.h"
#include "gui/messages/image/ImagePropertyData.h"
#include "gui/messages/image/ImageSelectionData.h"
#include "gui/messages/image/ImageTransformationData.h"
#include "gui/messages/parcellation/ParcellationLabelData.h"
#include "gui/messages/parcellation/ParcellationPropertyData.h"
#include "gui/messages/parcellation/ParcellationSelectionData.h"
#include "gui/messages/slide/SlideData.h"
#include "gui/messages/slide/SlideStackData.h"
#include "gui/view/ViewSliderParams.h"

#include "common/PublicTypes.h"
#include "common/UID.h"
#include "logic/interaction/InteractionHandlerType.h"

#include "rendering/common/SceneType.h"
#include "rendering/common/ShaderProviderType.h"

#include <glm/fwd.hpp>

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>

class CoordinateFrame;
class IDrawable;
class IInteractionHandler;
class InteractionPack;

class QWidget;

namespace camera
{
class Camera;
}

namespace gui
{
class ActionsContainer;
class MainWindow;
class RefFrameEditorDock;
class SlideStackEditorDock;
class ViewWidget;
} // namespace gui

/**
 * @brief This class owns the UI elements and acts as an intermediary between the UI and
 * the rest of the application.
 */
class GuiManager
{
private:
  /// Function returning a non-owning pointer to the InteractionPack for a view keyed by its UID.
  /// If the view UID does not exist, nullptr is returned.
  using InteractionPackProviderType = std::function<InteractionPack*(const UID& viewUid)>;

  /// Function returning the root Drawable for a given ViewType.
  using RootDrawableProviderType = std::function<std::weak_ptr<IDrawable>(const gui::ViewType&)>;

  /// Function returning the SceneType corresponding to a ViewType. If the view type does not exist,
  /// std::nullopt is returned.
  using SceneTypeProviderType = std::function<SceneType(const gui::ViewType&)>;

  /// Function for assigning the ViewWidget to its layout in the UI.
  using ViewWidgetToLayoutSetterType = std::function<void(gui::ViewWidget*)>;

  /// Function for loading either an anatomical image or a parcellation image. (The same
  /// interface is used to load images and parcellations.)
  using ImageLoaderType = std::function<
    void(const std::string& filename, const std::optional<std::string>& dicomSeriesUid)>;

  /// Function for loading a slide image.
  using SlideLoaderType
    = std::function<void(const std::string& filename, bool autoTranslateSlideToTopOfStack)>;

  /// Function returning the parameters for a given view's horizontal/vertical scroll bars
  /// and slice slider. The parameters are as a triple in order 1) horizontal scrollbar params,
  /// 2) vertical scrollbar params, 3) slice slider params.
  using ScrollBarsAndSliderParamsProviderType = std::function<
    std::tuple<gui::ViewSliderParams, gui::ViewSliderParams, gui::ViewSliderParams>(const UID& viewUid
    )>;

  /// Function for notifying the application of the horizontal and vertical scroll bar values
  /// in a given view. This will affect the view's Camera position.
  using ViewScrollBarValuesBroadcasterType
    = std::function<void(const UID& viewUid, double xValue, double yValue)>;

  /// Function for notifying the application of the slice slider value in a given view. This will
  /// affect the crosshairs position.
  using ViewSliceSliderValueBroadcasterType = std::function<void(const UID& viewUid, double zValue)>;

public:
  GuiManager(
    GetterType<view_type_range_t> viewUidAndTypeRangeProvider,
    ViewWidgetToLayoutSetterType viewWidgetSetter,
    InteractionPackProviderType interactionPackProvider,
    RootDrawableProviderType rootDrawableProvider,
    RootDrawableProviderType overlayDrawableProvider,
    SceneTypeProviderType sceneTypeProvider,
    ShaderProgramActivatorType shaderActivator,
    UniformsProviderType uniformsProvider
  );

  ~GuiManager();

  /// Initialize the OpenGL objects
  void initializeGL();

  void setupMainWindow();

  /// Show the application's MainWindow
  void showMainWindow();

  /// @brief Get the widget associated with a view, keyed by view UID.
  /// @return A non-owning pointer to the ViewWidget; nullptr if the view UID does not exist.
  gui::ViewWidget* getViewWidget(const UID& viewUid);

  /// Set the function that sets the view interaction mode type.
  void setInteractionModeSetter(SetterType<InteractionModeType>);

  /// Set the function that aligns crosshairs to the active slide.
  void setCrosshairsToActiveSlideAligner(CrosshairsAlignerType);

  /// Set the function that aligns crosshairs to the slide stack frame.
  void setCrosshairsToSlideStackFrameAligner(CrosshairsAlignerType);

  /// Set the function that aligns crosshairs to the anatomical planes.
  void setCrosshairsToAnatomicalPlanesAligner(CrosshairsAlignerType);

  /// Set the function that makes the Slide Stack views display slides in 3D mode
  void setSlideStackView3dModeSetter(SetterType<bool>);

  /// Set the function that provides the camera
  void setCameraQuerier(QuerierType<camera::Camera*, UID>);

  /// Set the function that provides the crosshairs
  void setCrosshairsQuerier(QuerierType<CoordinateFrame, gui::ViewType>);

  /// Set the function that provides the active interaction handler for a view
  void setInteractionHandlerQuerier(QuerierType<IInteractionHandler*, UID>);

  void setViewScrollBarsAndSliderParamsProvider(ScrollBarsAndSliderParamsProviderType);
  void setViewScrollBarValuesBroadcaster(ViewScrollBarValuesBroadcasterType);
  void setViewSliceSliderValueBroadcaster(ViewSliceSliderValueBroadcasterType);

  void setViewLayoutTabChangedBroadcaster(SetterType<int>);

  /// Set the function that resets all views to their default state:
  /// Crosshairs are centered in the reference space and cameras are aligned to look at the
  /// full extent of the reference space.
  void setAllViewsResetter(AllViewsResetterType);

  /// Set the function that saves the current project to an optional new file name
  void setProjectSaver(ProjectSaverType);

  /// Set the function that loads images.
  void setImageLoader(ImageLoaderType);

  /// Set the function that loads parcellation images.
  void setParcellationLoader(ImageLoaderType);

  /// Set the function that loads slides.
  void setSlideLoader(SlideLoaderType);

  /////////////////////////////////////////////////////////////////////////////////////
  // Start UI hook-ups

  // From the perspective of GuiManager:
  //  - "Publishers" receive message from UI
  //  - "Responders" send message to UI (at request of UI)
  //  - "Senders" send message to UI (from app)

  // Images:

  /// Set the functional that notifies the application of UI changes to image selection
  void setImageSelectionsPublisher(gui::ImageSelections_msgFromUi_PublisherType);

  /// Set the functional that responds to request from UI for image selection
  void setImageSelectionsResponder(gui::ImageSelections_msgToUi_ResponderType);

  /// Set image selection parameters for the UI
  void sendImageSelectionsToUi(const gui::ImageSelections_msgToUi&);

  /// Set the functional that responds to request from UI for image color maps
  void setImageColorMapsResponder(gui::ImageColorMaps_msgToUi_ResponderType);

  /// Set image color maps for the UI
  void sendImageColorMapsToUi(const gui::ImageColorMaps_msgToUi&);

  /// Set the functional that notifies the app of UI changes to image properties
  void setImagePropertiesPartialPublisher(gui::ImagePropertiesPartial_msgFromUi_PublisherType);

  /// Set the functional that responds to request from UI for all image properties
  void setImagePropertiesCompleteResponder(gui::ImagePropertiesComplete_msgToUi_ResponderType);

  /// Set some image property properties for the UI
  void sendImagePropertiesPartialToUi(const gui::ImagePropertiesPartial_msgToUi&);

  /// Set all image property properties for the UI
  void sendImagePropertiesCompleteToUi(const gui::ImagePropertiesComplete_msgToUi&);

  /// Set the functional that responds to request from UI for image header
  void setImageHeaderResponder(gui::ImageHeader_msgToUi_ResponderType);

  /// Set the functional that notifies the app of UI changes to image transformation
  void setImageTransformationPublisher(gui::ImageTransformation_msgFromUi_PublisherType);

  /// Set the functional that responds to request from UI for image transformation
  void setImageTransformationResponder(gui::ImageTransformation_msgToUi_ResponderType);

  /// Set the image transformation in the UI
  void sendImageTransformationToUi(const gui::ImageTransformation_msgToUi&);

  // Parcellations:

  /// Set the functional that notifies the application of UI changes to parcellation selection
  void setParcellationSelectionsPublisher(gui::ParcellationSelection_msgFromUi_PublisherType);

  /// Set the functional that responds to request from UI for parcellation selection
  void setParcellationSelectionsResponder(gui::ParcellationSelections_msgToUi_ResponderType);

  /// Set parcellation selection parameters for the UI
  void sendParcellationSelectionsToUi(const gui::ParcellationSelections_msgToUi&);

  /// Set the functional that notifies the app of UI changes to parcellation properties
  void setParcellationPropertiesPartialPublisher(
    gui::ParcellationPropertiesPartial_msgFromUi_PublisherType
  );

  /// Set the functional that responds to request from UI for all parcellation properties
  void setParcellationPropertiesCompleteResponder(
    gui::ParcellationPropertiesComplete_msgToUi_ResponderType
  );

  /// Set some parcellation properties for the UI
  void sendParcellationPropertiesPartialToUi(const gui::ParcellationPropertiesPartial_msgToUi&);

  /// Set all parcellation properties for the UI
  void sendParcellationPropertiesCompleteToUi(const gui::ParcellationPropertiesComplete_msgToUi&);

  /// Set the functional that responds to request from UI for image header
  void setParcellationHeaderResponder(gui::ParcellationHeader_msgToUi_ResponderType);

  /// Set the functional that notifies the app of UI changes to parcellation labels
  void setParcellationLabelsPartialPublisher(gui::ParcellationLabelsPartial_msgFromUi_PublisherType);

  /// Set the functional that responds to request from UI for all parcellation labels
  void setParcellationLabelsCompleteResponder(gui::ParcellationLabelsComplete_msgToUi_ResponderType);

  /// Set all parcellation labels in the UI
  void sendParcellationLabelsCompleteToUi(const gui::ParcellationLabelsComplete_msgToUi&);

  // Slides:

  /// Set the functional that notifies the app of UI changes to slide stack
  void setSlideStackPartialPublisher(gui::SlideStackPartial_msgFromUi_PublisherType);

  /// Set the functional that notifies the app of UI changes to slide stack ordering
  void setSlideStackOrderPublisher(gui::SlideStackOrder_msgFromUi_PublisherType);

  /// Set the functional that notifies the app of UI changes to the active slide
  void setActiveSlidePublisher(gui::ActiveSlide_msgFromUi_PublisherType);

  /// Set the functional that notifies the app of UI changes to some slide stack common properties
  void setSlideCommonPropertiesPartialPublisher(
    gui::SlideCommonPropertiesPartial_msgFromUi_PublisherType
  );

  /// Set function to notify the app of some slide header data in UI
  void setSlideHeaderPartialPublisher(gui::SlideHeaderPartial_msgFromUi_PublisherType);

  /// Set function to notify the app of some slide view data in UI
  void setSlideViewDataPartialPublisher(gui::SlideViewDataPartial_msgFromUi_PublisherType);

  /// Set function to notify the app of some slide transformation data in UI
  void setSlideTxDataPartialPublisher(gui::SlideTxDataPartial_msgFromUi_PublisherType);

  /// Set function for UI to notify the app of the slide to move to
  void setMoveToSlidePublisher(gui::MoveToSlide_msgFromUi_PublisherType);

  /// Set some slide stack data in UI
  void sendSlideStackPartialToUi(const gui::SlideStackPartial_msgToUi&);

  /// Set all slide stack data in UI
  void sendSlideStackCompleteToUi(const gui::SlideStackComplete_msgToUi&);

  /// Set active slide in UI
  void sendActiveSlideToUi(const gui::ActiveSlide_msgToUi&);

  /// Set some slide stack common properties in UI
  void sendSlideCommonPropertiesPartialToUi(const gui::SlideCommonPropertiesPartial_msgToUi&);

  /// Set all slide stack common properties in UI
  void sendSlideCommonPropertiesCompleteToUi(const gui::SlideCommonPropertiesComplete_msgToUi&);

  /// Set all slide header data in UI
  void setSlideHeaderCompleteToUi(const gui::SlideHeaderComplete_msgToUi&);

  /// Set all slide view data in UI
  void setSlideViewDataCompleteToUi(const gui::SlideViewDataComplete_msgToUi&);

  /// Set some slide view data in UI
  void setSlideViewDataPartialToUi(const gui::SlideViewDataPartial_msgToUi&);

  /// Set all slide transformation data in UI
  void setSlideTxDataCompleteToUi(const gui::SlideTxDataComplete_msgToUi&);

  /// Set some slide transformation data in UI
  void setSlideTxDataPartialToUi(const gui::SlideTxDataPartial_msgToUi&);

  /// Set the functional that responds to request from UI for all slide stack data
  void setSlideStackCompleteResponder(gui::SlideStackComplete_msgToUi_ResponderType);

  /// Set the functional that responds to request from UI for the active slide
  void setActiveSlideResponder(gui::ActiveSlide_msgToUi_ResponderType);

  /// Set the functional that responds to request from UI for all slide stack common properties
  void setSlideCommonPropertiesCompleteResponder(
    gui::SlideCommonPropertiesComplete_msgToUi_ResponderType
  );

  /// Set function that provides the UI with all slide header data
  void setSlideHeaderCompleteResponder(gui::SlideHeaderComplete_msgToUi_ResponderType);

  /// Set function that provides the UI with all slide view data
  void setSlideViewDataCompleteResponder(gui::SlideViewDataComplete_msgToUi_ResponderType);

  /// Set function that provides the UI with all slide transformation data
  void setSlideTxDataCompleteResponder(gui::SlideTxDataComplete_msgToUi_ResponderType);

  // End UI hook-ups
  /////////////////////////////////////////////////////////////////////////////////////

  /// Set the World Position status text
  void setWorldPositionStatusText(const std::string& status);

  /// Set the Image Value status text
  void setImageValueStatusText(const std::string& status);

  /// Set the Label Value status text
  void setLabelValueStatusText(const std::string& status);

  /// Clear the TabWidget
  void clearTabWidget();

  /// Set the widget for a tab of the TabWidget
  void insertViewLayoutTab(int index, QWidget* tabWidget, const std::string& name);

  /// Update the given view by setting its slice slider parameters and
  /// enqueueing a render of the widget. This requires an active GL context.
  void updateViewWidget(const UID& viewUid);

  /// Update all views by setting their slice slider parameters and
  /// enqueueing renders of their widget. This requires an active GL context.
  void updateAllViewWidgets();

  /// Update all dock widgets with their correct property values.
  void updateAllDockWidgets();

private:
  void createViewWidgets();

  /// Update all components of a view widget, including its rendering, scroll bars, and slice slider
  void updateViewWidget(gui::ViewWidget* widget);

  GetterType<view_type_range_t> m_viewUidAndTypeProvider;
  ViewWidgetToLayoutSetterType m_viewWidgetSetter;
  InteractionPackProviderType m_interactionPackProvider;
  RootDrawableProviderType m_rootDrawableProvider;
  RootDrawableProviderType m_overlayDrawableProvider;
  SceneTypeProviderType m_sceneTypeProvider;

  ScrollBarsAndSliderParamsProviderType m_viewScrollBarsAndSliderParamsProvider;

  ViewScrollBarValuesBroadcasterType m_viewScrollBarValuesBroadcaster;
  ViewSliceSliderValueBroadcasterType m_viewSliceSliderValueBroadcaster;

  SetterType<int> m_viewLayoutTabChangedBroadcaster;

  ShaderProgramActivatorType m_shaderActivator;
  UniformsProviderType m_uniformsProvider;

  SetterType<InteractionModeType> m_interactionModeSetter;
  CrosshairsAlignerType m_crosshairsToActiveSlideAligner;
  CrosshairsAlignerType m_crosshairsToSlideStackAligner;
  CrosshairsAlignerType m_crosshairsToAnatomicalPlanesAligner;
  AllViewsResetterType m_allViewsResetter;
  ProjectSaverType m_projectSaver;
  SetterType<bool> m_slideStackViews3dModeSetter;

  QuerierType<camera::Camera*, UID> m_cameraQuerier;
  QuerierType<CoordinateFrame, gui::ViewType> m_crosshairsQuerier;
  QuerierType<IInteractionHandler*, UID> m_interactionHandlerQuerier;

  std::unique_ptr<gui::ActionsContainer> m_actionsContainer; //!< Holds the GUI's QActions

  std::unique_ptr<gui::MainWindow> m_mainWindow; //!< Main window

  /// Dock widget for controlling reference images and their parcellations.
  /// (Per Qt's widget parenting architecture, widgets are held as raw pointers)
  gui::RefFrameEditorDock* m_refImageEditorDock;

  /// Dock widget for controlling the slide stack
  gui::SlideStackEditorDock* m_slideStackEditorDock;

  /// View widgets, keyed by their UID
  std::unordered_map<UID, gui::ViewWidget*> m_viewWidgets;
};

#endif // GUI_MANAGER_H
