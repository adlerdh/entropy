#ifndef DRAWABLE_SCALING_H
#define DRAWABLE_SCALING_H

#include <array>
#include <utility>

/// Describes how a drawable object scales in size as the camera zoom level changes.
enum class ScalingMode : uint32_t
{
  /// Landmark dimension is fixed in World-space units, so that its physical size remains the
  /// same irrespective of zoom level
  FixedInPhysicalWorld,

  /// Landmark dimension is fixed in Pixel-space units, so that its size remains the same in the
  /// view irrespective of zoom level
  FixedInViewPixels
};

/// Describes the scaling along a single axis of a drawable object
struct AxisScaling
{
  float m_scale = 1.0f;
  ScalingMode m_scalingMode = ScalingMode::FixedInPhysicalWorld;
};

/// Describes the scaling of a drawable object along its three primary axes x, y, and z
using DrawableScaling = std::array<AxisScaling, 3>;

#endif // DRAWABLE_SCALING_H
