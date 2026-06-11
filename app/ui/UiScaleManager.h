#pragma once

#include <imgui/imgui.h>

#include <functional>
#include <optional>

class ImFont;

class UiScaleManager
{
public:
  using FontReloadCallback = std::function<void(float scale)>;

  /**
   * @brief Capture the unscaled ImGui style used as the baseline for scaling.
   *
   * Every scale application resets ImGui's style to this baseline before
   * calling ImGuiStyle::ScaleAllSizes(), preventing cumulative scaling error.
   */
  void captureBaseStyle(const ImGuiStyle& style);

  /**
   * @brief Set the callback used to repopulate the ImGui font atlas.
   */
  void setFontReloadCallback(FontReloadCallback callback);

  /**
   * @brief Set or clear the user-selected UI scale override.
   *
   * A value of \c std::nullopt restores platform auto scaling.
   */
  void setUserScaleOverride(std::optional<float> scale);

  /**
   * @brief Return the raw platform content scale last reported by the window.
   */
  float contentScale() const;

  /**
   * @brief Return the UI scale currently applied to ImGui.
   *
   * This is either the user override or the platform-specific auto scale.
   */
  float effectiveScale() const;

  /**
   * @brief Return the current user-selected scale override, if any.
   */
  std::optional<float> userScaleOverride() const;

  /**
   * @brief Apply a platform content scale update.
   *
   * @return True when the effective ImGui scale changed and fonts/style were rebuilt.
   */
  bool applyContentScale(float scale);

  /**
   * @brief Rebuild the ImGui font atlas for a specific UI scale.
   */
  void rebuildFonts(float scale);

  /**
   * @brief Rebuild the ImGui font atlas for the current effective UI scale.
   */
  void rebuildFontsForCurrentScale();

private:
  /**
   * @brief Clamp invalid or unsupported UI scales into Entropy's supported range.
   */
  static float sanitizeScale(float scale);

  /**
   * @brief Convert a platform content scale to Entropy's automatic UI scale.
   *
   * On macOS, Retina backing scale should keep rendering sharp without doubling
   * the logical ImGui interface size. Other platforms use the GLFW content scale
   * so OS UI scaling remains reflected in Auto mode.
   */
  static float platformUiScale(float contentScale);

  /**
   * @brief Apply the current effective scale after a potential scale change.
   */
  bool applyEffectiveScale(float previousScale);

  ImGuiStyle m_baseStyle;
  float m_contentScale = 0.0f;
  bool m_hasAppliedScale = false;
  std::optional<float> m_userScaleOverride;
  FontReloadCallback m_fontReloadCallback;
};
