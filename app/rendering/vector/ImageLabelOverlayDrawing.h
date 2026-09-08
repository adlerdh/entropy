#pragma once

#include "common/Types.h"

#include <glm/vec3.hpp>

#include <array>
#include <span>
#include <string>
#include <string_view>

struct NVGcontext;

namespace rendering::vector_overlay
{

/**
 * @brief Presentation data for one image named in a view overlay.
 *
 * This deliberately contains no application objects. The application resolves image selection and roles before
 * handing the immutable presentation model to the vector renderer.
 */
struct ImageLabelEntry
{
  std::string displayName;         //!< User-visible image name
  glm::vec3 identificationColor{}; //!< Image identification color, with RGB components from zero to one
  bool isReference = false;        //!< Whether this is the project reference image
  bool isActive = false;           //!< Whether this is the active image
  bool isVisible = true;           //!< Whether image and active-component visibility are both enabled
  float effectiveOpacity = 1.0f;   //!< Product of image-wide and active-component opacity
};

enum class ImageSwatchMode
{
  Opaque,      //!< Solid identification color
  Translucent, //!< Checkerboard visible through the identification color
  Hidden       //!< Checkerboard crossed out in the identification color
};

/** @brief Classify how an image's identification swatch communicates visibility and opacity. */
constexpr ImageSwatchMode imageSwatchMode(const ImageLabelEntry& entry)
{
  if (!entry.isVisible || entry.effectiveOpacity <= 0.0f) {
    return ImageSwatchMode::Hidden;
  }
  return entry.effectiveOpacity < 1.0f ? ImageSwatchMode::Translucent : ImageSwatchMode::Opaque;
}

/**
 * @brief Return the ordered compact badges shown for an image's project roles.
 *
 * Empty elements represent absent roles. Reference is intentionally shown before active when both apply.
 */
constexpr std::array<std::string_view, 2> imageRoleBadgeLabels(const ImageLabelEntry& entry)
{
  return {entry.isReference ? "REF" : "", entry.isActive ? "ACTIVE" : ""};
}

/**
 * @brief Draw the image list beneath the controls at the top-left of a rendered frame.
 *
 * @param nvg NanoVG context.
 * @param frameBounds Frame bounds in top-left-origin miewport coordinates.
 * @param entries Ordered visible image entries.
 * @param uiScale Effective application UI scale. Device-pixel ratio is handled by NanoVG's frame setup.
 * @param controlBottomOffset Actual bottom edge of the ImGui controls relative to the frame top.
 */
void drawImageLabelOverlay(
  NVGcontext* nvg,
  const FrameBounds& frameBounds,
  std::span<const ImageLabelEntry> entries,
  float uiScale,
  float controlBottomOffset);

} // namespace rendering::vector_overlay
