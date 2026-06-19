#include "ui/GuiData.h"

#include <algorithm>

namespace
{

std::uint32_t clampPrecision(std::uint32_t precision)
{
  constexpr std::uint32_t kMaxPrecision = 9;
  return std::min(precision, kMaxPrecision);
}

std::string precisionFormat(std::uint32_t precision)
{
  return std::string("%0.") + std::to_string(clampPrecision(precision)) + "f";
}

} // namespace

void GuiData::setCoordsPrecisionFormat()
{
  m_coordsPrecision = clampPrecision(m_coordsPrecision);
  m_coordsPrecisionFormat = precisionFormat(m_coordsPrecision);
}

void GuiData::setTxPrecisionFormat()
{
  m_txPrecision = clampPrecision(m_txPrecision);
  m_txPrecisionFormat = precisionFormat(m_txPrecision);
}

GuiData::Margins GuiData::computeMargins() const
{
  Margins margins;

  if (m_showMainMenuBar) {
    margins.top += m_mainMenuBarDims.y;
  }

  // Corners: -1 custom, 0 top-left, 1 top-right, 2 bottom-left, 3 bottom-right

  if (m_showModeToolbar) {
    if (m_isModeToolbarHorizontal) {
      if (0 == m_modeToolbarCorner || 1 == m_modeToolbarCorner) {
        margins.top = std::max(margins.top, m_modeToolbarDockDims.y);
      }
      else if (2 == m_modeToolbarCorner || 3 == m_modeToolbarCorner) {
        margins.bottom = std::max(margins.bottom, m_modeToolbarDockDims.y);
      }
    }
    else {
      if (0 == m_modeToolbarCorner || 2 == m_modeToolbarCorner) {
        margins.left = std::max(margins.left, m_modeToolbarDockDims.x);
      }
      else if (1 == m_modeToolbarCorner || 3 == m_modeToolbarCorner) {
        margins.right = std::max(margins.right, m_modeToolbarDockDims.x);
      }
    }
  }

  if (m_showSegToolbar) {
    if (m_isSegToolbarHorizontal) {
      if (0 == m_segToolbarCorner || 1 == m_segToolbarCorner) {
        margins.top = std::max(margins.top, m_segToolbarDockDims.y);
      }
      else if (2 == m_segToolbarCorner || 3 == m_segToolbarCorner) {
        margins.bottom = std::max(margins.bottom, m_segToolbarDockDims.y);
      }
    }
    else {
      if (0 == m_segToolbarCorner || 2 == m_segToolbarCorner) {
        margins.left = std::max(margins.left, m_segToolbarDockDims.x);
      }
      else if (1 == m_segToolbarCorner || 3 == m_segToolbarCorner) {
        margins.right = std::max(margins.right, m_segToolbarDockDims.x);
      }
    }
  }

  if (m_showLayoutTabs) {
    if (LayoutTabPlacement::Top == m_layoutTabPlacement) {
      margins.top += m_layoutTabBarHeight;
    }
    else {
      margins.bottom += m_layoutTabBarHeight;
    }
  }

  return margins;
}

float GuiData::topDockOffset() const
{
  float offset = 0.0f;
  if (m_showMainMenuBar) {
    offset += m_mainMenuBarDims.y;
  }
  if (m_showLayoutTabs && LayoutTabPlacement::Top == m_layoutTabPlacement) {
    offset += m_layoutTabBarHeight;
  }
  return offset;
}

float GuiData::bottomDockOffset() const
{
  return (m_showLayoutTabs && LayoutTabPlacement::Bottom == m_layoutTabPlacement) ? m_layoutTabBarHeight : 0.0f;
}
