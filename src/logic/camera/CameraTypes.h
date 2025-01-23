#ifndef CAMERA_TYPES_H
#define CAMERA_TYPES_H

#include <array>
#include <string>
#include <vector>

namespace camera
{

/**
 * @brief Types of camera projections
 */
enum class ProjectionType
{
  /// Orthographic projection is used for the "2D" views, since there's no compelling reason
  /// to use perspectvie in them. Orthographic projections make logic for zooming and rotating
  /// about arbitrary points easier.
  Orthographic,

  /// Perspective projection is used by default for the 3D views. Perspective lets lets the user
  /// fly through the scene.
  Perspective
};

/**
 * @brief View rendering mode
 */
enum class ViewRenderMode
{
  Image,        //!< Images rendered in 2D using color maps
  Overlay,      //!< Image pair rendered in 2D with overlap highlighted
  Checkerboard, //!< Image pair rendered in 2D using checkerboard pattern
  Quadrants,    //!< Image pair rendered in 2D, with each image occupying opposing view quadrants
  Flashlight, //!< Image pair rendered in 2D, with moving image appearing as circular region at crosshairs
  Difference,       //!< Absolute or squared difference of the image pair rendered in 2D
  CrossCorrelation, //!< Cross-correlation of the image pair rendered in 2D
  JointHistogram,   //!< Joint intensity histogram of the image pair
  VolumeRender,     //!< Volume rendering of one image using raycasting
  Disabled,         //!< Disabled (no rendering)
  NumElements
};

/**
 * @brief Shader group
 */
enum class ShaderGroup
{
  Image,
  Metric,
  Volume,
  None,
  NumElements
};

/**
 * @brief Intensity projection modes
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

/**
 * @brief Vector of all render modes available for 2D view types with two or more images
 */
inline std::vector<ViewRenderMode> const All2dViewRenderModes = {
  ViewRenderMode::Image,
  ViewRenderMode::Overlay,
  ViewRenderMode::Checkerboard,
  ViewRenderMode::Quadrants,
  ViewRenderMode::Flashlight,
  ViewRenderMode::Difference,
  ViewRenderMode::JointHistogram,
  ViewRenderMode::Disabled
};

/**
 * @brief Vector of all render modes available for 2D view types with only one image
 */
inline std::vector<ViewRenderMode> const All2dNonMetricRenderModes
  = {ViewRenderMode::Image, ViewRenderMode::Disabled};

/**
 * @brief Vector of all render modes available for 3D view types with two or more images
 */
inline std::vector<ViewRenderMode> const All3dViewRenderModes
  = {ViewRenderMode::VolumeRender, ViewRenderMode::Disabled};

/**
 * @brief Vector of all render modes available for 3D view types with only one image
 */
inline std::vector<ViewRenderMode> const All3dNonMetricRenderModes
  = {ViewRenderMode::VolumeRender, ViewRenderMode::Disabled};

/**
 * @brief Array of all intensity projection modes
 */
inline std::array<IntensityProjectionMode, 5> const AllIntensityProjectionModes = {
  IntensityProjectionMode::None,
  IntensityProjectionMode::Maximum,
  IntensityProjectionMode::Mean,
  IntensityProjectionMode::Minimum,
  IntensityProjectionMode::Xray
};

/**
 * @brief Get the display string of a projection type
 * @param[in] projectionType
 * @return Type string
 */
std::string typeString(const ProjectionType& projectionType);

/**
 * @brief Get the display string of a view rendering mode
 * @param[in] shaderType
 * @return Type string
 */
std::string typeString(const ViewRenderMode& renderMode);

/**
 * @brief Get the display string of an intensity projection mode
 * @param[in] ipMode
 * @return Type string
 */
std::string typeString(const IntensityProjectionMode& ipMode);

/**
 * @brief Get the description string of a view render mode
 * @param[in] renderMode
 * @return Description string
 */
std::string descriptionString(const ViewRenderMode& renderMode);

/**
 * @brief Get the description string of an intensity projection mode
 * @param[in] renderMode
 * @return Description string
 */
std::string descriptionString(const IntensityProjectionMode& ipMode);

/**
 * @brief Get the shader group for a view render mode
 * @param[in] renderMode
 * @return Shader group
 */
ShaderGroup getShaderGroup(const ViewRenderMode& renderMode);

} // namespace camera

#endif // CAMERA_TYPES_H
