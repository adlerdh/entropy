#include "ui/Helpers.h"

#include <IconsForkAwesome.h>

#include <imgui/imgui.h>

#include <algorithm>
#include <stdio.h>

void helpMarker(const char* tooltip, bool sameLine)
{
  if (sameLine) {
    ImGui::SameLine();
  }

  ImGui::TextDisabled(ICON_FK_QUESTION_CIRCLE_O);

  if (ImGui::IsItemHovered()) {
    ImGui::BeginTooltip();
    ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
    ImGui::TextUnformatted(tooltip);
    ImGui::PopTextWrapPos();
    ImGui::EndTooltip();
  }
}

ImVec2 constrainedWindowMaxSize(float viewportFraction, float fallbackWidth, float fallbackHeight)
{
  const ImGuiViewport* viewport = ImGui::GetMainViewport();
  if (viewport == nullptr) {
    return ImVec2{fallbackWidth, fallbackHeight};
  }

  return ImVec2{viewport->WorkSize.x * viewportFraction, viewport->WorkSize.y * viewportFraction};
}

void setNextWindowSizeConstraintsToMainViewport(
  float minWidth,
  float minHeight,
  float viewportFraction,
  float fallbackMaxWidth,
  float fallbackMaxHeight)
{
  const ImVec2 minSize{minWidth, minHeight};
  ImVec2 maxSize = constrainedWindowMaxSize(viewportFraction, fallbackMaxWidth, fallbackMaxHeight);
  maxSize.x = std::max(maxSize.x, minSize.x);
  maxSize.y = std::max(maxSize.y, minSize.y);
  ImGui::SetNextWindowSizeConstraints(minSize, maxSize);
}

bool mySliderS32(const char* label, int32_t* value, int32_t min, int32_t max, const char* format)
{
  return ImGui::SliderScalar(label, ImGuiDataType_S32, value, &min, &max, format);
}

bool mySliderS64(const char* label, int64_t* value, int64_t min, int64_t max, const char* format)
{
  return ImGui::SliderScalar(label, ImGuiDataType_S64, value, &min, &max, format);
}

bool mySliderF32(const char* label, float* value, float min, float max, const char* format)
{
  return ImGui::SliderScalar(label, ImGuiDataType_Float, value, &min, &max, format);
}

bool mySliderF64(const char* label, double* value, double min, double max, const char* format)
{
  return ImGui::SliderScalar(label, ImGuiDataType_Double, value, &min, &max, format);
}

int myImFormatString(char* buf, size_t buf_size, const char* fmt, ...)
{
  va_list args;
  va_start(args, fmt);
  int w = vsnprintf(buf, buf_size, fmt, args);
  va_end(args);

  if (!buf) {
    return w;
  }

  if (-1 == w || (int)buf_size <= w) {
    w = static_cast<int>(buf_size) - 1;
  }

  buf[w] = 0;
  return w;
}
