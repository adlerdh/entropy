#include "common/AnatomicalLabels.h"

namespace
{
std::optional<QuadrupedBodyRegion> resolvedBodyRegion(
  const QuadrupedBodyRegion requested,
  const std::optional<DicomAnatomyInfo>& dicomAnatomy)
{
  if (QuadrupedBodyRegion::Automatic != requested) {
    return requested;
  }
  return dicomAnatomy ? dicomAnatomy->bodyRegion : std::nullopt;
}
} // namespace

AnatomicalLabelResolution resolveAnatomicalLabels(
  const AnatomicalLabelType requestedType,
  const QuadrupedBodyRegion requestedBodyRegion,
  const std::optional<DicomAnatomyInfo>& dicomAnatomy)
{
  AnatomicalLabelResolution result;

  if (AnatomicalLabelType::Automatic != requestedType) {
    result.type = requestedType;
  }
  else if (!dicomAnatomy) {
    result.type = AnatomicalLabelType::Human;
    result.sourceText = "Resolved to Human because the reference image is not a DICOM series.";
  }
  else {
    switch (dicomAnatomy->orientation) {
      case DicomAnatomicalOrientation::Biped:
        result.type = AnatomicalLabelType::Human;
        result.sourceText = "Resolved to Human from DICOM Anatomical Orientation Type: BIPED.";
        break;
      case DicomAnatomicalOrientation::Quadruped:
        result.type = AnatomicalLabelType::Quadruped;
        result.sourceText = "Resolved to Quadruped from DICOM Anatomical Orientation Type: QUADRUPED.";
        break;
      case DicomAnatomicalOrientation::Unspecified:
        result.type = AnatomicalLabelType::Human;
        result.sourceText = "Resolved to Human because DICOM Anatomical Orientation Type is absent.";
        result.warning = dicomAnatomy->nonHumanSpecies;
        if (result.warning) {
          result.sourceText += " The DICOM species appears non-human, so verify this convention manually.";
        }
        break;
    }
  }

  if (AnatomicalLabelType::Quadruped == result.type) {
    result.quadrupedBodyRegion = resolvedBodyRegion(requestedBodyRegion, dicomAnatomy);
    result.quadrupedBodyRegionRequired = !result.quadrupedBodyRegion.has_value();
    if (result.quadrupedBodyRegion) {
      if (!result.sourceText.empty()) {
        result.sourceText += ' ';
      }
      result.sourceText += "Body region: ";
      result.sourceText += quadrupedBodyRegionName(*result.quadrupedBodyRegion);
      if (QuadrupedBodyRegion::Automatic == requestedBodyRegion && dicomAnatomy) {
        result.sourceText += dicomAnatomy->bodyRegionSource == DicomBodyRegionSource::AnatomicRegionSequence
                               ? " (DICOM Anatomic Region Sequence)."
                               : " (DICOM Body Part Examined).";
      }
      else {
        result.sourceText += ".";
      }
    }
    else {
      result.warning = true;
      if (!result.sourceText.empty()) {
        result.sourceText += ' ';
      }
      result.sourceText +=
        "Choose a quadruped body region because the DICOM metadata does not identify one. "
        "Unresolved axes use Cartesian labels.";
    }
  }

  return result;
}

std::array<const char*, 6> anatomicalDirectionAbbreviations(
  const AnatomicalLabelType type,
  const std::optional<QuadrupedBodyRegion>& quadrupedBodyRegion)
{
  switch (type) {
    case AnatomicalLabelType::Cartesian:
      return {"+x", "+y", "+z", "-x", "-y", "-z"};
    case AnatomicalLabelType::Rodent:
      return {"L", "Dor", "Ros", "R", "Ven", "Cau"};
    case AnatomicalLabelType::Quadruped:
      if (!quadrupedBodyRegion) {
        return {"L", "+y", "+z", "R", "-y", "-z"};
      }
      switch (*quadrupedBodyRegion) {
        case QuadrupedBodyRegion::Head:
          return {"L", "Dor", "Ros", "R", "Ven", "Cau"};
        case QuadrupedBodyRegion::NeckTrunkTail:
          return {"L", "Dor", "Cra", "R", "Ven", "Cau"};
        case QuadrupedBodyRegion::ProximalLimb:
          return {"L", "Cra", "Prox", "R", "Cau", "Dist"};
        case QuadrupedBodyRegion::DistalForelimb:
          return {"L", "Dor", "Prox", "R", "Pal", "Dist"};
        case QuadrupedBodyRegion::DistalHindlimb:
          return {"L", "Dor", "Prox", "R", "Pla", "Dist"};
        case QuadrupedBodyRegion::Automatic:
          break;
      }
      return {"L", "+y", "+z", "R", "-y", "-z"};
    case AnatomicalLabelType::Automatic:
    case AnatomicalLabelType::Human:
      return {"L", "P", "S", "R", "A", "I"};
    case AnatomicalLabelType::Disabled:
      return {"", "", "", "", "", ""};
  }
  return {"", "", "", "", "", ""};
}

const char* quadrupedBodyRegionName(const QuadrupedBodyRegion region)
{
  switch (region) {
    case QuadrupedBodyRegion::Automatic:
      return "Automatic from DICOM";
    case QuadrupedBodyRegion::Head:
      return "Head";
    case QuadrupedBodyRegion::NeckTrunkTail:
      return "Neck, trunk, or tail";
    case QuadrupedBodyRegion::ProximalLimb:
      return "Proximal limb";
    case QuadrupedBodyRegion::DistalForelimb:
      return "Distal forelimb";
    case QuadrupedBodyRegion::DistalHindlimb:
      return "Distal hindlimb";
  }
  return "Unknown";
}
