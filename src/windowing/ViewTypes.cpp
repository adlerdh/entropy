#include "windowing/ViewTypes.h"

#include <unordered_map>

std::string typeString(const ViewType& type, bool crosshairsRotated)
{
  // View type names when the crosshairs are NOT rotated:
  static const std::unordered_map<ViewType, std::string> s_typeToStringMap{
    {ViewType::Axial, "Axial"},
    {ViewType::Coronal, "Coronal"},
    {ViewType::Sagittal, "Sagittal"},
    {ViewType::ThreeD, "3D"},
    {ViewType::Oblique, "Oblique"}
  };

  // View type names when the crosshairs are ARE rotated:
  static const std::unordered_map<ViewType, std::string> s_rotatedTypeToStringMap{
    {ViewType::Axial, "Crosshairs Z"},
    {ViewType::Coronal, "Crosshairs Y"},
    {ViewType::Sagittal, "Crosshairs X"},
    {ViewType::ThreeD, "3D"},
    {ViewType::Oblique, "Oblique"}
  };

  return (crosshairsRotated ? s_rotatedTypeToStringMap.at(type) : s_typeToStringMap.at(type));
}
