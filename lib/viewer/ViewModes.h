#pragma once

#include <array>
#include <cstddef>
#include <string>
#include <vector>

/**
 * @brief View rendering mode.
 *
 * This mode applies to 2D image rendering. Three-dimensional scene contents are
 * selected independently with `ThreeDSceneContents`.
 */
enum class ViewRenderMode
{
  Image,               //!< Images rendered in 2D using color maps
  Checkerboard,        //!< Image pair rendered in 2D using checkerboard pattern
  Quadrants,           //!< Image pair rendered in 2D, with each image occupying opposing view quadrants
  Flashlight,          //!< Image pair rendered in 2D, with moving image appearing under the crosshairs
  Overlay,             //!< Image pair rendered in 2D with overlap highlighted
  Difference,          //!< Absolute or squared difference of the image pair rendered in 2D
  JointHistogram,      //!< Joint intensity histogram of the image pair
  LocalNcc,            //!< Local normalized cross-correlation metric for the image pair
  LocalLinearResidual, //!< Residual after fitting a local linear intensity model
  Disabled,            //!< Disabled (no 2D image rendering)
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
    case ViewRenderMode::Disabled:
    case ViewRenderMode::NumElements:
      return false;
  }
  return false;
}

/** @brief Return the 2D render-mode choices allowed for the loaded image count. */
inline const std::vector<ViewRenderMode>& twoDRenderModesForImageCount(const std::size_t imageCount)
{
  return imageCount > 1 ? All2dViewRenderModes : All2dSingleImageRenderModes;
}

/** @brief Reconcile a 2D render mode with the loaded image count. */
constexpr ViewRenderMode reconcileRenderMode(const ViewRenderMode renderMode, const std::size_t imageCount)
{
  if (imageCount < 2 && isComparisonRenderMode(renderMode)) {
    return ViewRenderMode::Image;
  }
  return renderMode;
}

/**
 * @brief Return a longer intensity projection mode description for tooltips/help text.
 * @param ipMode Intensity projection mode.
 * @return Description.
 */
std::string descriptionString(const IntensityProjectionMode& mode);
