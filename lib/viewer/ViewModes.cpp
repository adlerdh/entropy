#include "viewer/ViewModes.h"

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
    case ViewRenderMode::LocalNcc:
      return "Local NCC";
    case ViewRenderMode::LocalLinearResidual:
      return "Local linear residual";
    case ViewRenderMode::JointHistogram:
      return "Joint histogram";
    case ViewRenderMode::VolumeRender:
      return "Volume render isosurfaces";
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
      return "Maximum projection";
    case IntensityProjectionMode::Mean:
      return "Mean projection";
    case IntensityProjectionMode::Minimum:
      return "Minimum projection";
    case IntensityProjectionMode::Xray:
      return "X-ray projection";
    case IntensityProjectionMode::NumElements:
      break;
  }

  return "Unknown projection";
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
    case ViewRenderMode::LocalNcc:
      return "Local normalized cross-correlation metric";
    case ViewRenderMode::LocalLinearResidual:
      return "Local linear residual metric";
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
