#pragma once

#include "common/CoordinateFrame.h"
#include "common/Types.h"
#include "logic/camera/Camera3DControls.h"
#include "logic/camera/CameraTypes.h"
#include "ui/UiControls.h"
#include "viewer/ViewModes.h"
#include "viewer/ViewTypes.h"

#include <glm/gtc/quaternion.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <uuid.h>

#include <cstddef>
#include <functional>
#include <string>
#include <utility>
#include <vector>

/**
 * @brief Immutable placement and identity information shared by per-view overlay controls.
 */
struct ViewOverlayWindowContext
{
  uuids::uuid viewOrLayoutUid;       //!< View or layout identifier used for ImGui IDs and bulk actions.
  FrameBounds viewFrameBounds;       //!< Pixel bounds of the rendered view frame.
  UiControls uiControls;             //!< Flags describing which overlay controls the view supports.
  bool showApplyToAllButton = false; //!< Whether to show the apply-to-all-views button.
  bool allowImageSelection = true;   //!< Whether the image-selection popup is editable.
  CoordinateFrame worldCrosshairs;   //!< Current world crosshairs frame.
  glm::vec2 contentScales{1.0f};     //!< Platform content scale ratios.
};

/**
 * @brief Image-selection callbacks used by the per-view settings overlay.
 */
struct ViewOverlayImageCallbacks
{
  std::size_t numImages = 0; //!< Number of loaded images.

  std::function<bool(std::size_t index)> isImageRendered;                //!< Is image visible in this view.
  std::function<void(std::size_t index, bool visible)> setImageRendered; //!< Set image visibility in this view.
  std::function<void(const uuids::uuid& viewUid)>
    applyImageVisibilityToAllViews; //!< Copy this view's image visibility to all views.

  std::function<bool(std::size_t index)> isImageUsedForMetric; //!< Is image used in comparison/metric mode.
  std::function<void(std::size_t index, bool visible)> setImageUsedForMetric; //!< Set metric image participation.

  std::function<std::pair<std::string, std::string>(std::size_t index)>
    getImageDisplayAndFileName;                                          //!< Display/file name.
  std::function<bool(std::size_t imageIndex)> getImageVisibilitySetting; //!< Global image visibility.
  std::function<bool(std::size_t imageIndex)> getImageIsActive;          //!< Active-image predicate.
  std::function<bool(std::size_t imageIndex)> getImageIsReference;       //!< Reference-image predicate.
  std::function<bool(std::size_t imageIndex)>
    canImageBeVolumeRendered; //!< Whether this image can be selected for 3D volume rendering.
};

/**
 * @brief View-mode state and mutators used by the per-view settings overlay.
 */
struct ViewOverlayModeCallbacks
{
  ViewType viewType = ViewType::Axial;                                             //!< Current view type.
  ViewRenderMode renderMode = ViewRenderMode::Image;                               //!< Current render mode.
  IntensityProjectionMode intensityProjectionMode = IntensityProjectionMode::None; //!< Current projection mode.

  std::function<void(const ViewType& viewType)> setViewType;                               //!< Change view type.
  std::function<void(const ViewRenderMode& renderMode)> setRenderMode;                     //!< Change render mode.
  std::function<void(const IntensityProjectionMode& projMode)> setIntensityProjectionMode; //!< Change projection.
  std::function<void()> recenter; //!< Recenter after mode/type changes when needed.

