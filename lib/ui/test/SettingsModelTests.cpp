#include "ui/settings/SettingsModel.h"

#include <catch2/catch_test_macros.hpp>

#include <string>

namespace settings = entropy::ui::settings;

TEST_CASE("scale bar edge positions force valid orientations", "[ui][settings]")
{
  CHECK(settings::forcedScaleBarOrientation(ScaleBarPosition::Top) == ScaleBarOrientation::Horizontal);
  CHECK(settings::forcedScaleBarOrientation(ScaleBarPosition::Bottom) == ScaleBarOrientation::Horizontal);
  CHECK(settings::forcedScaleBarOrientation(ScaleBarPosition::Left) == ScaleBarOrientation::Vertical);
  CHECK(settings::forcedScaleBarOrientation(ScaleBarPosition::Right) == ScaleBarOrientation::Vertical);

  CHECK_FALSE(settings::forcedScaleBarOrientation(ScaleBarPosition::TopLeft).has_value());
  CHECK_FALSE(settings::forcedScaleBarOrientation(ScaleBarPosition::BottomRight).has_value());

  CHECK(
    settings::normalizedScaleBarOrientation(ScaleBarPosition::Top, ScaleBarOrientation::Vertical) ==
    ScaleBarOrientation::Horizontal);
  CHECK(
    settings::normalizedScaleBarOrientation(ScaleBarPosition::Left, ScaleBarOrientation::Horizontal) ==
    ScaleBarOrientation::Vertical);
  CHECK(
    settings::normalizedScaleBarOrientation(ScaleBarPosition::TopLeft, ScaleBarOrientation::Vertical) ==
    ScaleBarOrientation::Vertical);

  CHECK_FALSE(settings::canChooseScaleBarOrientation(ScaleBarPosition::Top));
  CHECK_FALSE(settings::canChooseScaleBarOrientation(ScaleBarPosition::Left));
  CHECK(settings::canChooseScaleBarOrientation(ScaleBarPosition::TopLeft));
  CHECK(settings::canChooseScaleBarOrientation(ScaleBarPosition::BottomRight));
}

TEST_CASE("scale bar position names cover every settings position", "[ui][settings]")
{
  CHECK(settings::scaleBarPositionName(ScaleBarPosition::BottomRight) == std::string("Right bottom"));
  CHECK(settings::scaleBarPositionName(ScaleBarPosition::BottomLeft) == std::string("Left bottom"));
  CHECK(settings::scaleBarPositionName(ScaleBarPosition::TopRight) == std::string("Right top"));
  CHECK(settings::scaleBarPositionName(ScaleBarPosition::TopLeft) == std::string("Left top"));
  CHECK(settings::scaleBarPositionName(ScaleBarPosition::Bottom) == std::string("Bottom"));
  CHECK(settings::scaleBarPositionName(ScaleBarPosition::Top) == std::string("Top"));
  CHECK(settings::scaleBarPositionName(ScaleBarPosition::Left) == std::string("Left"));
  CHECK(settings::scaleBarPositionName(ScaleBarPosition::Right) == std::string("Right"));
}

TEST_CASE("scale bar Settings choices stay in the intended order", "[ui][settings]")
{
  const auto& positions = settings::orderedScaleBarPositions();
  REQUIRE(positions.size() == 8);
  CHECK(positions[0] == ScaleBarPosition::BottomRight);
  CHECK(positions[1] == ScaleBarPosition::Right);
  CHECK(positions[2] == ScaleBarPosition::TopRight);
  CHECK(positions[3] == ScaleBarPosition::Top);
  CHECK(positions[4] == ScaleBarPosition::TopLeft);
  CHECK(positions[5] == ScaleBarPosition::Left);
  CHECK(positions[6] == ScaleBarPosition::BottomLeft);
  CHECK(positions[7] == ScaleBarPosition::Bottom);

  const auto& buttons = settings::scaleBarPositionButtons();
  REQUIRE(buttons.size() == 8);
  CHECK(buttons.front().position == ScaleBarPosition::TopLeft);
  CHECK(buttons.front().label == std::string("LT"));
  CHECK(buttons.back().position == ScaleBarPosition::BottomRight);
  CHECK(buttons.back().label == std::string("RB"));
}

