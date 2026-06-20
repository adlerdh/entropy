#include "logic/camera/CameraTypes.h"

std::string typeString(const ProjectionType& projectionType)
{
  switch (projectionType) {
    case ProjectionType::Orthographic:
      return "Orthographic";
    case ProjectionType::Perspective:
      return "Perspective";
  }

  return "Unknown";
}

ShaderGroup getShaderGroup(const ViewRenderMode& renderMode)
{
  switch (renderMode) {
    case ViewRenderMode::Image:
    case ViewRenderMode::Checkerboard:
    case ViewRenderMode::Quadrants:
    case ViewRenderMode::Flashlight: {
      return ShaderGroup::Image;
    }

    case ViewRenderMode::Overlay:
    case ViewRenderMode::Difference:
    case ViewRenderMode::LocalNcc:
    case ViewRenderMode::JointHistogram: {
      return ShaderGroup::Metric;
    }

    case ViewRenderMode::VolumeRender: {
      return ShaderGroup::Volume;
    }

    case ViewRenderMode::Disabled:
    default: {
      return ShaderGroup::None;
    }
  }
}
