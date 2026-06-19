#include "ui/GuiData.h"

#include <catch2/catch_test_macros.hpp>

TEST_CASE("layout tabs reserve a fixed viewport edge", "[ui][gui]")
{
  GuiData guiData;
  guiData.m_showLayoutTabs = true;
  guiData.m_layoutTabBarHeight = 42.0f;

  SECTION("top")
  {
    guiData.m_layoutTabPlacement = GuiData::LayoutTabPlacement::Top;

    const GuiData::Margins margins = guiData.computeMargins();

    CHECK(margins.top == 42.0f);
    CHECK(margins.bottom == 0.0f);
  }

  SECTION("bottom")
  {
    guiData.m_layoutTabPlacement = GuiData::LayoutTabPlacement::Bottom;

    const GuiData::Margins margins = guiData.computeMargins();

    CHECK(margins.top == 0.0f);
    CHECK(margins.bottom == 42.0f);
  }
}

TEST_CASE("hidden layout tabs do not reserve viewport space", "[ui][gui]")
{
  GuiData guiData;
  guiData.m_showLayoutTabs = false;

  const GuiData::Margins margins = guiData.computeMargins();

  CHECK(margins.top == 0.0f);
  CHECK(margins.bottom == 0.0f);
}

TEST_CASE("dock offsets include menu and visible layout tabs", "[ui][gui]")
{
  GuiData guiData;
  guiData.m_showMainMenuBar = true;
  guiData.m_mainMenuBarDims.y = 22.0f;
  guiData.m_showLayoutTabs = true;
  guiData.m_layoutTabBarHeight = 42.0f;

  SECTION("top tabs add to the top dock offset")
  {
    guiData.m_layoutTabPlacement = GuiData::LayoutTabPlacement::Top;

    CHECK(guiData.topDockOffset() == 22.0f + 42.0f);
    CHECK(guiData.bottomDockOffset() == 0.0f);
  }

  SECTION("bottom tabs add to the bottom dock offset")
  {
    guiData.m_layoutTabPlacement = GuiData::LayoutTabPlacement::Bottom;

    CHECK(guiData.topDockOffset() == 22.0f);
    CHECK(guiData.bottomDockOffset() == 42.0f);
  }

  SECTION("hidden tabs do not affect dock offsets")
  {
    guiData.m_showLayoutTabs = false;

    CHECK(guiData.topDockOffset() == 22.0f);
    CHECK(guiData.bottomDockOffset() == 0.0f);
  }
}

TEST_CASE("precision format helpers use their own precision fields", "[ui][gui]")
{
  GuiData guiData;

  SECTION("valid precision")
  {
    guiData.m_coordsPrecision = 2;
    guiData.m_txPrecision = 6;

    guiData.setCoordsPrecisionFormat();
    guiData.setTxPrecisionFormat();

    CHECK(guiData.m_coordsPrecision == 2);
    CHECK(guiData.m_coordsPrecisionFormat == "%0.2f");
    CHECK(guiData.m_txPrecision == 6);
    CHECK(guiData.m_txPrecisionFormat == "%0.6f");
  }

  SECTION("large precision")
  {
    guiData.m_coordsPrecision = 99;
    guiData.m_txPrecision = 99;

    guiData.setCoordsPrecisionFormat();
    guiData.setTxPrecisionFormat();

    CHECK(guiData.m_coordsPrecision == 9);
    CHECK(guiData.m_coordsPrecisionFormat == "%0.9f");
    CHECK(guiData.m_txPrecision == 9);
    CHECK(guiData.m_txPrecisionFormat == "%0.9f");
  }
}
