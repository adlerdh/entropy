#include "logic/serialization/ProjectSerialization.h"

#include <catch2/catch_test_macros.hpp>

#include <glm/vec3.hpp>

#include <filesystem>
#include <fstream>
#include <string>

namespace fs = std::filesystem;

using json = nlohmann::json;

void to_json(json& j, const Annotation&)
{
  j = json::object();
}

void from_json(const json&, std::vector<Annotation>& annotations)
{
  annotations.clear();
}

namespace
{
fs::path uniqueTempProjectDirectory()
{
  const fs::path base = fs::temp_directory_path() / "entropy-project-serialization-tests";
  fs::create_directories(base);

  for (int i = 0; i < 1000; ++i) {
    fs::path candidate = base / (std::string{"case-"} + std::to_string(i));
    std::error_code ec;
    if (fs::create_directories(candidate, ec)) {
      return candidate;
    }
  }

  throw std::runtime_error("Unable to create temporary project serialization test directory");
}

void touchFile(const fs::path& path)
{
  fs::create_directories(path.parent_path());
  std::ofstream out(path);
  out << "test";
}
} // namespace

TEST_CASE("Project serialization preserves DICOM source metadata", "[project][dicom][serialization]")
{
  const fs::path root = uniqueTempProjectDirectory();
  const fs::path dicomRoot = root / "dicom";
  const fs::path slice1 = dicomRoot / "slice-001.dcm";
  const fs::path slice2 = dicomRoot / "slice-002.dcm";
  const fs::path projectFile = root / "project.json";

  touchFile(slice1);
  touchFile(slice2);

  serialize::EntropyProject project;
  project.m_referenceImage.m_imageFileName = slice1;
  project.m_referenceImage.m_dicomSource = serialize::DicomSource{
    .m_rootPath = dicomRoot,
    .m_studyInstanceUid = "1.2.3.study",
    .m_seriesInstanceUid = "1.2.3.series",
    .m_files = {slice1, slice2}};

  REQUIRE(serialize::save(project, projectFile));

  serialize::EntropyProject loaded;
  REQUIRE(serialize::open(loaded, projectFile));
  REQUIRE(loaded.m_referenceImage.m_dicomSource.has_value());

  const auto& source = *loaded.m_referenceImage.m_dicomSource;
  CHECK(loaded.m_referenceImage.m_imageFileName == fs::canonical(slice1));
  CHECK(source.m_rootPath == fs::canonical(dicomRoot));
  CHECK(source.m_studyInstanceUid == "1.2.3.study");
  CHECK(source.m_seriesInstanceUid == "1.2.3.series");
  REQUIRE(source.m_files.size() == 2);
  CHECK(source.m_files.at(0) == fs::canonical(slice1));
  CHECK(source.m_files.at(1) == fs::canonical(slice2));
}

TEST_CASE("Project serialization preserves interface settings", "[project][serialization]")
{
  serialize::EntropyProject project;
  project.m_referenceImage.m_imageFileName = "image.nii.gz";
  project.m_interface.m_showLayoutTabs = false;
  project.m_interface.m_layoutTabPlacement = serialize::ProjectLayoutTabPlacement::Bottom;
  project.m_interface.m_imageValuePrecision = 4;
  project.m_interface.m_coordsPrecision = 5;
  project.m_interface.m_txPrecision = 6;
  project.m_interface.m_percentilePrecision = 7;

  const json root = project;

  REQUIRE(root.contains("interface"));
  CHECK(root.at("interface").at("showLayoutTabs") == false);
  CHECK(root.at("interface").at("layoutTabsPosition") == "bottom");
  CHECK(root.at("interface").at("imageValuePrecision") == 4);
  CHECK(root.at("interface").at("coordinatesPrecision") == 5);
  CHECK(root.at("interface").at("transformPrecision") == 6);
  CHECK(root.at("interface").at("percentilePrecision") == 7);

  const serialize::EntropyProject parsed = root.get<serialize::EntropyProject>();

  CHECK(parsed.m_interface.m_showLayoutTabs == false);
  CHECK(parsed.m_interface.m_layoutTabPlacement == serialize::ProjectLayoutTabPlacement::Bottom);
  CHECK(parsed.m_interface.m_imageValuePrecision == 4);
  CHECK(parsed.m_interface.m_coordsPrecision == 5);
  CHECK(parsed.m_interface.m_txPrecision == 6);
  CHECK(parsed.m_interface.m_percentilePrecision == 7);
}

