#include "ui/GuiData.h"

#include <catch2/catch_test_macros.hpp>

TEST_CASE("layout tabs reserve a fixed viewport edge", "[ui][gui]")
{
  GuiData guiData;
  guiData.m_renderUiWindows = true;
  guiData.m_showLayoutTabs = true;
  guiData.m_layoutTabBarHeight = 42.0f;
  guiData.m_layoutTabInnerGap = 2.0f;

  SECTION("top")
  {
    guiData.m_layoutTabPlacement = GuiData::LayoutTabPlacement::Top;

    const GuiData::Margins margins = guiData.computeMargins();

    CHECK(margins.top == 44.0f);
    CHECK(margins.bottom == 0.0f);
  }

  SECTION("bottom")
  {
    guiData.m_layoutTabPlacement = GuiData::LayoutTabPlacement::Bottom;

    const GuiData::Margins margins = guiData.computeMargins();

    CHECK(margins.top == 0.0f);
    CHECK(margins.bottom == 44.0f);
  }
}

TEST_CASE("hidden layout tabs do not reserve viewport space", "[ui][gui]")
{
  GuiData guiData;
  guiData.m_renderUiWindows = true;
  guiData.m_showLayoutTabs = false;

  const GuiData::Margins margins = guiData.computeMargins();

  CHECK(margins.top == 0.0f);
  CHECK(margins.bottom == 0.0f);
}

TEST_CASE("dock offsets include menu and visible layout tabs", "[ui][gui]")
{
  GuiData guiData;
  guiData.m_renderUiWindows = true;
  guiData.m_showMainMenuBar = true;
  guiData.m_mainMenuBarDims.y = 22.0f;
  guiData.m_showLayoutTabs = true;
  guiData.m_layoutTabBarHeight = 42.0f;
  guiData.m_layoutTabInnerGap = 2.0f;

  SECTION("top tabs add to the top dock offset")
  {
    guiData.m_layoutTabPlacement = GuiData::LayoutTabPlacement::Top;

    CHECK(guiData.topDockOffset() == 22.0f + 44.0f);
    CHECK(guiData.bottomDockOffset() == 0.0f);
  }

  SECTION("bottom tabs add to the bottom dock offset")
  {
    guiData.m_layoutTabPlacement = GuiData::LayoutTabPlacement::Bottom;

    CHECK(guiData.topDockOffset() == 22.0f);
    CHECK(guiData.bottomDockOffset() == 44.0f);
  }

  SECTION("hidden tabs do not affect dock offsets")
  {
    guiData.m_showLayoutTabs = false;

    CHECK(guiData.topDockOffset() == 22.0f);
    CHECK(guiData.bottomDockOffset() == 0.0f);
  }
}

TEST_CASE("toolbar margins reserve only the occupied viewport edges", "[ui][gui]")
{
  GuiData guiData;
  guiData.m_renderUiWindows = true;
  guiData.m_showMainMenuBar = true;
  guiData.m_mainMenuBarDims.y = 22.0f;
  guiData.m_showLayoutTabs = true;
  guiData.m_layoutTabBarHeight = 42.0f;
  guiData.m_layoutTabInnerGap = 2.0f;
  guiData.m_layoutTabPlacement = GuiData::LayoutTabPlacement::Top;

  guiData.m_showModeToolbar = true;
  guiData.m_isModeToolbarHorizontal = false;
  guiData.m_modeToolbarCorner = 1;
  guiData.m_modeToolbarDockDims = glm::vec2{48.0f, 320.0f};

  guiData.m_showSegToolbar = true;
  guiData.m_isSegToolbarHorizontal = true;
  guiData.m_segToolbarCorner = 2;
  guiData.m_segToolbarDockDims = glm::vec2{280.0f, 36.0f};

  const GuiData::Margins toolbarMargins = guiData.computeToolbarMargins();

  CHECK(toolbarMargins.left == 0.0f);
  CHECK(toolbarMargins.right == 48.0f);
  CHECK(toolbarMargins.top == 0.0f);
  CHECK(toolbarMargins.bottom == 36.0f);

  const GuiData::Margins fullMargins = guiData.computeMargins();

  CHECK(fullMargins.left == 0.0f);
  CHECK(fullMargins.right == 48.0f);
  CHECK(fullMargins.top == 22.0f + 44.0f);
  CHECK(fullMargins.bottom == 36.0f);
}

TEST_CASE("hidden user interface does not reserve viewport space", "[ui][gui]")
{
  GuiData guiData;
  guiData.m_renderUiWindows = false;
  guiData.m_showMainMenuBar = true;
  guiData.m_mainMenuBarDims.y = 22.0f;
  guiData.m_showLayoutTabs = true;
  guiData.m_layoutTabBarHeight = 42.0f;
  guiData.m_layoutTabInnerGap = 2.0f;
  guiData.m_showModeToolbar = true;
  guiData.m_isModeToolbarHorizontal = false;
  guiData.m_modeToolbarCorner = 0;
  guiData.m_modeToolbarDockDims = glm::vec2{48.0f, 320.0f};
  guiData.m_showSegToolbar = true;
  guiData.m_isSegToolbarHorizontal = true;
  guiData.m_segToolbarCorner = 2;
  guiData.m_segToolbarDockDims = glm::vec2{280.0f, 36.0f};

  const GuiData::Margins toolbarMargins = guiData.computeToolbarMargins();
  const GuiData::Margins margins = guiData.computeMargins();

  CHECK(toolbarMargins.left == 0.0f);
  CHECK(toolbarMargins.right == 0.0f);
  CHECK(toolbarMargins.top == 0.0f);
  CHECK(toolbarMargins.bottom == 0.0f);
  CHECK(margins.left == 0.0f);
  CHECK(margins.right == 0.0f);
  CHECK(margins.top == 0.0f);
  CHECK(margins.bottom == 0.0f);
  CHECK(guiData.topDockOffset() == 0.0f);
  CHECK(guiData.bottomDockOffset() == 0.0f);
}

TEST_CASE("precision format helpers use their own precision fields", "[ui][gui]")
{
  GuiData guiData;

  SECTION("valid precision")
  {
    guiData.m_coordsPrecision = 2;
    guiData.m_txPrecision = 6;
    guiData.m_timeValuePrecision = 4;

    guiData.setCoordsPrecisionFormat();
    guiData.setTxPrecisionFormat();
    guiData.setTimeValuePrecisionFormat();

    CHECK(guiData.m_coordsPrecision == 2);
    CHECK(guiData.m_coordsPrecisionFormat == "%0.2f");
    CHECK(guiData.m_txPrecision == 6);
    CHECK(guiData.m_txPrecisionFormat == "%0.6f");
    CHECK(guiData.m_timeValuePrecision == 4);
    CHECK(guiData.m_timeValuePrecisionFormat == "%0.4f");
  }

  SECTION("large precision")
  {
    guiData.m_coordsPrecision = 99;
    guiData.m_txPrecision = 99;
    guiData.m_timeValuePrecision = 99;

    guiData.setCoordsPrecisionFormat();
    guiData.setTxPrecisionFormat();
    guiData.setTimeValuePrecisionFormat();

    CHECK(guiData.m_coordsPrecision == 9);
    CHECK(guiData.m_coordsPrecisionFormat == "%0.9f");
    CHECK(guiData.m_txPrecision == 9);
    CHECK(guiData.m_txPrecisionFormat == "%0.9f");
    CHECK(guiData.m_timeValuePrecision == 9);
    CHECK(guiData.m_timeValuePrecisionFormat == "%0.9f");
  }
}
