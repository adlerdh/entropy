#pragma once

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

inline float scaledPixel(float value)
{
  return value * (ImGui::GetFontSize() / 16.0f);
}

inline ImVec2 scaledSize(float x, float y)
{
  return ImVec2(scaledPixel(x), scaledPixel(y));
}

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

inline std::string currentDirectory()
{
  std::error_code error;
  const fs::path path = fs::current_path(error);
  return error ? std::string{"<unavailable>"} : displayPath(path);
}

inline void renderRuntimePathField(const char* label, const std::string& value, float inputWidth)
{
  ImGui::PushID(label);
  ImGui::SetNextItemWidth(inputWidth);
  ImGui::InputText("##value", const_cast<char*>(value.c_str()), value.size() + 1, ImGuiInputTextFlags_ReadOnly);
  ImGui::SameLine();
  ImGui::TextUnformatted(label);
  ImGui::PopID();
}

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
