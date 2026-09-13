#pragma once

#include "common/Types.h"
#include "logic/interaction/TransformationGuide.h"

#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

struct NVGcontext;
class View;
class Viewport;

namespace rendering::vector_overlay
{

/**
 * @brief Draw a projected world-space translation guide in a 2D view.
 *
 * Renders the total translation and its Cartesian component arrows, optionally
 * including numeric parameter labels. Drawing is clipped to the target frame.
 *
 * @param nvg NanoVG context that receives the guide drawing.
 * @param frameBounds Bounds used to clip the guide and constrain its labels.
 * @param windowViewport Window viewport used to project world coordinates.
 * @param view View whose camera and clip transform define the projection.
 * @param guide Translation geometry, values, and presentation state.
 * @param color RGBA color of the guide.
 * @param uiScale Scale factor applied to guide strokes, markers, and text.
 * @param precision Number of decimal places used in parameter labels.
 * @param showParameters Whether to draw numeric parameter labels.
 */
void drawTranslationGuide(
  NVGcontext* nvg,
  const FrameBounds& frameBounds,
  const Viewport& windowViewport,
  const View& view,
  const interaction::TranslationGuide& guide,
  const glm::vec4& color,
  float uiScale,
  int precision,
  bool showParameters);

/**
 * @brief Draw a projected world-space rotation guide in a 2D view.
 *
 * Renders the rotation axis, projected arc, and angle indicator, optionally
 * including numeric parameter labels. Drawing is clipped to the target frame.
 *
 * @param nvg NanoVG context that receives the guide drawing.
 * @param frameBounds Bounds used to clip the guide and constrain its labels.
 * @param windowViewport Window viewport used to project world coordinates.
 * @param view View whose camera and clip transform define the projection.
 * @param guide Rotation geometry, angle, and presentation state.
 * @param color RGBA color of the guide.
 * @param uiScale Scale factor applied to guide strokes, markers, and text.
 * @param precision Number of decimal places used in parameter labels.
 * @param showParameters Whether to draw numeric parameter labels.
 */
void drawRotationGuide(
  NVGcontext* nvg,
  const FrameBounds& frameBounds,
  const Viewport& windowViewport,
  const View& view,
  const interaction::RotationGuide& guide,
  const glm::vec4& color,
  float uiScale,
  int precision,
  bool showParameters);

/**
 * @brief Draw a projected world-space scale guide in a 2D view.
 *
 * Renders the original and scaled image-plane outlines together with the
 * pointer drag, optionally including numeric parameter labels.
 *
 * @param nvg NanoVG context that receives the guide drawing.
 * @param frameBounds Bounds used to clip the guide and constrain its labels.
 * @param windowViewport Window viewport used to project world coordinates.
 * @param view View whose camera and clip transform define the projection.
 * @param slicePlaneOriginWorld World-space origin of the displayed slice plane.
 * @param guide Scale geometry, factors, and presentation state.
 * @param color RGBA color of the guide.
 * @param uiScale Scale factor applied to guide strokes, markers, and text.
 * @param precision Number of decimal places used in parameter labels.
 * @param showParameters Whether to draw numeric parameter labels.
 */
void drawScaleGuide(
  NVGcontext* nvg,
  const FrameBounds& frameBounds,
  const Viewport& windowViewport,
  const View& view,
  const glm::vec3& slicePlaneOriginWorld,
  const interaction::ScaleGuide& guide,
  const glm::vec4& color,
  float uiScale,
  int precision,
  bool showParameters);

/**
 * @brief Draw the active transformation guide in a 2D view.
 *
 * Dispatches the transformation variant to the translation, rotation, or
 * scale renderer. The slice-plane origin is used only by scale guides.
 *
 * @param nvg NanoVG context that receives the guide drawing.
 * @param frameBounds Bounds used to clip the guide and constrain its labels.
 * @param windowViewport Window viewport used to project world coordinates.
 * @param view View whose camera and clip transform define the projection.
 * @param slicePlaneOriginWorld World-space origin of the displayed slice plane.
 * @param guide Transformation guide variant and presentation state.
 * @param color RGBA color of the guide.
 * @param uiScale Scale factor applied to guide strokes, markers, and text.
 * @param precision Number of decimal places used in parameter labels.
 * @param showParameters Whether to draw numeric parameter labels.
 */
void drawTransformationGuide(
  NVGcontext* nvg,
  const FrameBounds& frameBounds,
  const Viewport& windowViewport,
  const View& view,
  const glm::vec3& slicePlaneOriginWorld,
  const interaction::TransformationGuide& guide,
  const glm::vec4& color,
  float uiScale,
  int precision,
  bool showParameters);

} // namespace rendering::vector_overlay
