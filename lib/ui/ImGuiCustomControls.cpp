#include "ui/ImGuiCustomControls.h"
#include "ui/NativeFileDialogs.h"

#include <imgui/imgui_internal.h>

namespace
{

static const ImGuiDataTypeInfo GDataTypeInfo[] = {
  {sizeof(char), "S8", "%d", "%d"}, // ImGuiDataType_S8
  {sizeof(unsigned char), "U8", "%u", "%u"},
  {sizeof(short), "S16", "%d", "%d"}, // ImGuiDataType_S16
  {sizeof(unsigned short), "U16", "%u", "%u"},
  {sizeof(int), "S32", "%d", "%d"}, // ImGuiDataType_S32
  {sizeof(unsigned int), "U32", "%u", "%u"},
#ifdef _MSC_VER
  {sizeof(ImS64), "S64", "%I64d", "%I64d"}, // ImGuiDataType_S64
  {sizeof(ImU64), "U64", "%I64u", "%I64u"},
#else
  {sizeof(ImS64), "S64", "%lld", "%lld"}, // ImGuiDataType_S64
  {sizeof(ImU64), "U64", "%llu", "%llu"},
#endif
  {sizeof(float), "float", "%.3f", "%f"},  // ImGuiDataType_Float (float are promoted to double in va_arg)
  {sizeof(double), "double", "%f", "%lf"}, // ImGuiDataType_Double
};

} // namespace

namespace ImGui
{

std::optional<std::string> renderFileButtonDialogAndWindow(
  const char* buttonText,
  const char* dialogTitle,
  const std::vector<native_dialog::Filter>& dialogFilters)
{
  (void)dialogTitle;

  if (ImGui::Button(buttonText)) {
    if (const auto selectedFile = native_dialog::saveFile(dialogFilters)) {
      return selectedFile->string();
    }
  }

  return std::nullopt;
}

bool SliderScalarN_multiComp(
  const char* label,
  ImGuiDataType data_type,
  void* v,
  int components,
  const void** v_min,
  const void** v_max,
  const char** format,
  ImGuiSliderFlags flags)
{
  ImGuiWindow* window = GetCurrentWindow();
  if (window->SkipItems) {
    return false;
  }

  ImGuiContext& g = *GImGui;
  bool value_changed = false;
  BeginGroup();
  PushID(label);
  PushMultiItemsWidths(components, CalcItemWidth());
  size_t type_size = GDataTypeInfo[data_type].Size;
  for (int i = 0; i < components; i++) {
    PushID(i);
    if (i > 0) {
      SameLine(0, g.Style.ItemInnerSpacing.x);
    }
    value_changed |= SliderScalar("", data_type, v, v_min[i], v_max[i], format[i], flags);
    PopID();
    PopItemWidth();
    v = (void*)((char*)v + type_size);
  }
  PopID();

  const char* label_end = FindRenderedTextEnd(label);
  if (label != label_end) {
    SameLine(0, g.Style.ItemInnerSpacing.x);
    TextEx(label, label_end);
  }

  EndGroup();
  return value_changed;
}

} // namespace ImGui
