#pragma once

#include "ui/Scaling.h"

#include <imgui/imgui.h>
#include <imgui/misc/cpp/imgui_stdlib.h>

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <initializer_list>
#include <string>

namespace ui::popups
{
namespace fs = std::filesystem;

/**
 * @brief Scale a reference logical pixel value by the current UI scale
 * @param value Value measured against Entropy's reference font size
 * @return Scaled pixel value
 */
inline float scaledPixel(float value)
{
  return ui::scaledPixel(value);
}

/**
 * @brief Scale a reference logical size by the current UI scale
 * @param x Width measured against Entropy's reference font size
 * @param y Height measured against Entropy's reference font size
 * @return Scaled size
 */
inline ImVec2 scaledSize(float x, float y)
{
  return ui::scaledSize(x, y);
}

/**
 * @brief Scale a reference logical size and clamp it to the main viewport
 * @param width Reference width measured against Entropy's reference font size
 * @param height Reference height measured against Entropy's reference font size
 * @param maxViewportFraction Largest fraction of the main viewport work area allowed
 * @return Scaled and viewport-clamped size
 */
inline ImVec2 viewportClampedScaledSize(float width, float height, float maxViewportFraction = 0.9f)
{
  return ui::viewportClampedScaledSize(width, height, maxViewportFraction);
}

/**
 * @brief Format a filesystem path for display in a popup
 * @param path Path to format
 * @return Canonical or normalized display path, or "<none>" for an empty path
 * @throw Propagates exceptions from filesystem path or string operations
 */
inline std::string displayPath(fs::path path)
{
  if (path.empty()) {
    return "<none>";
  }

  std::error_code error;
  if (path.is_relative()) {
    const fs::path absolutePath = fs::absolute(path, error);
    if (!error) {
      path = absolutePath;
    }
  }

  error.clear();
  const fs::path canonicalPath = fs::weakly_canonical(path, error);
  if (!error) {
    return canonicalPath.string();
  }

  return path.lexically_normal().string();
}

/**
 * @brief Return the current working directory formatted for display
 * @return Current directory path, or "<unavailable>" when it cannot be read
 * @throw Propagates exceptions from path formatting
 */
inline std::string currentDirectory()
{
  std::error_code error;
  const fs::path path = fs::current_path(error);
  return error ? std::string{"<unavailable>"} : displayPath(path);
}

/**
 * @brief Render a read-only runtime path text field with a label to its right
 * @param label Visible label and ImGui ID seed
 * @param value Path text shown in the read-only input
 * @param inputWidth Width of the read-only input field
 */
inline void renderRuntimePathField(const char* label, const std::string& value, float inputWidth)
{
  ImGui::PushID(label);
  ImGui::SetNextItemWidth(inputWidth);
  ImGui::InputText("##value", const_cast<char*>(value.c_str()), value.size() + 1, ImGuiInputTextFlags_ReadOnly);
  ImGui::SameLine();
  ImGui::TextUnformatted(label);
  ImGui::PopID();
}

/**
 * @brief Compute a popup path input width that leaves room for the widest label
 * @param labels Labels displayed next to the path input field
 * @return Input width constrained by available content width and a minimum text width
 */
inline float runtimePathInputWidth(const std::initializer_list<const char*> labels)
{
  float labelWidth = 0.0f;
  for (const char* label : labels) {
    labelWidth = std::max(labelWidth, ImGui::CalcTextSize(label).x);
  }

  return std::max(
    ImGui::GetFontSize() * 12.0f,
    ImGui::GetContentRegionAvail().x - labelWidth - ImGui::GetStyle().ItemSpacing.x);
}

/**
 * @brief Trim leading and trailing ASCII whitespace
 * @param value String to trim
 * @return Trimmed copy of `value`
 * @throw Propagates exceptions from string allocation
 */
inline std::string trimWhitespace(const std::string& value)
{
  auto first = value.begin();
  while (first != value.end() && std::isspace(static_cast<unsigned char>(*first))) {
    ++first;
  }

  auto last = value.end();
  while (last != first && std::isspace(static_cast<unsigned char>(*(last - 1)))) {
    --last;
  }

  return std::string(first, last);
}

} // namespace ui::popups
