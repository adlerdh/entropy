#pragma once

#include "common/Types.h"

#include <glm/fwd.hpp>
#include <glm/vec2.hpp>

#include <uuid.h>

#include <array>
#include <list>
#include <optional>
#include <string>
#include <utility>
#include <vector>

class AppData;
class View;
class Viewport;

struct NVGcontext;

using ImageSegPairs = std::vector<std::pair<std::optional<uuids::uuid>, std::optional<uuids::uuid> > >;

/**
 * @brief Begin a NanoVG frame for vector overlay drawing.
 * @param nvg NanoVG context.
 * @param windowViewport Full application window viewport.
 */
void startNvgFrame(NVGcontext* nvg, const Viewport& windowViewport);

/**
 * @brief End a NanoVG frame started by startNvgFrame().
 * @param nvg NanoVG context.
 */
void endNvgFrame(NVGcontext* nvg);

/**
 * @brief Draw the loading overlay shown while image/project data is loading.
 * @param nvg NanoVG context.
 * @param windowViewport Full application window viewport.
 */
void drawLoadingOverlay(NVGcontext* nvg, const Viewport& windowViewport);

/**
 * @brief Draw the outline around the full application window.
 * @param nvg NanoVG context.
 * @param windowViewport Full application window viewport.
 */
void drawWindowOutline(NVGcontext* nvg, const Viewport& windowViewport);

/// Highlight state for an individual view outline.
enum class ViewOutlineMode
{
  Hovered,  //!< View is under the pointer
  Selected, //!< View is selected/active for interaction
  None      //!< View has no special highlight
};

/**
 * @brief Draw the border and optional highlight around a view.
 * @param nvg NanoVG context.
 * @param miewportViewBounds View bounds in miewport coordinates, meaning mouse-oriented/flipped viewport coordinates.
 * @param outlineMode Highlight state to render.
 */
void drawViewOutline(NVGcontext* nvg, const FrameBounds& miewportViewBounds, const ViewOutlineMode& outlineMode);

/**
 * @brief Draw image slice-plane intersection outlines for images visible in a view.
 * @param nvg NanoVG context.
 * @param miewportViewBounds View bounds in miewport coordinates, meaning mouse-oriented/flipped viewport coordinates.
 * @param worldCrosshairs Current crosshairs position in world/LPS coordinates.
 * @param appData Application data containing images, settings, and render state.
 * @param view View being rendered.
 * @param imageSegPairs Image/segmentation pairs visible in the view.
 * @param renderInactiveImageIntersections Whether inactive image plane intersections should be rendered.
 */
void drawImageViewIntersections(
  NVGcontext* nvg,
  const FrameBounds& miewportViewBounds,
  const glm::vec3& worldCrosshairs,
  AppData& appData,
  const View& view,
  const ImageSegPairs& imageSegPairs,
  bool renderInactiveImageIntersections);

/**
 * @brief Draw anatomical direction labels around a 2D view.
 * @param nvg NanoVG context.
 * @param miewportViewBounds View bounds in miewport coordinates, meaning mouse-oriented/flipped viewport coordinates.
 * @param isObliqueView Whether the view is oblique relative to the displayed coordinate system.
 * @param color Label color as non-premultiplied RGBA.
 * @param anatLabelType Anatomical label vocabulary.
 * @param labelScale Text scale multiplier.
 * @param labelPosInfo Label positions and label indices computed for the current view orientation.
 */
void drawAnatomicalLabels(
  NVGcontext* nvg,
  const FrameBounds& miewportViewBounds,
  bool isObliqueView,
  const glm::vec4& color,
  const AnatomicalLabelType& anatLabelType,
  float labelScale,
  const std::array<AnatomicalLabelPosInfo, 2>& labelPosInfo);

