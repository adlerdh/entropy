#include "layout/LayoutPresetInfo.h"

namespace layout
{

std::string_view layoutPresetViewName(ViewType viewType)
{
  switch (viewType) {
    case ViewType::Axial:
      return "axial";
    case ViewType::Coronal:
      return "coronal";
    case ViewType::Sagittal:
      return "sagittal";
    case ViewType::Oblique:
      return "oblique";
    case ViewType::ThreeD:
      return "ThreeD";
    case ViewType::NumElements:
      break;
  }
  return "axial";
}

std::optional<ViewType> layoutPresetViewType(std::string_view name)
{
  if (name.empty() || "Axial" == name || "axial" == name) {
    return ViewType::Axial;
  }
  if ("Coronal" == name || "coronal" == name) {
    return ViewType::Coronal;
  }
  if ("Sagittal" == name || "sagittal" == name) {
    return ViewType::Sagittal;
  }
  if ("Oblique" == name || "oblique" == name) {
    return ViewType::Oblique;
  }
  if ("ThreeD" == name || "3D" == name || "threeD" == name || "three-d" == name) {
    return ViewType::ThreeD;
  }
  return std::nullopt;
}

std::vector<std::size_t> presetImageIndicesOrDefault(const LayoutPreset& preset)
{
  return preset.m_imageIndices.empty() ? std::vector<std::size_t>{0} : preset.m_imageIndices;
}

} // namespace layout