  std::function<void(const uuids::uuid& viewUid)> applyImageSelectionAndShaderToAllViews; //!< Apply to all views.
  std::function<bool()> isIsosurfacesPanelVisible;           //!< Is the isosurfaces panel currently visible.
  std::function<void()> showIsosurfacesPanel;                //!< Open the isosurfaces panel.
  std::function<void()> showIsosurfacesPanelForRaycastImage; //!< Open the panel for the current raycast image.
  std::function<void()>
    showIsosurfacesPanelForMissingRaycastImageIsosurface;      //!< Open the panel if the raycast image has no surfaces.
  std::function<ProjectionType()> getThreeDProjectionType;     //!< Current 3D projection type.
  std::function<void(ProjectionType)> setThreeDProjectionType; //!< Set 3D projection type.
  std::function<float()> getThreeDFovAngleDegrees;             //!< Current perspective field-of-view angle.
  std::function<void(float)> setThreeDFovAngleDegrees;         //!< Set perspective field-of-view angle.
  std::function<bool()> getThreeDViewPositionFollowsCrosshairs;     //!< Whether 3D eye position follows crosshairs.
  std::function<void(bool)> setThreeDViewPositionFollowsCrosshairs; //!< Set whether 3D eye position follows crosshairs.
  std::function<camera3d::OrbitTargetMode()> getThreeDOrbitTargetMode;     //!< Current 3D orbit target mode.
  std::function<void(camera3d::OrbitTargetMode)> setThreeDOrbitTargetMode; //!< Set 3D orbit target mode.
  std::function<bool()> getThreeDRenderImageBox;                           //!< Whether 3D raycast image box is drawn.
  std::function<void(bool)> setThreeDRenderImageBox;                       //!< Set 3D raycast image box drawing.
  std::function<std::optional<std::string>()> exportAsciiText;             //!< Export this view's ASCII text.
  std::vector<ViewType> selectableViewTypes; //!< Empty means all supported view types are selectable.
};

/**
 * @brief Intensity-projection callbacks used by the per-view settings overlay.
 */
struct ViewOverlayProjectionCallbacks
{
  std::function<float()> getIntensityProjectionSlabThickness;               //!< Current slab thickness.
  std::function<void(float thickness)> setIntensityProjectionSlabThickness; //!< Set slab thickness.

  std::function<bool()> getDoMaxExtentIntensityProjection;         //!< Current max-extent projection state.
  std::function<void(bool set)> setDoMaxExtentIntensityProjection; //!< Set max-extent projection state.

  std::function<float()> getXrayProjectionWindow;            //!< Current x-ray projection window.
  std::function<void(float window)> setXrayProjectionWindow; //!< Set x-ray projection window.

  std::function<float()> getXrayProjectionLevel;           //!< Current x-ray projection level.
  std::function<void(float level)> setXrayProjectionLevel; //!< Set x-ray projection level.

  std::function<float()> getXrayProjectionEnergy;            //!< Current x-ray energy.
  std::function<void(float energy)> setXrayProjectionEnergy; //!< Set x-ray energy.
};

/**
 * @brief Camera callbacks used by the per-view orientation overlay.
 */
struct ViewOrientationOverlayCallbacks
{
  ViewType viewType = ViewType::Axial;              //!< Current view type.
  std::function<glm::quat()> getViewCameraRotation; //!< Current camera rotation.
  std::function<void(const glm::quat& camera_T_world_rotationDelta)> setViewCameraRotation; //!< Apply rotation delta.
  std::function<void(const glm::vec3& worldDirection)> setViewCameraDirection; //!< Set camera forward direction.
  std::function<glm::vec3()> getViewNormal;                                    //!< Current view normal.
  std::function<std::vector<glm::vec3>(const uuids::uuid& viewUidToExclude)>
    getObliqueViewDirections; //!< Other oblique normals.
};

/**
 * @brief Render the compact per-view settings overlay.
 * @param context Immutable placement and identity information.
 * @param images Image-selection callbacks.
 * @param modes View-mode callbacks.
 * @param projection Intensity-projection callbacks.
 */
void renderViewSettingsComboWindow(
  const ViewOverlayWindowContext& context,
  const ViewOverlayImageCallbacks& images,
  const ViewOverlayModeCallbacks& modes,
  const ViewOverlayProjectionCallbacks& projection);

/**
 * @brief Render the oblique-view orientation gizmo overlay.
 * @param context Immutable placement and identity information.
 * @param callbacks Camera callbacks for the active view.
 */
void renderViewOrientationToolWindow(
  const ViewOverlayWindowContext& context,
  const ViewOrientationOverlayCallbacks& callbacks);
