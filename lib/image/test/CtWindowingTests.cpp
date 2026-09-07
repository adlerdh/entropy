#include "image/CtWindowing.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <string>

namespace
{
ct_windowing::DetectionInput scalarInput(const MetaDataMap& metadata)
{
  return ct_windowing::DetectionInput{
    .metadata = &metadata,
    .pixelType = PixelType::Scalar,
    .componentType = ComponentType::Int16,
    .numComponents = 1u};
}
} // namespace

TEST_CASE("CT detection gives DICOM modality and SOP class authoritative priority", "[image][ct][metadata]")
{
  MetaDataMap metadata{{"0008|0060", std::string{"CT"}}, {"0018|1030", std::string{"Head CTA arterial"}}};
  const auto modalityDetection = ct_windowing::detect(scalarInput(metadata));
  CHECK(modalityDetection.confidence == ct_windowing::DetectionConfidence::Confirmed);
  CHECK(modalityDetection.angiography);

  metadata = {{"0008|0016", std::string{"1.2.840.10008.5.1.4.1.1.2.1"}}};
  CHECK(ct_windowing::detect(scalarInput(metadata)).confidence == ct_windowing::DetectionConfidence::Confirmed);
}

TEST_CASE("CT detection avoids substring and component-type false positives", "[image][ct][metadata]")
{
  MetaDataMap metadata{{"series_description", std::string{"Corrected structural MRI"}}};
  auto input = scalarInput(metadata);
  input.valueRange = std::pair<double, double>{0.0, 4095.0};
  CHECK_FALSE(ct_windowing::detect(input).isCt());

  metadata = {{"series_description", std::string{"Noncontrast CT head"}}};
  CHECK(ct_windowing::detect(scalarInput(metadata)).confidence == ct_windowing::DetectionConfidence::Likely);

  const MetaDataMap emptyMetadata;
  input = scalarInput(emptyMetadata);
  input.valueRange = std::pair<double, double>{-1024.0, 3071.0};
  CHECK(ct_windowing::detect(input).confidence == ct_windowing::DetectionConfidence::Likely);

  metadata = {{"0008|0060", std::string{"MR"}}, {"series_description", std::string{"CT-like reconstruction"}}};
  input = scalarInput(metadata);
  input.valueRange = std::pair<double, double>{-1024.0, 3071.0};
  CHECK_FALSE(ct_windowing::detect(input).isCt());
}

TEST_CASE("DICOM CT window presets preserve paired values and explanations", "[image][ct][metadata]")
{
  const MetaDataMap metadata{
    {"0028|1050", std::string{"40\\-600\\40"}},
    {"0028|1051", std::string{"80\\1500\\80"}},
    {"0028|1055", std::string{"Brain\\Lung\\Duplicate brain"}}};

  const auto presets = ct_windowing::dicomPresets(metadata);
  REQUIRE(presets.size() == 2u);
  CHECK(presets[0].name == "Brain");
  CHECK(presets[0].width == Catch::Approx(80.0));
  CHECK(presets[0].level == Catch::Approx(40.0));
  CHECK(presets[1].name == "Lung");
  CHECK(presets[1].width == Catch::Approx(1500.0));
  CHECK(presets[1].level == Catch::Approx(-600.0));
}

TEST_CASE("built-in CT presets expose clinically useful width and level values", "[image][ct][presets]")
{
  const auto presets = ct_windowing::builtInPresets();
  REQUIRE_FALSE(presets.empty());
  CHECK(presets.front().name == "Brain");
  CHECK(presets.front().width == Catch::Approx(80.0));
  CHECK(presets.front().level == Catch::Approx(40.0));
}
