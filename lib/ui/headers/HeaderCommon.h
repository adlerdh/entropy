#pragma once

#include <glm/glm.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/color_space.hpp>
#include <imgui/imgui.h>

#include <algorithm>
#include <cstddef>
#include <string>
#include <utility>

namespace entropy::ui::headers
{

inline const ImVec4 whiteText(1, 1, 1, 1);
inline const ImVec4 blackText(0, 0, 0, 1);

inline ImVec2 scaledToolbarButtonSize(const glm::vec2& contentScale)
{
  static const ImVec2 smallToolbarButtonSize(24, 24);
  (void)contentScale;
  const float scale = ImGui::GetFontSize() / 16.0f;
  return ImVec2{scale * smallToolbarButtonSize.x, scale * smallToolbarButtonSize.y};
}

inline const char* referenceAndActiveImageMessage = "This is the reference and active image";
inline const char* referenceImageMessage = "This is the reference image";
inline const char* activeImageMessage = "This is the active image";
inline const char* nonActiveImageMessage = "This is not the active image";

inline std::string imageRoleSuffix(bool isReferenceImage, bool isActiveImage, std::size_t numImages)
{
  if (numImages <= 1) {
    return isActiveImage ? " (active)" : std::string{};
  }
  if (isReferenceImage && isActiveImage) {
    return " (reference + active)";
  }
  if (isReferenceImage) {
    return " (reference)";
  }
  if (isActiveImage) {
    return " (active)";
  }
  return {};
}

inline std::string imageRoleSuffixShortReference(bool isReferenceImage, bool isActiveImage, std::size_t numImages)
{
  if (numImages <= 1) {
    return isActiveImage ? "(act.)" : std::string{};
  }
  if (isReferenceImage && isActiveImage) {
    return "(ref. + act.)";
  }
  if (isReferenceImage) {
    return "(ref.)";
  }
  if (isActiveImage) {
    return "(act.)";
  }
  return {};
}

inline std::string imageDisplayNameWithRole(
  const std::string& displayName,
  bool isReferenceImage,
  bool isActiveImage,
  std::size_t numImages)
{
  return displayName + imageRoleSuffix(isReferenceImage, isActiveImage, numImages);
}

inline std::string imageDisplayNameWithShortReferenceRole(
  const std::string& displayName,
  bool isReferenceImage,
  bool isActiveImage,
  std::size_t numImages)
{
  return displayName + imageRoleSuffixShortReference(isReferenceImage, isActiveImage, numImages);
}

inline const ImGuiColorEditFlags colorEditFlags = ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_PickerHueBar |
                                                  ImGuiColorEditFlags_DisplayRGB | ImGuiColorEditFlags_DisplayHSV |
                                                  ImGuiColorEditFlags_DisplayHex | ImGuiColorEditFlags_Uint8 |
                                                  ImGuiColorEditFlags_InputRGB;

inline std::pair<ImVec4, ImVec4> computeHeaderBgAndTextColors(const glm::vec3& color)
{
  glm::vec3 darkerBorderColorHsv = glm::hsvColor(color);
  darkerBorderColorHsv[2] = std::max(0.5f * darkerBorderColorHsv[2], 0.0f);
  const glm::vec3 darkerBorderColorRgb = glm::rgbColor(darkerBorderColorHsv);

  const ImVec4 headerColor(darkerBorderColorRgb.r, darkerBorderColorRgb.g, darkerBorderColorRgb.b, 1.0f);
  const ImVec4 headerTextColor = (glm::luminosity(darkerBorderColorRgb) < 0.75f) ? whiteText : blackText;

  return {headerColor, headerTextColor};
}

} // namespace entropy::ui::headers
