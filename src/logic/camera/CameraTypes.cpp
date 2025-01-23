#include "logic/camera/CameraTypes.h"

#include <unordered_map>

namespace camera
{

std::string typeString(const ProjectionType& projectionType)
{
  static const std::unordered_map<ProjectionType, std::string> s_typeToStringMap{
    {ProjectionType::Orthographic, "Orthographic"}, {ProjectionType::Perspective, "Perspective"}
  };

  return s_typeToStringMap.at(projectionType);
}

std::string typeString(const ViewRenderMode& mode)
{
  static const std::unordered_map<ViewRenderMode, std::string> s_modeToStringMap{
    {ViewRenderMode::Image, "Layers"},
    {ViewRenderMode::Overlay, "Overlap"},
    {ViewRenderMode::Checkerboard, "Checkerboard"},
    {ViewRenderMode::Quadrants, "Quadrants"},
    {ViewRenderMode::Flashlight, "Flashlight"},
    {ViewRenderMode::Difference, "Difference"},
    {ViewRenderMode::CrossCorrelation, "Correlation"},
    {ViewRenderMode::JointHistogram, "Joint Histogram"},
    {ViewRenderMode::VolumeRender, "Volume Render"},
    {ViewRenderMode::Disabled, "Disabled"}
  };

  return s_modeToStringMap.at(mode);
}

std::string typeString(const IntensityProjectionMode& mode)
{
  static const std::unordered_map<IntensityProjectionMode, std::string> s_modeToStringMap{
    {IntensityProjectionMode::None, "None"},
    {IntensityProjectionMode::Maximum, "Maximum Projection"},
    {IntensityProjectionMode::Mean, "Mean Projection"},
    {IntensityProjectionMode::Minimum, "Minimum Projection"},
    {IntensityProjectionMode::Xray, "X-ray Projection"}
  };

  return s_modeToStringMap.at(mode);
}

std::string descriptionString(const ViewRenderMode& mode)
{
  static const std::unordered_map<ViewRenderMode, std::string> s_modeToStringMap{
    {ViewRenderMode::Image, "Overlay of image layers"},
    {ViewRenderMode::Overlay, "Overlap comparison"},
    {ViewRenderMode::Checkerboard, "Checkerboard comparison"},
    {ViewRenderMode::Quadrants, "Quadrants comparison"},
    {ViewRenderMode::Flashlight, "Flashlight comparison"},
    {ViewRenderMode::Difference, "Difference metric"},
    {ViewRenderMode::CrossCorrelation, "Correlation metric"},
    {ViewRenderMode::JointHistogram, "Joint histogram metric"},
    {ViewRenderMode::VolumeRender, "Volume rendering"},
    {ViewRenderMode::Disabled, "Disabled"}
  };

  return s_modeToStringMap.at(mode);
}

std::string descriptionString(const IntensityProjectionMode& mode)
{
  static const std::unordered_map<IntensityProjectionMode, std::string> s_modeToStringMap{
    {IntensityProjectionMode::None, "No intensity projection"},
    {IntensityProjectionMode::Maximum, "Maximum intensity projection"},
    {IntensityProjectionMode::Mean, "Mean intensity projection"},
    {IntensityProjectionMode::Minimum, "Minimum intensity projection"},
    {IntensityProjectionMode::Xray, "X-ray intensity projection"}
  };

  return s_modeToStringMap.at(mode);
}

ShaderGroup getShaderGroup(const ViewRenderMode& renderMode)
{
  switch (renderMode)
  {
  case ViewRenderMode::Image:
  case ViewRenderMode::Checkerboard:
  case ViewRenderMode::Quadrants:
  case ViewRenderMode::Flashlight:
  {
    return ShaderGroup::Image;
  }

  case ViewRenderMode::Overlay:
  case ViewRenderMode::Difference:
  case ViewRenderMode::CrossCorrelation:
  case ViewRenderMode::JointHistogram:
  {
    return ShaderGroup::Metric;
  }

  case ViewRenderMode::VolumeRender:
  {
    return ShaderGroup::Volume;
  }

  case ViewRenderMode::Disabled:
  default:
  {
    return ShaderGroup::None;
  }
  }
}

} // namespace camera
