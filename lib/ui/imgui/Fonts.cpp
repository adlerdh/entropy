#define IMGUI_DEFINE_MATH_OPERATORS
#include "ui/ImGuiWrapper.h"

#include "logic/app/Data.h"
#include "ui/Helpers.h"
#include "ui/ImGuiCustomControls.h"
#include "ui/Style.h"

#include <cmrc/cmrc.hpp>

#include <IconsForkAwesome.h>
#include <imgui/imgui_internal.h>

#include <spdlog/fmt/std.h>
#include <spdlog/spdlog.h>

#include <functional>
#include <utility>

CMRC_DECLARE(fonts);

namespace
{
ImFont* loadFont(
  const std::string& fontPath,
  const ImFontConfig& fontConfig,
  float fontSize,
  const ImWchar* glyphRange = nullptr)
{
  auto filesystem = cmrc::fonts::get_filesystem();
  cmrc::file fontFile = filesystem.open(fontPath);

  auto fontData = std::make_unique<char[]>(fontFile.size());
  std::copy(fontFile.cbegin(), fontFile.cend(), fontData.get());

  ImFontConfig config = fontConfig;
  config.FontDataOwnedByAtlas = true;

  ImFont* font = ImGui::GetIO().Fonts->AddFontFromMemoryTTF(
    static_cast<void*>(fontData.get()),
    static_cast<int32_t>(fontFile.size()),
    fontSize,
    &config,
    glyphRange);
  if (font) {
    fontData.release();
  }

  return font;
}

struct UiFontSpec
{
  const char* name;
  const char* path;
  float size;
};

/**
 * @brief Return the embedded font resource and base size for an ImGui UI font family.
 */
UiFontSpec uiFontSpec(UiFontFamily family)
{
  switch (family) {
    case UiFontFamily::SpaceGrotesk:
      return {"Space Grotesk Light", "res/fonts/SpaceGrotesk/SpaceGrotesk-Light.ttf", 16.0f};
    case UiFontFamily::Inter:
      return {"Inter Regular", "res/fonts/Inter/Inter-Regular.ttf", 16.0f};
    case UiFontFamily::NotoSans:
      return {"Noto Sans Regular", "res/fonts/NotoSans/NotoSans-Regular.ttf", 16.0f};
    case UiFontFamily::Roboto:
      return {"Roboto Regular", "res/fonts/Roboto/Roboto-Regular.ttf", 16.0f};
    case UiFontFamily::SourceSans3:
      return {"Source Sans 3 Regular", "res/fonts/SourceSans3/SourceSans3-Regular.ttf", 16.0f};
    case UiFontFamily::IBMPlexSans:
      return {"IBM Plex Sans Regular", "res/fonts/IBMPlexSans/IBMPlexSans-Regular.ttf", 16.0f};
    case UiFontFamily::Cousine:
      return {"Cousine Regular", "res/fonts/Cousine/Cousine-Regular.ttf", 14.0f};
  }

  return {"Space Grotesk Light", "res/fonts/SpaceGrotesk/SpaceGrotesk-Light.ttf", 16.0f};
}

} // namespace

// Apply DPI scaling to the captured base style, rather than scaling an already-scaled style.
void ImGuiWrapper::setContentScale(float scale)
{
  m_uiScaleManager.applyContentScale(scale);
  m_appData.guiData().m_effectiveUiScale = m_uiScaleManager.effectiveScale();
}

void ImGuiWrapper::setUserScaleOverride(std::optional<float> scale)
{
  m_pendingUserScaleOverride = scale;
  if (m_postEmptyGlfwEvent) {
    m_postEmptyGlfwEvent();
  }
}

void ImGuiWrapper::requestFontReload()
{
  m_pendingFontReload = true;
  if (m_postEmptyGlfwEvent) {
    m_postEmptyGlfwEvent();
  }
}

void ImGuiWrapper::applyUiColorPreset(UiColorPreset preset)
{
  m_appData.settings().setUiColorPreset(preset);
  const UiDensityPreset densityPreset = m_appData.settings().uiDensityPreset();
  const float windowBgOpacity = m_appData.settings().uiWindowBgOpacity();
  m_uiScaleManager.updateBaseStyle([preset, densityPreset, windowBgOpacity](ImGuiStyle& style) {
    applyUiStylePreset(preset, &style);
    ::applyUiDensityPreset(densityPreset, &style);
    ::applyUiWindowBgOpacity(windowBgOpacity, &style);
  });
}

