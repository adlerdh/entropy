#include "viewer/ViewTypes.h"

std::string to_string(const ViewType& type, bool crosshairsRotated)
{
  switch (type) {
    case ViewType::Axial:
      return crosshairsRotated ? "Z" : "Axial";
    case ViewType::Coronal:
      return crosshairsRotated ? "Y" : "Coronal";
    case ViewType::Sagittal:
      return crosshairsRotated ? "X" : "Sagittal";
    case ViewType::Oblique:
      return "Oblique";
    case ViewType::ThreeD:
      return "3D";
    case ViewType::NumElements:
      break;
  }

  return "Unknown";
}
