#pragma once

#include "viewer/ViewTypes.h"

#include <array>
#include <cstddef>
#include <string>
#include <vector>

/**
 * @brief View rendering mode.
 *
 * Values are serialized; keep existing ordinals stable.
 */
enum class ViewRenderMode
{
  Image,                      //!< Images rendered in 2D using color maps
  Checkerboard,               //!< Image pair rendered in 2D using checkerboard pattern
  Quadrants,                  //!< Image pair rendered in 2D, with each image occupying opposing view quadrants
  Flashlight,                 //!< Image pair rendered in 2D, with moving image appearing under the crosshairs
  Overlay,                    //!< Image pair rendered in 2D with overlap highlighted
  Difference,                 //!< Absolute or squared difference of the image pair rendered in 2D
  JointHistogram,             //!< Joint intensity histogram of the image pair
  Isosurfaces,                //!< Image isosurfaces rendered in 3D
  Disabled,                   //!< Disabled (no rendering)
  LocalNcc,                   //!< Local normalized cross-correlation metric for the image pair
  LocalLinearResidual,        //!< Residual after fitting a local linear intensity model
  SegmentationMesh,           //!< Segmentation labels rendered as 3D meshes
  SegmentationAndIsosurfaces, //!< Segmentation meshes and image isosurfaces rendered together
  NumElements
};

/**
 * @brief Intensity projection mode.
 *
 * Values are serialized; keep existing ordinals stable.
 */
enum class IntensityProjectionMode : int
{
  None = 0,    //!< No intensity projection
  Maximum = 1, //!< Maximum intensity projection
  Mean = 2,    //!< Mean intensity projection
  Minimum = 3, //!< Minimum intensity projection
  Xray = 4,    //!< Simulation of x-ray intensity projection
  NumElements
};

/** @brief Render modes for 2D views with two or more images, in UI order. */
inline std::vector<ViewRenderMode> const All2dViewRenderModes = {
  ViewRenderMode::Image,
  ViewRenderMode::Checkerboard,
  ViewRenderMode::Quadrants,
  ViewRenderMode::Flashlight,
  ViewRenderMode::Overlay,
  ViewRenderMode::Difference,
  ViewRenderMode::LocalNcc,
  ViewRenderMode::LocalLinearResidual,
  ViewRenderMode::Disabled};

/** @brief Render modes for 2D views with fewer than two images, in UI order. */
inline std::vector<ViewRenderMode> const All2dSingleImageRenderModes = {
  ViewRenderMode::Image,
  ViewRenderMode::Disabled};

/** @brief Render modes for 3D views with two or more images, in UI order. */
inline std::vector<ViewRenderMode> const All3dViewRenderModes = {
  ViewRenderMode::SegmentationMesh,
  ViewRenderMode::Isosurfaces,
  ViewRenderMode::SegmentationAndIsosurfaces,
  ViewRenderMode::Disabled};

/** @brief Render modes for 3D views with one image, in UI order. */
inline std::vector<ViewRenderMode> const All3dNonMetricRenderModes = {
  ViewRenderMode::SegmentationMesh,
  ViewRenderMode::Isosurfaces,
  ViewRenderMode::SegmentationAndIsosurfaces,
  ViewRenderMode::Disabled};

/** @brief Intensity projection modes in UI order. */
inline std::array<IntensityProjectionMode, 5> const AllIntensityProjectionModes = {
  IntensityProjectionMode::None,
  IntensityProjectionMode::Maximum,
  IntensityProjectionMode::Mean,
  IntensityProjectionMode::Minimum,
  IntensityProjectionMode::Xray};

/**
 * @brief Return a concise user-facing render mode label.
 * @param renderMode Render mode.
 * @return Display label.
 */
std::string typeString(const ViewRenderMode& mode);

/**
 * @brief Return a concise user-facing intensity projection mode label.
 * @param ipMode Intensity projection mode.
 * @return Display label.
 */
std::string typeString(const IntensityProjectionMode& mode);

/**
 * @brief Return a longer render mode description for tooltips/help text.
 * @param renderMode Render mode.
 * @return Description.
 */
std::string descriptionString(const ViewRenderMode& mode);

/** @brief Return whether the mode is available in a 3D view. */
constexpr bool is3dRenderMode(const ViewRenderMode renderMode)
{
  return ViewRenderMode::SegmentationMesh == renderMode || ViewRenderMode::Isosurfaces == renderMode ||
         ViewRenderMode::SegmentationAndIsosurfaces == renderMode || ViewRenderMode::Disabled == renderMode;
}

/** @brief Return whether a render mode compares two images. */
constexpr bool isComparisonRenderMode(const ViewRenderMode renderMode)
{
  switch (renderMode) {
    case ViewRenderMode::Checkerboard:
    case ViewRenderMode::Quadrants:
    case ViewRenderMode::Flashlight:
    case ViewRenderMode::Overlay:
    case ViewRenderMode::Difference:
    case ViewRenderMode::JointHistogram:
    case ViewRenderMode::LocalNcc:
    case ViewRenderMode::LocalLinearResidual:
      return true;
    case ViewRenderMode::Image:
    case ViewRenderMode::Isosurfaces:
    case ViewRenderMode::Disabled:
    case ViewRenderMode::SegmentationMesh:
    case ViewRenderMode::SegmentationAndIsosurfaces:
    case ViewRenderMode::NumElements:
      return false;
  }
  return false;
}