TEST_CASE("Project serialization preserves image edge settings", "[project][serialization]")
{
  serialize::EntropyProject project;
  project.m_referenceImage.m_imageFileName = "image.nii.gz";
  project.m_referenceImage.m_settings = serialize::ImageSettings{
    .m_displayName = "Image",
    .m_level = 42.0,
    .m_window = 12.0,
    .m_thresholdLow = 1.0,
    .m_thresholdHigh = 11.0,
    .m_opacity = 0.75,
    .m_activeComponent = 2,
    .m_componentRenderMode = serialize::ProjectComponentRenderMode::Magnitude,
    .m_ignoreAlpha = true,
    .m_componentVisibility = {true, false, true},
    .m_componentOpacities = {1.0, 0.25, 0.5},
    .m_edgeDetectionMethod = serialize::ProjectEdgeDetectionMethod::Pixel,
    .m_showEdges = true,
    .m_thresholdEdges = false,
    .m_thinPixelEdges = true,
    .m_overlayEdges = false,
    .m_colormapEdges = true,
    .m_edgeMagnitude = 0.33,
    .m_pixelEdgeScale = 2.5,
    .m_pixelEdgeThreshold = 0.44,
    .m_edgeColor = glm::vec3{0.1f, 0.2f, 0.3f},
    .m_edgeOpacity = 0.6};

  const json root = project;
  const json& settings = root.at("reference").at("settings");

  CHECK(settings.at("componentRenderMode") == "magnitude");
  CHECK(settings.at("activeComponent") == 2);
  CHECK(settings.at("ignoreAlpha") == true);
  CHECK(settings.at("componentVisibility") == json::array({true, false, true}));
  CHECK(settings.at("componentOpacities") == json::array({1.0, 0.25, 0.5}));
  CHECK(settings.at("edgeDetectionMethod") == "pixel");
  CHECK(settings.at("showEdges") == true);
  CHECK(settings.at("hardEdges") == false);
  CHECK(settings.at("thinPixelEdges") == true);
  CHECK(settings.at("overlayEdges") == false);
  CHECK_FALSE(settings.contains("colormapEdges"));
  CHECK(settings.at("edgeMagnitude") == 0.33);
  CHECK(settings.at("pixelEdgeScale") == 2.5);
  CHECK(settings.at("pixelEdgeThreshold") == 0.44);
  CHECK(settings.at("edgeColor") == json::array({0.1f, 0.2f, 0.3f}));
  CHECK(settings.at("edgeOpacity") == 0.6);

  const serialize::EntropyProject parsed = root.get<serialize::EntropyProject>();
  REQUIRE(parsed.m_referenceImage.m_settings.has_value());
  const serialize::ImageSettings& parsedSettings = *parsed.m_referenceImage.m_settings;

  CHECK(parsedSettings.m_componentRenderMode == serialize::ProjectComponentRenderMode::Magnitude);
  CHECK(parsedSettings.m_activeComponent == 2);
  CHECK(parsedSettings.m_ignoreAlpha);
  CHECK(parsedSettings.m_componentVisibility == std::vector<bool>{true, false, true});
  CHECK(parsedSettings.m_componentOpacities == std::vector<double>{1.0, 0.25, 0.5});
  CHECK(parsedSettings.m_edgeDetectionMethod == serialize::ProjectEdgeDetectionMethod::Pixel);
  CHECK(parsedSettings.m_showEdges);
  CHECK_FALSE(parsedSettings.m_thresholdEdges);
  CHECK(parsedSettings.m_thinPixelEdges);
  CHECK_FALSE(parsedSettings.m_overlayEdges);
  CHECK_FALSE(parsedSettings.m_colormapEdges);
  CHECK(parsedSettings.m_edgeMagnitude == 0.33);
  CHECK(parsedSettings.m_pixelEdgeScale == 2.5);
  CHECK(parsedSettings.m_pixelEdgeThreshold == 0.44);
  CHECK(parsedSettings.m_edgeColor == glm::vec3{0.1f, 0.2f, 0.3f});
  CHECK(parsedSettings.m_edgeOpacity == 0.6);
}

TEST_CASE("Project serialization preserves voxel edge colormap setting", "[project][serialization]")
{
  serialize::EntropyProject project;
  project.m_referenceImage.m_imageFileName = "image.nii.gz";
  project.m_referenceImage.m_settings = serialize::ImageSettings{};
  project.m_referenceImage.m_settings->m_edgeDetectionMethod = serialize::ProjectEdgeDetectionMethod::Voxel;
  project.m_referenceImage.m_settings->m_showEdges = true;
  project.m_referenceImage.m_settings->m_colormapEdges = true;

  const json root = project;
  const json& settings = root.at("reference").at("settings");
  CHECK(settings.at("edgeDetectionMethod") == "voxel");
  CHECK(settings.at("colormapEdges") == true);

  const serialize::EntropyProject parsed = root.get<serialize::EntropyProject>();
  REQUIRE(parsed.m_referenceImage.m_settings.has_value());
  CHECK(parsed.m_referenceImage.m_settings->m_edgeDetectionMethod == serialize::ProjectEdgeDetectionMethod::Voxel);
  CHECK(parsed.m_referenceImage.m_settings->m_colormapEdges);
}
