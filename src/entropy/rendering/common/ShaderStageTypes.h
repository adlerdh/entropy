#ifndef SHADER_STAGE_TYPES_H
#define SHADER_STAGE_TYPES_H

#include <cstdint>

/**
 * @brief Describes the stages of rendering, accounting for specific steps in the
 * "Dual Depth Peeling" algorithm. N/A is used when rendering special objects (e.g. DDP quads)
 * that are not part of the scene itself.
 */
enum class RenderStage
{
  /// Rendering of opaque 3D scene objects (happens first in DDP)
  Opaque,

  /// Initialization of DDP depth buffers
  Initialize,

  /// DDP depth peel render pass of 3D scene objects
  DepthPeel,

  /// Resolve pass for DDP using a quad that is not itself part of the 3D scene or overlay layers
  QuadResolve,

  /// Rendering of 2D overlay layers (happens after 3D scene is rendered with DDP)
  Overlay
};

/**
 * @brief Describes class of Drawable objects that are to be rendered
 */
enum class ObjectsToRender
{
  /// Opaque objects only; equivalent to ( ! Translucent )
  Opaque,

  /// Translucent objects only; equivalent to ( ! Opaque )
  Translucent,

  /// Pickable objects only
  Pickable,

  /// All opaque and transparent objects; equivalent to ( Translucent & Opaque )
  All
};

/**
 * @brief Describes a type of Drawable. With 8 bits, there are 256 different types possible.
 */
enum class DrawableType : uint8_t
{
  AnnotationExtrusion = 1,
  AnnotationSlice = 2,
  BasicMesh = 3,
  Box = 4,
  CameraLabel = 5,
  Crosshairs = 6,
  DynamicTransformation = 7,
  FullScreenQuad = 8,
  ImageSlice = 9,
  Landmark = 10,
  Line = 11,
  Slide = 12,
  SlideSlice = 13,
  SlideStackArrow = 14,
  Transformation = 15,
  TexturedMesh = 16
};

#endif // SHADER_STAGE_TYPES_H
