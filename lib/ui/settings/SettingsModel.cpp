#include "ui/settings/SettingsModel.h"

#include <algorithm>
#include <cmath>
#include <string>

namespace entropy::ui::settings
{

namespace
{
constexpr int sk_minTargetLengthPercent = 5;
constexpr int sk_maxTargetLengthPercent = 100;
constexpr int sk_targetLengthPercentStep = 5;

constexpr int sk_minMarginPx = 12;
constexpr int sk_maxMarginPx = 96;

constexpr std::uint32_t sk_minPrecision = 0;
constexpr std::uint32_t sk_maxPrecision = 9;

int roundToNearestStep(int value, int step)
{
  return step * static_cast<int>(std::round(static_cast<float>(value) / static_cast<float>(step)));
}
} // namespace

const char* scaleBarPositionName(ScaleBarPosition position)
{
  switch (position) {
    case ScaleBarPosition::BottomRight:
      return "Right bottom";
    case ScaleBarPosition::BottomLeft:
      return "Left bottom";
    case ScaleBarPosition::TopRight:
      return "Right top";
    case ScaleBarPosition::TopLeft:
      return "Left top";
    case ScaleBarPosition::Bottom:
      return "Bottom";
    case ScaleBarPosition::Top:
      return "Top";
    case ScaleBarPosition::Left:
      return "Left";
    case ScaleBarPosition::Right:
      return "Right";
  }

  return "Right bottom";
}

std::optional<ScaleBarOrientation> forcedScaleBarOrientation(ScaleBarPosition position)
{
  switch (position) {
    case ScaleBarPosition::Bottom:
    case ScaleBarPosition::Top:
      return ScaleBarOrientation::Horizontal;
    case ScaleBarPosition::Left:
    case ScaleBarPosition::Right:
      return ScaleBarOrientation::Vertical;
    case ScaleBarPosition::BottomRight:
    case ScaleBarPosition::BottomLeft:
    case ScaleBarPosition::TopRight:
    case ScaleBarPosition::TopLeft:
      return std::nullopt;
  }

  return std::nullopt;
}

ScaleBarOrientation normalizedScaleBarOrientation(ScaleBarPosition position, ScaleBarOrientation currentOrientation)
{
  return forcedScaleBarOrientation(position).value_or(currentOrientation);
}

bool canChooseScaleBarOrientation(ScaleBarPosition position)
{
  return !forcedScaleBarOrientation(position).has_value();
}

const std::array<ScaleBarPosition, 8>& orderedScaleBarPositions()
{
  static constexpr std::array<ScaleBarPosition, 8> sk_positions{
    ScaleBarPosition::BottomRight,
    ScaleBarPosition::Right,
    ScaleBarPosition::TopRight,
    ScaleBarPosition::Top,
    ScaleBarPosition::TopLeft,
    ScaleBarPosition::Left,
    ScaleBarPosition::BottomLeft,
    ScaleBarPosition::Bottom};

  return sk_positions;
}

const std::array<ScaleBarPositionButton, 8>& scaleBarPositionButtons()
{
  static constexpr std::array<ScaleBarPositionButton, 8> sk_buttons{
    ScaleBarPositionButton{ScaleBarPosition::TopLeft, "LT"},
    ScaleBarPositionButton{ScaleBarPosition::Top, "T"},
    ScaleBarPositionButton{ScaleBarPosition::TopRight, "RT"},
    ScaleBarPositionButton{ScaleBarPosition::Left, "L"},
    ScaleBarPositionButton{ScaleBarPosition::Right, "R"},
    ScaleBarPositionButton{ScaleBarPosition::BottomLeft, "LB"},
    ScaleBarPositionButton{ScaleBarPosition::Bottom, "B"},
    ScaleBarPositionButton{ScaleBarPosition::BottomRight, "RB"}};

  return sk_buttons;
}

int targetLengthPercentFromFraction(float fraction)
{
  const int rounded = roundToNearestStep(static_cast<int>(std::round(100.0f * fraction)), sk_targetLengthPercentStep);

  return std::clamp(rounded, sk_minTargetLengthPercent, sk_maxTargetLengthPercent);
}

float targetLengthFractionFromPercent(int percent)
{
  const int rounded = roundToNearestStep(percent, sk_targetLengthPercentStep);
  return static_cast<float>(std::clamp(rounded, sk_minTargetLengthPercent, sk_maxTargetLengthPercent)) / 100.0f;
}

int marginPixelsFromFloat(float marginPx)
{
  return std::clamp(static_cast<int>(std::round(marginPx)), sk_minMarginPx, sk_maxMarginPx);
}

float marginFloatFromPixels(int marginPx)
{
  return static_cast<float>(std::clamp(marginPx, sk_minMarginPx, sk_maxMarginPx));
}

std::uint32_t clampPrecision(std::uint32_t precision)
{
  return std::clamp(precision, sk_minPrecision, sk_maxPrecision);
}

std::string precisionFormat(std::uint32_t precision)
{
  return std::string("%0.") + std::to_string(clampPrecision(precision)) + std::string("f");
}

const std::array<ScaleChoice, 12>& uiScaleChoices()
{
  static const std::array<ScaleChoice, 12> sk_choices{
    ScaleChoice{"Auto", std::nullopt},
    ScaleChoice{"50%", 0.5f},
    ScaleChoice{"75%", 0.75f},
    ScaleChoice{"100%", 1.0f},
    ScaleChoice{"125%", 1.25f},
    ScaleChoice{"150%", 1.5f},
    ScaleChoice{"175%", 1.75f},
    ScaleChoice{"200%", 2.0f},
    ScaleChoice{"225%", 2.25f},
    ScaleChoice{"250%", 2.5f},
    ScaleChoice{"275%", 2.75f},
    ScaleChoice{"300%", 3.0f}};

  return sk_choices;
}

const std::array<FontChoice, 3>& visibleFontChoices()
{
  static constexpr std::array<FontChoice, 3> sk_choices{
    FontChoice{"Inter", UiFontFamily::Inter},
    FontChoice{"Roboto", UiFontFamily::Roboto},
    FontChoice{"Cousine", UiFontFamily::Cousine}};

  return sk_choices;
}

const std::array<UiColorPreset, 9>& uiColorPresets()
{
  static constexpr std::array<UiColorPreset, 9> sk_presets{
    UiColorPreset::EntropyDark,
    UiColorPreset::ImGuiDark,
    UiColorPreset::ImGuiClassic,
    UiColorPreset::ImGuiLight,
    UiColorPreset::SlateBlue,
    UiColorPreset::Graphite,
    UiColorPreset::DeepTeal,
    UiColorPreset::Midnight,
    UiColorPreset::SoftLight};

  return sk_presets;
}

const std::array<UiDensityPreset, 3>& uiDensityPresets()
{
  static constexpr std::array<UiDensityPreset, 3> sk_presets{
    UiDensityPreset::Compact,
    UiDensityPreset::Default,
    UiDensityPreset::Comfortable};

  return sk_presets;
}

const std::array<SettingsPageChoice, 10>& settingsPageChoices()
{
  static constexpr std::array<SettingsPageChoice, 10> sk_choices{
    SettingsPageChoice{GuiData::SettingsTab::Views, "Views"},
    SettingsPageChoice{GuiData::SettingsTab::Interface, "Interface"},
    SettingsPageChoice{GuiData::SettingsTab::Images, "Images"},
    SettingsPageChoice{GuiData::SettingsTab::Segmentation, "Segmentation"},
    SettingsPageChoice{GuiData::SettingsTab::Registration, "Registration"},
    SettingsPageChoice{GuiData::SettingsTab::Comparison, "Comparison"},
    SettingsPageChoice{GuiData::SettingsTab::Annotations, "Annotation"},
    SettingsPageChoice{GuiData::SettingsTab::Synchronization, "Synchronization"},
    SettingsPageChoice{GuiData::SettingsTab::Rendering, "Rendering"},
    SettingsPageChoice{GuiData::SettingsTab::System, "System"}};

  return sk_choices;
}

const char* settingsPageLabel(GuiData::SettingsTab page)
{
  for (const SettingsPageChoice& choice : settingsPageChoices()) {
    if (choice.page == page) {
      return choice.label;
    }
  }

  return "Views";
}

} // namespace entropy::ui::settings
