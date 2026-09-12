#include "viewer/ViewTypes.h"

#include "common/Types.h"

std::string viewTypeDisplayName(
  const ViewType viewType,
  const AnatomicalLabelType anatomicalLabelType,
  const bool crosshairsRotated)
{
  if (crosshairsRotated) {
    switch (viewType) {
      case ViewType::Axial:
        return "Z\xE2\x80\xB2"; // Z followed by the UTF-8 prime symbol
      case ViewType::Coronal:
        return "Y\xE2\x80\xB2";
      case ViewType::Sagittal:
        return "X\xE2\x80\xB2";
      case ViewType::Oblique:
      case ViewType::ThreeD:
      case ViewType::NumElements:
        break;
    }
  }

  switch (viewType) {
    case ViewType::Axial:
      switch (anatomicalLabelType) {
        case AnatomicalLabelType::Rodent:
          return "Coronal";
        case AnatomicalLabelType::Quadruped:
          return "Transverse";
        case AnatomicalLabelType::Cartesian:
          return "Z";
        case AnatomicalLabelType::Automatic:
        case AnatomicalLabelType::Human:
        case AnatomicalLabelType::Disabled:
          return "Axial";
      }
      break;
    case ViewType::Coronal:
      switch (anatomicalLabelType) {
        case AnatomicalLabelType::Rodent:
          return "Horizontal";
        case AnatomicalLabelType::Quadruped:
          return "Dorsal";
        case AnatomicalLabelType::Cartesian:
          return "Y";
        case AnatomicalLabelType::Automatic:
        case AnatomicalLabelType::Human:
        case AnatomicalLabelType::Disabled:
          return "Coronal";
      }
      break;
    case ViewType::Sagittal:
      return AnatomicalLabelType::Cartesian == anatomicalLabelType ? "X" : "Sagittal";
    case ViewType::Oblique:
      return "Oblique";
    case ViewType::ThreeD:
      return "3D";
    case ViewType::NumElements:
      break;
  }

  return "Unknown";
}
