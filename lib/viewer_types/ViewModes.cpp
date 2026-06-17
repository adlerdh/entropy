#include "viewer_types/ViewModes.h"

std::string typeString(const ViewRenderMode& mode)
{
  switch (mode) {
    case ViewRenderMode::Image:
      return "Layers";
    case ViewRenderMode::Overlay:
      return "Overlap";
    case ViewRenderMode::Checkerboard:
      return "Checkerboard";
    case ViewRenderMode::Quadrants:
      return "Quadrants";
    case ViewRenderMode::Flashlight:
      return "Flashlight";
    case ViewRenderMode::Difference:
      return "Difference";
    case ViewRenderMode::JointHistogram:
      return "Joint Histogram";
    case ViewRenderMode::VolumeRender:
      return "Volume Render";
    case ViewRenderMode::Disabled:
      return "Disabled";
    case ViewRenderMode::NumElements:
      break;
  }

  return "Unknown";
}

std::string typeString(const IntensityProjectionMode& mode)
{
  switch (mode) {
    case IntensityProjectionMode::None:
      return "None";
    case IntensityProjectionMode::Maximum:
      return "Maximum Projection";
    case IntensityProjectionMode::Mean:
      return "Mean Projection";
    case IntensityProjectionMode::Minimum:
      return "Minimum Projection";
    case IntensityProjectionMode::Xray:
      return "X-ray Projection";
    case IntensityProjectionMode::NumElements:
      break;
  }

  return "Unknown Projection";
}

std::string descriptionString(const ViewRenderMode& mode)
{
  switch (mode) {
    case ViewRenderMode::Image:
      return "Overlay of image layers";
    case ViewRenderMode::Overlay:
      return "Overlap comparison";
    case ViewRenderMode::Checkerboard:
      return "Checkerboard comparison";
    case ViewRenderMode::Quadrants:
      return "Quadrants comparison";
    case ViewRenderMode::Flashlight:
      return "Flashlight comparison";
    case ViewRenderMode::Difference:
      return "Difference metric";
    case ViewRenderMode::JointHistogram:
      return "Joint histogram metric";
    case ViewRenderMode::VolumeRender:
      return "Iso-surface volume rendering";
    case ViewRenderMode::Disabled:
      return "Disabled";
    case ViewRenderMode::NumElements:
      break;
  }

  return "Unknown render mode";
}

std::string descriptionString(const IntensityProjectionMode& mode)
{
  switch (mode) {
    case IntensityProjectionMode::None:
      return "No intensity projection";
    case IntensityProjectionMode::Maximum:
      return "Maximum intensity projection";
    case IntensityProjectionMode::Mean:
      return "Mean intensity projection";
    case IntensityProjectionMode::Minimum:
      return "Minimum intensity projection";
    case IntensityProjectionMode::Xray:
      return "X-ray intensity projection";
    case IntensityProjectionMode::NumElements:
      break;
  }

  return "Unknown intensity projection";
}