void ImGuiWrapper::applyUiDensityPreset(UiDensityPreset preset)
{
  m_appData.settings().setUiDensityPreset(preset);
  m_uiScaleManager.updateBaseStyle([preset](ImGuiStyle& style) { ::applyUiDensityPreset(preset, &style); });
}

void ImGuiWrapper::applyUiWindowBgOpacity(float opacity)
{
  m_appData.settings().setUiWindowBgOpacity(opacity);
  const float clampedOpacity = m_appData.settings().uiWindowBgOpacity();
  m_uiScaleManager.updateBaseStyle(
    [clampedOpacity](ImGuiStyle& style) { ::applyUiWindowBgOpacity(clampedOpacity, &style); });
}

void ImGuiWrapper::initializeFonts(float scale)
{
  static const std::string forkAwesomeFontPath = std::string("res/fonts/ForkAwesome/") + FONT_ICON_FILE_NAME_FK;
  const char* const interBoldFontPath = "res/fonts/Inter/Inter-Bold.ttf";
  const UiFontSpec uiFont = uiFontSpec(m_appData.settings().uiFontFamily());

  spdlog::debug("Begin loading fonts for UI scale {}", scale);

  ImFontConfig uiFontConfig;

  myImFormatString(uiFontConfig.Name, IM_ARRAYSIZE(uiFontConfig.Name), "%s, %.0fpx", uiFont.name, uiFont.size);

  // Merge in icons from Fork Awesome:
  ImFontConfig forkAwesomeFontConfig;
  forkAwesomeFontConfig.MergeMode = true;
  forkAwesomeFontConfig.PixelSnapH = true;

  const float forkAwesomeFontSize = 14.0f;

  myImFormatString(
    forkAwesomeFontConfig.Name,
    IM_ARRAYSIZE(forkAwesomeFontConfig.Name),
    "%s, %.0fpx",
    "Fork Awesome",
    forkAwesomeFontSize);

  /// @see For details about Fork Awesome fonts: https://forkaweso.me/Fork-Awesome/icons/
  static const ImWchar forkAwesomeIconGlyphRange[] = {ICON_MIN_FK, ICON_MAX_FK, 0};

  // Load fonts: If no fonts are loaded, dear imgui will use the default font.
  // You can also load multiple fonts and use ImGui::PushFont()/PopFont() to select them.
  // AddFontFromFileTTF() will return the ImFont* so you can store it if you need to select the
  // font among multiple. If the file cannot be loaded, the function will return NULL.
  // Please handle those errors in your application (e.g. use an assertion, or display an error and
  // quit). With ImGui 1.92 dynamic textures, glyphs are baked and uploaded by the backend
  // as needed during the frame/render path.
  /// @todo use Freetype Rasterizer and Small Font Sizes

  m_appData.guiData().m_fonts.clear();

  ImFontConfig interBoldFontConfig;
  myImFormatString(
    interBoldFontConfig.Name,
    IM_ARRAYSIZE(interBoldFontConfig.Name),
    "%s, %.0fpx",
    "Inter Bold",
    uiFont.size);

  ImFont* uiFontPtr = loadFont(uiFont.path, uiFontConfig, uiFont.size, nullptr);
  ImFont* fork1Ptr =
    loadFont(forkAwesomeFontPath, forkAwesomeFontConfig, forkAwesomeFontSize, forkAwesomeIconGlyphRange);
  ImFont* interBoldFontPtr = loadFont(interBoldFontPath, interBoldFontConfig, uiFont.size, nullptr);
  const ImFont* boldForkPtr =
    loadFont(forkAwesomeFontPath, forkAwesomeFontConfig, forkAwesomeFontSize, forkAwesomeIconGlyphRange);

  if (uiFontPtr && fork1Ptr) {
    m_appData.guiData().m_fonts[uiFont.path] = uiFontPtr;
    if (interBoldFontPtr && boldForkPtr) {
      m_appData.guiData().m_fonts[interBoldFontPath] = interBoldFontPtr;
    }
    else {
      spdlog::error("Unable to load font {} or {}", interBoldFontPath, forkAwesomeFontPath);
    }
    m_appData.guiData().m_fonts[std::string(uiFont.path) + forkAwesomeFontPath] = fork1Ptr;
    ImGui::GetIO().FontDefault = uiFontPtr;
    spdlog::debug("Loaded font {}", uiFont.path);
  }
  else {
    spdlog::error("Unable to load font {} or {}", uiFont.path, forkAwesomeFontPath);
  }

  spdlog::debug("Done loading fonts");
}
