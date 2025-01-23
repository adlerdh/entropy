#include "ui/GuiData.h"

void GuiData::setCoordsPrecisionFormat()
{
  m_coordsPrecisionFormat = std::string("%0.") + std::to_string(m_coordsPrecision) + "f";
}

void GuiData::setTxPrecisionFormat()
{
  m_txPrecisionFormat = std::string("%0.") + std::to_string(m_coordsPrecision) + "f";
}

GuiData::Margins GuiData::computeMargins() const
{
  Margins margins;

  if (m_showMainMenuBar)
  {
    margins.top += m_mainMenuBarDims.y;
  }

  // Corners: -1 custom, 0 top-left, 1 top-right, 2 bottom-left, 3 bottom-right

  if (m_showModeToolbar)
  {
    if (m_isModeToolbarHorizontal)
    {
      if (0 == m_modeToolbarCorner || 1 == m_modeToolbarCorner)
      {
        margins.top = std::max(margins.top, m_modeToolbarDockDims.y);
      }
      else if (2 == m_modeToolbarCorner || 3 == m_modeToolbarCorner)
      {
        margins.bottom = std::max(margins.bottom, m_modeToolbarDockDims.y);
      }
    }
    else
    {
      if (0 == m_modeToolbarCorner || 2 == m_modeToolbarCorner)
      {
        margins.left = std::max(margins.left, m_modeToolbarDockDims.x);
      }
      else if (1 == m_modeToolbarCorner || 3 == m_modeToolbarCorner)
      {
        margins.right = std::max(margins.right, m_modeToolbarDockDims.x);
      }
    }
  }

  if (m_showSegToolbar)
  {
    if (m_isSegToolbarHorizontal)
    {
      if (0 == m_segToolbarCorner || 1 == m_segToolbarCorner)
      {
        margins.top = std::max(margins.top, m_segToolbarDockDims.y);
      }
      else if (2 == m_segToolbarCorner || 3 == m_segToolbarCorner)
      {
        margins.bottom = std::max(margins.bottom, m_segToolbarDockDims.y);
      }
    }
    else
    {
      if (0 == m_segToolbarCorner || 2 == m_segToolbarCorner)
      {
        margins.left = std::max(margins.left, m_segToolbarDockDims.x);
      }
      else if (1 == m_segToolbarCorner || 3 == m_segToolbarCorner)
      {
        margins.right = std::max(margins.right, m_segToolbarDockDims.x);
      }
    }
  }

  return margins;
}