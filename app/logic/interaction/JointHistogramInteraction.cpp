#include "logic/interaction/JointHistogramInteraction.h"

#include <glm/common.hpp>

#include <algorithm>
#include <cmath>
#include <limits>

namespace joint_histogram
{
namespace
{
constexpr float kMinimumVisibleSpan = 1.0f / 1024.0f;
}

Plot plotForFrame(const FrameBounds& bounds, const float topLeftControlsBottom)
{
  constexpr float leftMargin = 76.0f;
  constexpr float rightMargin = 18.0f;
  constexpr float bottomMargin = 69.0f;
  constexpr float defaultTopMargin = 17.0f;
  constexpr float controlClearance = 8.0f;
  const float topMargin = std::max(defaultTopMargin, topLeftControlsBottom + controlClearance);
  const float availableWidth = bounds.bounds.width - leftMargin - rightMargin;
  const float availableHeight = bounds.bounds.height - bottomMargin - topMargin;
  const float size = std::min(availableWidth, availableHeight);
  if (size < 32.0f) {
    return {};
  }
  return {
    bounds.bounds.xoffset + leftMargin + 0.5f * (availableWidth - size),
    bounds.bounds.yoffset + topMargin + 0.5f * (availableHeight - size),
    size};
}

std::optional<glm::vec2>
plotCoordinates(const Plot& plot, const FrameBounds& frameBounds, const glm::vec2& viewClipPosition)
{
  if (plot.size <= 0.0f || frameBounds.bounds.width <= 0.0f || frameBounds.bounds.height <= 0.0f) {
    return std::nullopt;
  }

  const glm::vec2 positionInFrame{
    0.5f * (viewClipPosition.x + 1.0f) * frameBounds.bounds.width,
    0.5f * (1.0f - viewClipPosition.y) * frameBounds.bounds.height};
  const glm::vec2 plotOriginInFrame{plot.left - frameBounds.bounds.xoffset, plot.top - frameBounds.bounds.yoffset};
  const glm::vec2 positionFromPlotTopLeft = (positionInFrame - plotOriginInFrame) / plot.size;
  return glm::vec2{positionFromPlotTopLeft.x, 1.0f - positionFromPlotTopLeft.y};
}

bool contains(const glm::vec2& plotPosition)
{
  return 0.0f <= plotPosition.x && plotPosition.x <= 1.0f && 0.0f <= plotPosition.y && plotPosition.y <= 1.0f;
}

const glm::vec2& Navigation::visibleMinimum() const
{
  return m_visibleMinimum;
}

const glm::vec2& Navigation::visibleMaximum() const
{
  return m_visibleMaximum;
}

void Navigation::pan(const glm::vec2& plotDelta)
{
  if (!std::isfinite(plotDelta.x) || !std::isfinite(plotDelta.y)) {
    return;
  }
  const glm::vec2 span = m_visibleMaximum - m_visibleMinimum;
  m_visibleMinimum -= plotDelta * span;
  m_visibleMaximum = m_visibleMinimum + span;
  clampToDomain();
}

void Navigation::zoom(const float magnification, const glm::vec2& anchor)
{
  if (!std::isfinite(magnification) || magnification <= 0.0f || !std::isfinite(anchor.x) || !std::isfinite(anchor.y)) {
    return;
  }

  const glm::vec2 clampedAnchor = glm::clamp(anchor, glm::vec2{0.0f}, glm::vec2{1.0f});
  const glm::vec2 oldSpan = m_visibleMaximum - m_visibleMinimum;
  const glm::vec2 anchorValue = m_visibleMinimum + clampedAnchor * oldSpan;
  const glm::vec2 newSpan = glm::clamp(oldSpan / magnification, glm::vec2{kMinimumVisibleSpan}, glm::vec2{1.0f});
  m_visibleMinimum = anchorValue - clampedAnchor * newSpan;
  m_visibleMaximum = m_visibleMinimum + newSpan;
  clampToDomain();
}

void Navigation::reset()
{
  m_visibleMinimum = glm::vec2{0.0f};
  m_visibleMaximum = glm::vec2{1.0f};
}

void Navigation::clampToDomain()
{
  for (int axis = 0; axis < 2; ++axis) {
    if (m_visibleMinimum[axis] < 0.0f) {
      m_visibleMaximum[axis] -= m_visibleMinimum[axis];
      m_visibleMinimum[axis] = 0.0f;
    }
    if (m_visibleMaximum[axis] > 1.0f) {
      m_visibleMinimum[axis] -= m_visibleMaximum[axis] - 1.0f;
      m_visibleMaximum[axis] = 1.0f;
    }
    m_visibleMinimum[axis] = std::clamp(m_visibleMinimum[axis], 0.0f, 1.0f - kMinimumVisibleSpan);
    m_visibleMaximum[axis] = std::clamp(m_visibleMaximum[axis], m_visibleMinimum[axis] + kMinimumVisibleSpan, 1.0f);
  }
}

} // namespace joint_histogram