/**
 * @brief Draw a filled and stroked circle overlay.
 * @param nvg NanoVG context.
 * @param miewportPos Circle center in miewport coordinates, meaning mouse-oriented/flipped viewport coordinates.
 * @param radius Circle radius in screen pixels.
 * @param fillColor Fill color as non-premultiplied RGBA.
 * @param strokeColor Stroke color as non-premultiplied RGBA.
 * @param strokeWidth Stroke width in screen pixels.
 */
void drawCircle(
  NVGcontext* nvg,
  const glm::vec2& miewportPos,
  float radius,
  const glm::vec4& fillColor,
  const glm::vec4& strokeColor,
  float strokeWidth);

/**
 * @brief Draw centered text plus optional offset text at a viewport position.
 * @param nvg NanoVG context.
 * @param miewportPos Anchor position in miewport coordinates, meaning mouse-oriented/flipped viewport coordinates.
 * @param centeredString Text centered on the anchor position.
 * @param offsetString Text drawn down/right from the anchor position.
 * @param textColor Text color as non-premultiplied RGBA.
 * @param offset Offset, in screen pixels, used for the offset string.
 * @param fontSizePixels Base font size in screen pixels.
 */
void drawText(
  NVGcontext* nvg,
  const glm::vec2& miewportPos,
  const std::string& centeredString,
  const std::string& offsetString,
  const glm::vec4& textColor,
  float offset,
  float fontSizePixels);

/**
 * @brief Draw landmarks associated with the images visible in a view.
 * @param nvg NanoVG context.
 * @param miewportViewBounds View bounds in miewport coordinates, meaning mouse-oriented/flipped viewport coordinates.
 * @param worldCrosshairs Current crosshairs position in world/LPS coordinates.
 * @param appData Application data containing landmarks, images, and render state.
 * @param view View being rendered.
 * @param imageSegPairs Image/segmentation pairs visible in the view.
 */
void drawLandmarks(
  NVGcontext* nvg,
  const FrameBounds& miewportViewBounds,
  const glm::vec3& worldCrosshairs,
  AppData& appData,
  const View& view,
  const ImageSegPairs& imageSegPairs);

/**
 * @brief Draw vector annotations associated with the images visible in a view.
 * @param nvg NanoVG context.
 * @param miewportViewBounds View bounds in miewport coordinates, meaning mouse-oriented/flipped viewport coordinates.
 * @param worldCrosshairs Current crosshairs position in world/LPS coordinates.
 * @param appData Application data containing annotations, images, and render state.
 * @param view View being rendered.
 * @param imageSegPairs Image/segmentation pairs visible in the view.
 */
void drawAnnotations(
  NVGcontext* nvg,
  const FrameBounds& miewportViewBounds,
  const glm::vec3& worldCrosshairs,
  AppData& appData,
  const View& view,
  const ImageSegPairs& imageSegPairs);

/**
 * @brief Draw sampled vector-field arrows over the current image slice.
 * @param nvg NanoVG drawing context.
 * @param miewportViewBounds View bounds in miewport coordinates, meaning mouse-oriented/flipped viewport coordinates.
 * @param worldCrosshairs Current view slice point in world/LPS coordinates.
 * @param appData Application data containing images and window state.
 * @param view View being rendered.
 * @param imageUids Logical images visible in the view.
 */
void drawVectorFieldArrows(
  NVGcontext* nvg,
  const FrameBounds& miewportViewBounds,
  const glm::vec3& worldCrosshairs,
  AppData& appData,
  const View& view,
  const std::list<uuids::uuid>& imageUids);

/**
 * @brief Draw crosshair lines for a 2D view.
 * @param nvg NanoVG context.
 * @param miewportViewBounds View bounds in miewport coordinates, meaning mouse-oriented/flipped viewport coordinates.
 * @param view View being rendered.
 * @param color Crosshairs color as non-premultiplied RGBA.
 * @param labelPosInfo Crosshair line positions computed from the current view orientation.
 */
void drawCrosshairs(
  NVGcontext* nvg,
  const FrameBounds& miewportViewBounds,
  const View& view,
  const glm::vec4& color,
  const std::array<AnatomicalLabelPosInfo, 2>& labelPosInfo);
