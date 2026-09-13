#include "common/AnatomicalLabels.h"

#include <catch2/catch_test_macros.hpp>

TEST_CASE("Automatic anatomical labels follow DICOM orientation", "[anatomical-labels]")
{
  DicomAnatomyInfo biped{.orientation = DicomAnatomicalOrientation::Biped};
  CHECK(
    resolveAnatomicalLabels(AnatomicalLabelType::Automatic, QuadrupedBodyRegion::Automatic, biped).type ==
    AnatomicalLabelType::Human);

  DicomAnatomyInfo quadruped{
    .orientation = DicomAnatomicalOrientation::Quadruped,
    .bodyRegion = QuadrupedBodyRegion::Head,
    .bodyRegionSource = DicomBodyRegionSource::AnatomicRegionSequence};
  const auto result =
    resolveAnatomicalLabels(AnatomicalLabelType::Automatic, QuadrupedBodyRegion::Automatic, quadruped);
  CHECK(result.type == AnatomicalLabelType::Quadruped);
  CHECK(result.quadrupedBodyRegion == QuadrupedBodyRegion::Head);
  CHECK_FALSE(result.quadrupedBodyRegionRequired);
}

TEST_CASE("Automatic anatomical labels use the DICOM default safely", "[anatomical-labels]")
{
  const auto nonDicom =
    resolveAnatomicalLabels(AnatomicalLabelType::Automatic, QuadrupedBodyRegion::Automatic, std::nullopt);
  CHECK(nonDicom.type == AnatomicalLabelType::Human);
  CHECK_FALSE(nonDicom.warning);

  DicomAnatomyInfo missingOrientation{.nonHumanSpecies = true};
  const auto missing =
    resolveAnatomicalLabels(AnatomicalLabelType::Automatic, QuadrupedBodyRegion::Automatic, missingOrientation);
  CHECK(missing.type == AnatomicalLabelType::Human);
  CHECK(missing.warning);
}

TEST_CASE("Quadruped labels require a known body region", "[anatomical-labels]")
{
  const auto unresolved =
    resolveAnatomicalLabels(AnatomicalLabelType::Quadruped, QuadrupedBodyRegion::Automatic, std::nullopt);
  CHECK(unresolved.quadrupedBodyRegionRequired);
  CHECK(unresolved.sourceText.find("Using the project selection") == std::string::npos);

  const auto unresolvedDirections = anatomicalDirectionAbbreviations(unresolved.type, unresolved.quadrupedBodyRegion);
  CHECK(std::string{unresolvedDirections[0]} == "L");
  CHECK(std::string{unresolvedDirections[1]} == "+y");
  CHECK(std::string{unresolvedDirections[2]} == "+z");
  CHECK(std::string{unresolvedDirections[3]} == "R");
  CHECK(std::string{unresolvedDirections[4]} == "-y");
  CHECK(std::string{unresolvedDirections[5]} == "-z");

  const auto distal =
    anatomicalDirectionAbbreviations(AnatomicalLabelType::Quadruped, QuadrupedBodyRegion::DistalForelimb);
  CHECK(std::string{distal[1]} == "Dor");
  CHECK(std::string{distal[4]} == "Pal");
  CHECK(std::string{distal[2]} == "Prox");
  CHECK(std::string{distal[5]} == "Dist");
}
