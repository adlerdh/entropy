#pragma once

#include <array>
#include <string>
#include <vector>

/**
 * @brief View rendering mode.
 *
 * Values are serialized; keep existing ordinals stable.
 */
enum class ViewRenderMode
{
  Image,          //!< Images rendered in 2D using color maps.
  Checkerboard,   //!< Image pair rendered in 2D using checkerboard pattern.
  Quadrants,      //!< Image pair rendered in 2D, with each image occupying opposing view quadrants.
  Flashlight,     //!< Image pair rendered in 2D, with moving image appearing under the crosshairs.
  Overlay,        //!< Image pair rendered in 2D with overlap highlighted.
  Difference,     //!< Absolute or squared difference of the image pair rendered in 2D.
  JointHistogram, //!< Joint intensity histogram of the image pair.
  VolumeRender,   //!< Volume rendering of one image using raycasting.
  Disabled,       //!< Disabled (no rendering).
  LocalNcc,       //!< Local normalized cross-correlation metric for the image pair.
  NumElements
};

/**
 * @brief Intensity projection mode.
 *
 * Values are serialized; keep existing ordinals stable.
 */
enum class IntensityProjectionMode : int
{
  None = 0,    //!< No intensity projection.
  Maximum = 1, //!< Maximum intensity projection.
  Mean = 2,    //!< Mean intensity projection.
  Minimum = 3, //!< Minimum intensity projection.
  Xray = 4,    //!< Simulation of x-ray intensity projection.
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
  ViewRenderMode::JointHistogram,
  ViewRenderMode::Disabled};

/** @brief Render modes for 2D views with one image, in UI order. */
inline std::vector<ViewRenderMode> const All2dNonMetricRenderModes = {ViewRenderMode::Image, ViewRenderMode::Disabled};

/** @brief Render modes for 3D views with two or more images, in UI order. */
inline std::vector<ViewRenderMode> const All3dViewRenderModes = {
  ViewRenderMode::VolumeRender,
  ViewRenderMode::Disabled};

/** @brief Render modes for 3D views with one image, in UI order. */
inline std::vector<ViewRenderMode> const All3dNonMetricRenderModes = {
  ViewRenderMode::VolumeRender,
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
std::string typeString(const ViewRenderMode& renderMode);

/**
 * @brief Return a concise user-facing intensity projection mode label.
 * @param ipMode Intensity projection mode.
 * @return Display label.
 */
std::string typeString(const IntensityProjectionMode& ipMode);

/**
 * @brief Return a longer render mode description for tooltips/help text.
 * @param renderMode Render mode.
 * @return Description.
 */
std::string descriptionString(const ViewRenderMode& renderMode);

/**
 * @brief Return a longer intensity projection mode description for tooltips/help text.
 * @param ipMode Intensity projection mode.
 * @return Description.
 */
std::string descriptionString(const IntensityProjectionMode& ipMode);
