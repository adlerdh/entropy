#pragma once

#include "common/Types.h"

#include <glm/vec2.hpp>

#include <optional>

namespace joint_histogram
{

/** Square joint-histogram plot in miewport coordinates. */
struct Plot
{
  float left = 0.0f;
  float top = 0.0f;
  float size = 0.0f;
};

/**
 * Reserve room for axis titles, tick labels, and the top-left view controls while keeping the histogram square.
 * @param bounds Rendered view bounds in top-left-origin window coordinates.
 * @param topLeftControlsBottom Bottom edge of the control row relative to the top of the view, or zero when hidden.
 */
Plot plotForFrame(const FrameBounds& bounds, float topLeftControlsBottom = 0.0f);

/**
 * Convert a view-clip pointer position to plot coordinates, where (0, 0) is the lower-left and (1, 1) is the
 * upper-right. Coordinates outside the plot are returned unchanged so drags remain continuous beyond its edges.
 */
std::optional<glm::vec2>
plotCoordinates(const Plot& plot, const FrameBounds& frameBounds, const glm::vec2& viewClipPosition);

/** Return whether a plot-coordinate position lies within the displayed histogram. */
bool contains(const glm::vec2& plotPosition);

/** Per-view navigation state for the normalized fixed/moving intensity domain. */
class Navigation
{
public:
  const glm::vec2& visibleMinimum() const;
  const glm::vec2& visibleMaximum() const;

  /** Translate the visible intensity rectangle by a drag delta expressed in plot coordinates. */
  void pan(const glm::vec2& plotDelta);

  /** Zoom about a plot-coordinate anchor. Values greater than one magnify the histogram. */
  void zoom(float magnification, const glm::vec2& anchor);

  /** Restore the complete normalized intensity domain. */
  void reset();

private:
  void clampToDomain();

  glm::vec2 m_visibleMinimum{0.0f};
  glm::vec2 m_visibleMaximum{1.0f};
};

} // namespace joint_histogram
