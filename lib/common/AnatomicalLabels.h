#pragma once

#include "common/Types.h"

#include <array>
#include <optional>
#include <string>

/// Effective anatomical-label convention and its provenance.
struct AnatomicalLabelResolution
{
  AnatomicalLabelType type = AnatomicalLabelType::Human;
  std::optional<QuadrupedBodyRegion> quadrupedBodyRegion = std::nullopt;
  std::string sourceText;
  bool warning = false;
  bool quadrupedBodyRegionRequired = false;
};

/// Resolve project choices against metadata from the reference DICOM series.
AnatomicalLabelResolution resolveAnatomicalLabels(
  AnatomicalLabelType requestedType,
  QuadrupedBodyRegion requestedBodyRegion,
  const std::optional<DicomAnatomyInfo>& dicomAnatomy);

/// Return positive X/Y/Z followed by negative X/Y/Z direction abbreviations.
std::array<const char*, 6> anatomicalDirectionAbbreviations(
  AnatomicalLabelType type,
  const std::optional<QuadrupedBodyRegion>& quadrupedBodyRegion = std::nullopt);

/// Return a user-facing name for a quadruped body region.
const char* quadrupedBodyRegionName(QuadrupedBodyRegion region);