TEST_CASE("scale bar numeric Settings values are rounded and clamped", "[ui][settings]")
{
  CHECK(settings::targetLengthPercentFromFraction(0.01f) == 5);
  CHECK(settings::targetLengthPercentFromFraction(0.23f) == 25);
  CHECK(settings::targetLengthPercentFromFraction(1.50f) == 100);

  CHECK(settings::targetLengthFractionFromPercent(1) == 0.05f);
  CHECK(settings::targetLengthFractionFromPercent(23) == 0.25f);
  CHECK(settings::targetLengthFractionFromPercent(250) == 1.0f);

  CHECK(settings::marginPixelsFromFloat(1.0f) == 12);
  CHECK(settings::marginPixelsFromFloat(42.6f) == 43);
  CHECK(settings::marginPixelsFromFloat(100.0f) == 96);
  CHECK(settings::marginFloatFromPixels(1) == 12.0f);
  CHECK(settings::marginFloatFromPixels(48) == 48.0f);
  CHECK(settings::marginFloatFromPixels(120) == 96.0f);
}

TEST_CASE("precision Settings values are clamped and formatted", "[ui][settings]")
{
  CHECK(settings::clampPrecision(0) == 0);
  CHECK(settings::clampPrecision(3) == 3);
  CHECK(settings::clampPrecision(99) == 9);

  CHECK(settings::precisionFormat(0) == "%0.0f");
  CHECK(settings::precisionFormat(3) == "%0.3f");
  CHECK(settings::precisionFormat(99) == "%0.9f");
}

TEST_CASE("visible font choices expose only production Settings fonts", "[ui][settings]")
{
  const auto& fonts = settings::visibleFontChoices();
  REQUIRE(fonts.size() == 3);
  CHECK(fonts[0].family == UiFontFamily::Inter);
  CHECK(fonts[1].family == UiFontFamily::Roboto);
  CHECK(fonts[2].family == UiFontFamily::Cousine);
}

TEST_CASE("UI scale choices expose auto and supported manual scales", "[ui][settings]")
{
  const auto& scales = settings::uiScaleChoices();
  REQUIRE(scales.size() == 12);

  CHECK(scales[0].label == std::string("Auto"));
  CHECK_FALSE(scales[0].scale.has_value());
  CHECK(scales[1].label == std::string("50%"));
  REQUIRE(scales[1].scale.has_value());
  CHECK(*scales[1].scale == 0.5f);
  CHECK(scales[3].label == std::string("100%"));
  REQUIRE(scales[3].scale.has_value());
  CHECK(*scales[3].scale == 1.0f);
  CHECK(scales.back().label == std::string("300%"));
  REQUIRE(scales.back().scale.has_value());
  CHECK(*scales.back().scale == 3.0f);
}

TEST_CASE("UI color and density presets stay in settings order", "[ui][settings]")
{
  const auto& colors = settings::uiColorPresets();
  REQUIRE(colors.size() == 9);
  CHECK(colors[0] == UiColorPreset::EntropyDark);
  CHECK(colors[1] == UiColorPreset::ImGuiDark);
  CHECK(colors[2] == UiColorPreset::ImGuiClassic);
  CHECK(colors[3] == UiColorPreset::ImGuiLight);
  CHECK(colors[4] == UiColorPreset::SlateBlue);
  CHECK(colors[5] == UiColorPreset::Graphite);
  CHECK(colors[6] == UiColorPreset::DeepTeal);
  CHECK(colors[7] == UiColorPreset::Midnight);
  CHECK(colors[8] == UiColorPreset::SoftLight);

  const auto& densities = settings::uiDensityPresets();
  REQUIRE(densities.size() == 3);
  CHECK(densities[0] == UiDensityPreset::Compact);
  CHECK(densities[1] == UiDensityPreset::Default);
  CHECK(densities[2] == UiDensityPreset::Comfortable);
}

TEST_CASE("settings pages stay in the intended navigation order", "[ui][settings]")
{
  const auto& pages = settings::settingsPageChoices();
  REQUIRE(pages.size() == 9);
  CHECK(pages[0].page == GuiData::SettingsTab::Views);
  CHECK(pages[0].label == std::string("Views"));
  CHECK(pages[1].page == GuiData::SettingsTab::Interface);
  CHECK(pages[1].label == std::string("Interface"));
  CHECK(pages[2].page == GuiData::SettingsTab::Images);
  CHECK(pages[3].page == GuiData::SettingsTab::Segmentation);
  CHECK(pages[4].page == GuiData::SettingsTab::Comparison);
  CHECK(pages[5].page == GuiData::SettingsTab::Rendering);
  CHECK(pages[6].page == GuiData::SettingsTab::Annotations);
  CHECK(pages[7].page == GuiData::SettingsTab::Synchronization);
  CHECK(pages[7].label == std::string("Synchronization"));
  CHECK(pages[8].page == GuiData::SettingsTab::System);
  CHECK(pages[8].label == std::string("System"));

  CHECK(settings::settingsPageLabel(GuiData::SettingsTab::Rendering) == std::string("Rendering"));
  CHECK(settings::settingsPageLabel(GuiData::SettingsTab::Synchronization) == std::string("Synchronization"));
}
