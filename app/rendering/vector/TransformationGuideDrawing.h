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

/// Draw a world-space translation and its Cartesian components in one 2D view.
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

/// Draw a projected protractor for a world-space rotation in one 2D view.
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

/// Draw old and current image slice borders plus the pointer drag for a scale operation.
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

/// Dispatch a typed transformation guide to its corresponding 2D renderer.
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