/** @brief Return whether a render mode can be used by the given view type. */
constexpr bool isRenderModeCompatibleWithViewType(const ViewType viewType, const ViewRenderMode renderMode)
{
  if (ViewType::ThreeD == viewType) {
    return is3dRenderMode(renderMode);
  }

  switch (renderMode) {
    case ViewRenderMode::Image:
    case ViewRenderMode::Checkerboard:
    case ViewRenderMode::Quadrants:
    case ViewRenderMode::Flashlight:
    case ViewRenderMode::Overlay:
    case ViewRenderMode::Difference:
    case ViewRenderMode::Disabled:
    case ViewRenderMode::LocalNcc:
    case ViewRenderMode::LocalLinearResidual:
      return true;
    case ViewRenderMode::JointHistogram:
    case ViewRenderMode::Isosurfaces:
    case ViewRenderMode::SegmentationMesh:
    case ViewRenderMode::SegmentationAndIsosurfaces:
    case ViewRenderMode::NumElements:
      return false;
  }
  return false;
}

/** @brief Reconcile a render mode with its view type. */
constexpr ViewRenderMode reconcileRenderModeForViewType(const ViewType viewType, const ViewRenderMode renderMode)
{
  if (isRenderModeCompatibleWithViewType(viewType, renderMode)) {
    return renderMode;
  }
  return ViewType::ThreeD == viewType ? ViewRenderMode::SegmentationAndIsosurfaces : ViewRenderMode::Image;
}

/** @brief Return the 2D render-mode choices allowed for the loaded image count. */
inline const std::vector<ViewRenderMode>& twoDRenderModesForImageCount(const std::size_t imageCount)
{
  return imageCount > 1 ? All2dViewRenderModes : All2dSingleImageRenderModes;
}

/** @brief Reconcile a render mode with its view type and the loaded image count. */
constexpr ViewRenderMode
reconcileRenderMode(const ViewType viewType, const ViewRenderMode renderMode, const std::size_t imageCount)
{
  const ViewRenderMode compatibleMode = reconcileRenderModeForViewType(viewType, renderMode);

  if (imageCount < 2 && isComparisonRenderMode(compatibleMode)) {
    return ViewRenderMode::Image;
  }
  return compatibleMode;
}

/** @brief Current and cached render modes maintained while a view switches between 2D and 3D. */
struct ViewRenderModeState
{
  ViewRenderMode current = ViewRenderMode::Image;
  ViewRenderMode last2d = ViewRenderMode::Image;
  ViewRenderMode last3d = ViewRenderMode::SegmentationAndIsosurfaces;

  bool operator==(const ViewRenderModeState&) const = default;
};

/** @brief Reconcile current and cached modes after the loaded image count changes. */
constexpr ViewRenderModeState
reconcileRenderModeState(const ViewType currentViewType, ViewRenderModeState state, const std::size_t imageCount)
{
  state.last2d = reconcileRenderMode(ViewType::Axial, state.last2d, imageCount);
  state.last3d = reconcileRenderMode(ViewType::ThreeD, state.last3d, imageCount);
  state.current = reconcileRenderMode(currentViewType, state.current, imageCount);

  if (ViewType::ThreeD == currentViewType) {
    state.last3d = state.current;
  }
  else {
    state.last2d = state.current;
  }
  return state;
}

/** @brief Return whether the mode includes segmentation meshes. */
constexpr bool rendersSegmentations(const ViewRenderMode renderMode)
{
  return ViewRenderMode::SegmentationMesh == renderMode || ViewRenderMode::SegmentationAndIsosurfaces == renderMode;
}

/** @brief Return whether the mode includes image isosurfaces. */
constexpr bool rendersIsosurfaces(const ViewRenderMode renderMode)
{
  return ViewRenderMode::Isosurfaces == renderMode || ViewRenderMode::SegmentationAndIsosurfaces == renderMode;
}

/** @brief Return the 3D render mode corresponding to independently selected scene contents. */
constexpr ViewRenderMode threeDRenderMode(bool renderSegmentations, bool renderIsosurfaceSurfaces)
{
  if (renderSegmentations && renderIsosurfaceSurfaces) {
    return ViewRenderMode::SegmentationAndIsosurfaces;
  }
  if (renderSegmentations) {
    return ViewRenderMode::SegmentationMesh;
  }
  if (renderIsosurfaceSurfaces) {
    return ViewRenderMode::Isosurfaces;
  }
  return ViewRenderMode::Disabled;
}

/**
 * @brief Return a longer intensity projection mode description for tooltips/help text.
 * @param ipMode Intensity projection mode.
 * @return Description.
 */
std::string descriptionString(const IntensityProjectionMode& mode);
