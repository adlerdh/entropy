#include "logic/serialization/ProjectSerialization.h"

#include <catch2/catch_test_macros.hpp>

#include <glm/mat4x4.hpp>
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

glm::mat4 testManualTransformation()
{
  glm::mat4 tx{1.0f};
  tx[0][0] = 1.25f;
  tx[1][1] = 0.75f;
  tx[2][2] = 1.5f;
  tx[3][0] = 3.0f;
  tx[3][1] = -4.0f;
  tx[3][2] = 5.5f;
  return tx;
}

void checkMat4(const glm::mat4& actual, const glm::mat4& expected)
{
  for (int col = 0; col < 4; ++col) {
    for (int row = 0; row < 4; ++row) {
      CHECK(actual[col][row] == expected[col][row]);
    }
  }
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
  project.m_interface.m_synchronizeTimeSeries = false;

  const json root = project;

  REQUIRE(root.contains("interface"));
  CHECK_FALSE(root.at("interface").contains("showLayoutTabs"));
  CHECK_FALSE(root.at("interface").contains("layoutTabsPosition"));
  CHECK_FALSE(root.at("interface").contains("showGlobalTimeControls"));
  CHECK(root.at("interface").at("synchronizeTimeSeries") == false);
  CHECK_FALSE(root.at("interface").contains("imageValuePrecision"));
  CHECK_FALSE(root.at("interface").contains("coordinatesPrecision"));
  CHECK_FALSE(root.at("interface").contains("transformPrecision"));
  CHECK_FALSE(root.at("interface").contains("percentilePrecision"));

  const serialize::EntropyProject parsed = root.get<serialize::EntropyProject>();

  CHECK(parsed.m_interface.m_synchronizeTimeSeries == false);
}

TEST_CASE("Project serialization preserves project view settings", "[project][serialization]")
{
  serialize::EntropyProject project;
  project.m_referenceImage.m_imageFileName = "image.nii.gz";
  project.m_view.m_anatomicalLabelType = AnatomicalLabelType::Rodent;
  project.m_view.m_lockAnatomicalDirectionsToReferenceImage = true;
  project.m_view.m_crosshairsSnapping = CrosshairsSnapping::ActiveImage;

  const json root = project;

  REQUIRE(root.contains("view"));
  CHECK(root.at("view").at("anatomicalLabelType") == "rodent");
  CHECK(root.at("view").at("lockAnatomicalDirectionsToReferenceImage") == true);
  CHECK(root.at("view").at("crosshairsSnapping") == "activeImage");

  const serialize::EntropyProject parsed = root.get<serialize::EntropyProject>();

  CHECK(parsed.m_view.m_anatomicalLabelType == AnatomicalLabelType::Rodent);
  CHECK(parsed.m_view.m_lockAnatomicalDirectionsToReferenceImage == true);
  CHECK(parsed.m_view.m_crosshairsSnapping == CrosshairsSnapping::ActiveImage);
}

TEST_CASE("Project serialization preserves comparison settings", "[project][serialization]")
{
  serialize::EntropyProject project;
  project.m_referenceImage.m_imageFileName = "image.nii.gz";
  project.m_comparison.m_difference.m_squared = false;
  project.m_comparison.m_difference.m_metric.m_colorMapIndex = 9;
  project.m_comparison.m_difference.m_metric.m_slopeIntercept = {2.0f, -1.0f};
  project.m_comparison.m_difference.m_metric.m_invertColormap = true;
  project.m_comparison.m_difference.m_metric.m_continuousColormap = false;
  project.m_comparison.m_difference.m_metric.m_colormapLevels = 11;
  project.m_comparison.m_localNcc.m_presentation = serialize::ProjectLocalNccPresentation::Correlation;
  project.m_comparison.m_localNcc.m_negativeCorrelationAsMismatch = false;
  project.m_comparison.m_localNcc.m_patchRadius = 5;
  project.m_comparison.m_localNcc.m_sampleSpacing = 2.5f;
  project.m_comparison.m_localNcc.m_minimumValidFraction = 0.6f;
  project.m_comparison.m_localNcc.m_varianceEpsilon = 0.0025f;
  project.m_comparison.m_localNcc.m_invalidStyle = serialize::ProjectLocalMetricInvalidStyle::Gray;
  project.m_comparison.m_localLinearResidual.m_patchRadius = 4;
  project.m_comparison.m_localLinearResidual.m_sampleSpacing = 3.5f;
  project.m_comparison.m_localLinearResidual.m_minimumValidFraction = 0.5f;
  project.m_comparison.m_localLinearResidual.m_varianceEpsilon = 0.0035f;
  project.m_comparison.m_localLinearResidual.m_invalidStyle = serialize::ProjectLocalMetricInvalidStyle::Gray;
  project.m_comparison.m_overlayMagentaCyan = false;
  project.m_comparison.m_quadrants = {false, true};
  project.m_comparison.m_checkerboardSquares = 31;
  project.m_comparison.m_flashlightRadiusFraction = 0.55f;
  project.m_comparison.m_flashlightOverlayMovingImage = false;

  const json root = project;

  REQUIRE(root.contains("comparison"));
  CHECK(root.at("comparison").at("difference").at("squared") == false);
  CHECK(root.at("comparison").at("difference").at("metric").at("colormapIndex") == 9);
  CHECK(root.at("comparison").at("difference").at("metric").at("windowSlopeIntercept").at(0) == 2.0f);
  CHECK(root.at("comparison").at("localNormalizedCrossCorrelation").at("presentation") == "correlation");
  CHECK(root.at("comparison").at("localNormalizedCrossCorrelation").at("invalidStyle") == "gray");
  CHECK(root.at("comparison").at("localLinearResidual").at("patchRadius") == 4);
  CHECK(root.at("comparison").at("overlay").at("magentaCyan") == false);
  CHECK(root.at("comparison").at("quadrants").at("x") == false);
  CHECK(root.at("comparison").at("checkerboard").at("squares") == 31);
  CHECK(root.at("comparison").at("flashlight").at("radiusFraction") == 0.55f);

  const serialize::EntropyProject parsed = root.get<serialize::EntropyProject>();

  CHECK(parsed.m_comparison.m_difference.m_squared == false);
  CHECK(parsed.m_comparison.m_difference.m_metric.m_colorMapIndex == 9);
  CHECK(parsed.m_comparison.m_difference.m_metric.m_slopeIntercept == glm::vec2{2.0f, -1.0f});
  CHECK(parsed.m_comparison.m_difference.m_metric.m_invertColormap == true);
  CHECK(parsed.m_comparison.m_difference.m_metric.m_continuousColormap == false);
  CHECK(parsed.m_comparison.m_difference.m_metric.m_colormapLevels == 11);
  CHECK(parsed.m_comparison.m_localNcc.m_presentation == serialize::ProjectLocalNccPresentation::Correlation);
  CHECK(parsed.m_comparison.m_localNcc.m_negativeCorrelationAsMismatch == false);
  CHECK(parsed.m_comparison.m_localNcc.m_patchRadius == 5);
  CHECK(parsed.m_comparison.m_localNcc.m_sampleSpacing == 2.5f);
  CHECK(parsed.m_comparison.m_localNcc.m_minimumValidFraction == 0.6f);
  CHECK(parsed.m_comparison.m_localNcc.m_varianceEpsilon == 0.0025f);
  CHECK(parsed.m_comparison.m_localNcc.m_invalidStyle == serialize::ProjectLocalMetricInvalidStyle::Gray);
  CHECK(parsed.m_comparison.m_localLinearResidual.m_patchRadius == 4);
  CHECK(parsed.m_comparison.m_localLinearResidual.m_sampleSpacing == 3.5f);
  CHECK(parsed.m_comparison.m_localLinearResidual.m_minimumValidFraction == 0.5f);
  CHECK(parsed.m_comparison.m_localLinearResidual.m_varianceEpsilon == 0.0035f);
  CHECK(parsed.m_comparison.m_localLinearResidual.m_invalidStyle == serialize::ProjectLocalMetricInvalidStyle::Gray);
  CHECK(parsed.m_comparison.m_overlayMagentaCyan == false);
  CHECK(parsed.m_comparison.m_quadrants == glm::ivec2{false, true});
  CHECK(parsed.m_comparison.m_checkerboardSquares == 31);
  CHECK(parsed.m_comparison.m_flashlightRadiusFraction == 0.55f);
  CHECK(parsed.m_comparison.m_flashlightOverlayMovingImage == false);
}

TEST_CASE("Project serialization preserves rendering presentation settings", "[project][serialization]")
{
  serialize::EntropyProject project;
  project.m_referenceImage.m_imageFileName = "image.nii.gz";
  project.m_raycasting.m_samplingFactor = 1.25f;
  project.m_raycasting.m_transparentBackgroundWhenNoHit = false;
  project.m_raycasting.m_renderFrontFaces = false;
  project.m_raycasting.m_renderBackFaces = true;
  project.m_raycasting.m_segmentationMasking = serialize::ProjectSegmentationRaycastMasking::MaskOut;
  project.m_intensityProjection.m_useMaximumImageExtent = true;
  project.m_intensityProjection.m_slabThicknessMm = 12.5f;
  project.m_intensityProjection.m_xrayEnergyKeV = 120.0f;
  project.m_intensityProjection.m_xrayWindow = 0.35f;
  project.m_intensityProjection.m_xrayLevel = 0.65f;
  project.m_segmentationDisplay.m_modulateOpacityWithImageOpacity = false;
  project.m_segmentationDisplay.m_outlineStyle = SegmentationOutlineStyle::ImageVoxel;
  project.m_segmentationDisplay.m_interiorOpacity = 0.4f;
  project.m_segmentationDisplay.m_erosionFactor = 0.8f;
  project.m_isosurfaces.m_floatingPointInterpolation = true;
  project.m_isosurfaces.m_modulateOpacityWithImageOpacity = true;
  project.m_annotationDisplay.m_annotationsOnTop = true;
  project.m_annotationDisplay.m_landmarksOnTop = true;
  project.m_annotationDisplay.m_hideAnnotationVertices = true;

  const json root = project;

  CHECK(root.at("raycasting").at("samplingFactor") == 1.25f);
  CHECK(root.at("raycasting").at("transparentBackgroundWhenNoHit") == false);
  CHECK(root.at("raycasting").at("renderFrontFaces") == false);
  CHECK(root.at("raycasting").at("segmentationMasking") == "maskOut");
  CHECK(root.at("intensityProjection").at("useMaximumImageExtent") == true);
  CHECK(root.at("intensityProjection").at("slabThicknessMm") == 12.5f);
  CHECK(root.at("intensityProjection").at("xrayEnergyKeV") == 120.0f);
  CHECK(root.at("segmentationDisplay").at("outlineStyle") == "voxel");
  CHECK(root.at("segmentationDisplay").at("erosionFactor") == 0.8f);
  CHECK(root.at("isosurfaces").at("floatingPointInterpolation") == true);
  CHECK(root.at("isosurfaces").at("modulateOpacityWithImageOpacity") == true);
  CHECK(root.at("annotationDisplay").at("annotationsOnTop") == true);
  CHECK(root.at("annotationDisplay").at("hideAnnotationVertices") == true);

  const serialize::EntropyProject parsed = root.get<serialize::EntropyProject>();

  CHECK(parsed.m_raycasting.m_samplingFactor == 1.25f);
  CHECK(parsed.m_raycasting.m_transparentBackgroundWhenNoHit == false);
  CHECK(parsed.m_raycasting.m_renderFrontFaces == false);
  CHECK(parsed.m_raycasting.m_renderBackFaces == true);
  CHECK(parsed.m_raycasting.m_segmentationMasking == serialize::ProjectSegmentationRaycastMasking::MaskOut);
  CHECK(parsed.m_intensityProjection.m_useMaximumImageExtent == true);
  CHECK(parsed.m_intensityProjection.m_slabThicknessMm == 12.5f);
  CHECK(parsed.m_intensityProjection.m_xrayEnergyKeV == 120.0f);
  CHECK(parsed.m_intensityProjection.m_xrayWindow == 0.35f);
  CHECK(parsed.m_intensityProjection.m_xrayLevel == 0.65f);
  CHECK(parsed.m_segmentationDisplay.m_modulateOpacityWithImageOpacity == false);
  CHECK(parsed.m_segmentationDisplay.m_outlineStyle == SegmentationOutlineStyle::ImageVoxel);
  CHECK(parsed.m_segmentationDisplay.m_interiorOpacity == 0.4f);
  CHECK(parsed.m_segmentationDisplay.m_erosionFactor == 0.8f);
  CHECK(parsed.m_isosurfaces.m_floatingPointInterpolation == true);
  CHECK(parsed.m_isosurfaces.m_modulateOpacityWithImageOpacity == true);
  CHECK(parsed.m_annotationDisplay.m_annotationsOnTop == true);
  CHECK(parsed.m_annotationDisplay.m_landmarksOnTop == true);
  CHECK(parsed.m_annotationDisplay.m_hideAnnotationVertices == true);
}

TEST_CASE("Project serialization sanitizes project-wide presentation settings", "[project][serialization]")
{
  const json root = {
    {"reference", {{"image", "image.nii.gz"}}},
    {"raycasting",
     {{"samplingFactor", 0.0f},
      {"segmentationMasking", "bad"},
      {"transparentBackgroundWhenNoHit", false},
      {"renderFrontFaces", false}}},
    {"intensityProjection",
     {{"useMaximumImageExtent", true},
      {"slabThicknessMm", -1.0f},
      {"xrayEnergyKeV", 120.0f},
      {"xrayWindow", 0.0f},
      {"xrayLevel", 2.0f}}},
    {"segmentationDisplay",
     {{"modulateOpacityWithImageOpacity", false},
      {"outlineStyle", "bad"},
      {"interiorOpacity", 2.0f},
      {"erosionFactor", 0.0f}}},
    {"isosurfaces", {{"floatingPointInterpolation", true}, {"modulateOpacityWithImageOpacity", true}}},
    {"annotationDisplay", {{"annotationsOnTop", true}, {"landmarksOnTop", true}, {"hideAnnotationVertices", true}}}};

  const serialize::EntropyProject parsed = root.get<serialize::EntropyProject>();

  CHECK(parsed.m_raycasting.m_samplingFactor == 0.1f);
  CHECK(parsed.m_raycasting.m_segmentationMasking == serialize::ProjectSegmentationRaycastMasking::Disabled);
  CHECK(parsed.m_raycasting.m_transparentBackgroundWhenNoHit == false);
  CHECK(parsed.m_raycasting.m_renderFrontFaces == false);
  CHECK(parsed.m_raycasting.m_renderBackFaces == true);
  CHECK(parsed.m_intensityProjection.m_useMaximumImageExtent == true);
  CHECK(parsed.m_intensityProjection.m_slabThicknessMm == 0.0f);
  CHECK(parsed.m_intensityProjection.m_xrayEnergyKeV == 120.0f);
  CHECK(parsed.m_intensityProjection.m_xrayWindow == 1.0e-3f);
  CHECK(parsed.m_intensityProjection.m_xrayLevel == 1.0f);
  CHECK(parsed.m_segmentationDisplay.m_modulateOpacityWithImageOpacity == false);
  CHECK(parsed.m_segmentationDisplay.m_outlineStyle == SegmentationOutlineStyle::Disabled);
  CHECK(parsed.m_segmentationDisplay.m_interiorOpacity == 1.0f);
  CHECK(parsed.m_segmentationDisplay.m_erosionFactor == 0.5f);
  CHECK(parsed.m_isosurfaces.m_floatingPointInterpolation == true);
  CHECK(parsed.m_isosurfaces.m_modulateOpacityWithImageOpacity == true);
  CHECK(parsed.m_annotationDisplay.m_annotationsOnTop == true);
  CHECK(parsed.m_annotationDisplay.m_landmarksOnTop == true);
  CHECK(parsed.m_annotationDisplay.m_hideAnnotationVertices == true);
}

TEST_CASE("Project serialization supports an external layouts file reference", "[project][serialization]")
{
  const fs::path root = uniqueTempProjectDirectory();
  const fs::path imageFile = root / "image.nii.gz";
  const fs::path layoutsFile = root / "layouts.json";
  const fs::path projectFile = root / "project.json";

  touchFile(imageFile);
  touchFile(layoutsFile);

  serialize::EntropyProject project;
  project.m_referenceImage.m_imageFileName = imageFile;
  project.m_layoutsFileName = layoutsFile;
  project.m_layouts.push_back(layout::LayoutSpec{});

  const json inlineJson = project;
  CHECK(inlineJson.at("layoutsFile") == layoutsFile.string());
  CHECK_FALSE(inlineJson.contains("layouts"));

  REQUIRE(serialize::save(project, projectFile));

  const json savedJson = json::parse(std::ifstream(projectFile));
  CHECK(savedJson.at("layoutsFile") == "layouts.json");
  CHECK_FALSE(savedJson.contains("layouts"));

  serialize::EntropyProject loaded;
  REQUIRE(serialize::open(loaded, projectFile));
  REQUIRE(loaded.m_layoutsFileName);
  CHECK(*loaded.m_layoutsFileName == fs::canonical(layoutsFile));
}

TEST_CASE(
  "Project serialization preserves image affine transform references and manual transforms",
  "[project][serialization]")
{
  serialize::EntropyProject project;
  project.m_referenceImage.m_imageFileName = "reference.nii.gz";

  serialize::Image image;
  image.m_imageFileName = "moving.nii.gz";
  image.m_affineTxFileName = "moving-affine.txt";
  image.m_worldDefTx = testManualTransformation();
  project.m_additionalImages.push_back(image);

  const json root = project;
  REQUIRE(root.contains("additional"));
  REQUIRE(root.at("additional").size() == 1);

  const json& serializedImage = root.at("additional").at(0);
  CHECK(serializedImage.at("affine") == "moving-affine.txt");
  REQUIRE(serializedImage.contains("manualTransformation"));

  const serialize::EntropyProject parsed = root.get<serialize::EntropyProject>();
  REQUIRE(parsed.m_additionalImages.size() == 1);
  REQUIRE(parsed.m_additionalImages.at(0).m_affineTxFileName.has_value());
  CHECK(*parsed.m_additionalImages.at(0).m_affineTxFileName == fs::path{"moving-affine.txt"});
  REQUIRE(parsed.m_additionalImages.at(0).m_worldDefTx.has_value());
  checkMat4(*parsed.m_additionalImages.at(0).m_worldDefTx, *image.m_worldDefTx);
}

TEST_CASE(
  "Project file save and open preserve affine transform paths and manual transforms",
  "[project][serialization]")
{
  const fs::path root = uniqueTempProjectDirectory();
  const fs::path referenceFile = root / "reference.nii.gz";
  const fs::path movingFile = root / "moving.nii.gz";
  const fs::path affineFile = root / "moving-affine.txt";
  const fs::path projectFile = root / "project.json";

  touchFile(referenceFile);
  touchFile(movingFile);
  touchFile(affineFile);

  serialize::EntropyProject project;
  project.m_referenceImage.m_imageFileName = referenceFile;

  serialize::Image image;
  image.m_imageFileName = movingFile;
  image.m_affineTxFileName = affineFile;
  image.m_worldDefTx = testManualTransformation();
  project.m_additionalImages.push_back(image);

  REQUIRE(serialize::save(project, projectFile));

  const json saved = json::parse(std::ifstream{projectFile});
  REQUIRE(saved.contains("additional"));
  CHECK(saved.at("additional").at(0).at("affine") == "moving-affine.txt");
  REQUIRE(saved.at("additional").at(0).contains("manualTransformation"));

  serialize::EntropyProject loaded;
  REQUIRE(serialize::open(loaded, projectFile));
  REQUIRE(loaded.m_additionalImages.size() == 1);
  REQUIRE(loaded.m_additionalImages.at(0).m_affineTxFileName.has_value());
  CHECK(*loaded.m_additionalImages.at(0).m_affineTxFileName == fs::canonical(affineFile));
  REQUIRE(loaded.m_additionalImages.at(0).m_worldDefTx.has_value());
  checkMat4(*loaded.m_additionalImages.at(0).m_worldDefTx, *image.m_worldDefTx);
}

TEST_CASE("Project serialization preserves image edge settings", "[project][serialization]")
{
  serialize::EntropyProject project;
  project.m_referenceImage.m_imageFileName = "image.nii.gz";
  project.m_referenceImage.m_settings = serialize::ImageSettings{
    .m_displayName = "Image",
    .m_globalVisibility = false,
    .m_globalOpacity = 0.25,
    .m_borderColor = glm::vec3{0.4f, 0.5f, 0.6f},
    .m_lockedToReference = false,
    .m_warpEnabled = false,
    .m_warpStrength = 2.5f,
    .m_level = 42.0,
    .m_window = 12.0,
    .m_thresholdLow = 1.0,
    .m_thresholdHigh = 11.0,
    .m_opacity = 0.75,
    .m_activeComponent = 2,
    .m_activeTimePoint = 4,
    .m_timePlaybackLoop = false,
    .m_timePlaybackPlaying = true,
    .m_timePlaybackSpeed = 1.5,
    .m_componentRenderMode = serialize::ProjectComponentRenderMode::ComplexPhase,
    .m_complexPhaseUnit = serialize::ProjectComplexPhaseUnit::Degrees,
    .m_complexPhaseRange = serialize::ProjectComplexPhaseRange::Unsigned,
    .m_vectorArrowOverlayVisible = true,
    .m_vectorArrowOverlayOnImage = false,
    .m_vectorArrowOverlayDensity = 24.0f,
    .m_vectorArrowOverlayVoxelSpacing = 2.5f,
    .m_vectorArrowOverlayMillimeterSpacing = 12.5f,
    .m_vectorArrowOverlaySpacingMode = serialize::ProjectVectorArrowOverlaySpacingMode::Millimeters,
    .m_vectorArrowOverlayColor = glm::vec3{0.7f, 0.8f, 0.9f},
    .m_vectorArrowOverlayUseDirectionColor = true,
    .m_vectorArrowOverlayLineThickness = 2.5f,
    .m_vectorArrowOverlayOpacity = 0.65f,
    .m_vectorArrowOverlayScaleByMagnitude = false,
    .m_vectorArrowOverlayScaleFactor = 3.0f,
    .m_vectorWarpedGridVisible = true,
    .m_vectorWarpedGridOverlayOnImage = false,
    .m_vectorWarpedGridConvention = serialize::ProjectVectorWarpedGridConvention::ApparentDeformation,
    .m_vectorWarpedGridPixelSpacing = 40.0f,
    .m_vectorWarpedGridVoxelSpacing = 6.0f,
    .m_vectorWarpedGridMillimeterSpacing = 14.0f,
    .m_vectorWarpedGridSpacingMode = serialize::ProjectVectorArrowOverlaySpacingMode::Voxels,
    .m_vectorWarpedGridLineThickness = 2.25f,
    .m_vectorWarpedGridScaleFactor = 1.75f,
    .m_vectorWarpedGridForegroundColor = glm::vec4{0.1f, 0.2f, 0.3f, 0.4f},
    .m_vectorWarpedGridBackgroundColor = glm::vec4{0.5f, 0.6f, 0.7f, 0.8f},
    .m_vectorPlanarProjectionSignedColors = false,
    .m_vectorLogJacobianDeterminant = true,
    .m_ignoreAlpha = true,
    .m_colorInterpolationMode = InterpolationMode::NearestNeighbor,
    .m_componentLevels = {10.0, 20.0, 30.0},
    .m_componentWindows = {11.0, 22.0, 33.0},
    .m_componentThresholdLows = {1.0, 2.0, 3.0},
    .m_componentThresholdHighs = {9.0, 8.0, 7.0},
    .m_componentVisibility = {true, false, true},
    .m_componentOpacities = {1.0, 0.25, 0.5},
    .m_colorMapIndices = {1, 2, 3},
    .m_colorMapInverted = {false, true, false},
    .m_colorMapContinuous = {true, false, true},
    .m_colorMapLevels = {8, 9, 10},
    .m_colorMapHsvModifiers = {glm::vec3{0.1f, 0.2f, 0.3f}, glm::vec3{0.4f, 0.5f, 0.6f}},
    .m_interpolationModes = {InterpolationMode::Linear, InterpolationMode::NearestNeighbor},
    .m_foregroundThresholdLows = {4.0, 5.0, 6.0},
    .m_foregroundThresholdHighs = {40.0, 50.0, 60.0},
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
    .m_edgeOpacity = 0.6,
    .m_useDistanceMapForRaycasting = false,
    .m_isosurfacesVisible = false,
    .m_applyImageColormapToIsosurfaces = true,
    .m_showIsocontoursIn2D = false,
    .m_isocontourLineWidthIn2D = 3.5,
    .m_isosurfaceOpacityModulator = 0.45f};

  const json root = project;
  const json& settings = root.at("reference").at("settings");

  CHECK(settings.at("globalVisibility") == false);
  CHECK(settings.at("globalOpacity") == 0.25);
  CHECK(settings.at("borderColor") == json::array({0.4f, 0.5f, 0.6f}));
  CHECK(settings.at("lockedToReference") == false);
  CHECK(settings.at("warpEnabled") == false);
  CHECK(settings.at("warpStrength") == 2.5f);
  CHECK(settings.at("componentRenderMode") == "complexPhase");
  CHECK(settings.at("complexPhaseUnit") == "degrees");
  CHECK(settings.at("complexPhaseRange") == "unsigned");
  CHECK(settings.at("vectorArrowOverlayVisible") == true);
  CHECK(settings.at("vectorArrowOverlayOnImage") == false);
  CHECK(settings.at("vectorArrowOverlayDensity") == 24.0f);
  CHECK(settings.at("vectorArrowOverlayVoxelSpacing") == 2.5f);
  CHECK(settings.at("vectorArrowOverlayMillimeterSpacing") == 12.5f);
  CHECK(settings.at("vectorArrowOverlaySpacingMode") == "millimeters");
  CHECK(settings.at("vectorArrowOverlayColor") == json::array({0.7f, 0.8f, 0.9f}));
  CHECK(settings.at("vectorArrowOverlayUseDirectionColor") == true);
  CHECK(settings.at("vectorArrowOverlayLineThickness") == 2.5f);
  CHECK(settings.at("vectorArrowOverlayOpacity") == 0.65f);
  CHECK(settings.at("vectorArrowOverlayScaleByMagnitude") == false);
  CHECK(settings.at("vectorArrowOverlayScaleFactor") == 3.0f);
  CHECK(settings.at("vectorWarpedGridVisible") == true);
  CHECK(settings.at("vectorWarpedGridOverlayOnImage") == false);
  CHECK(settings.at("vectorWarpedGridConvention") == "apparentDeformation");
  CHECK(settings.at("vectorWarpedGridPixelSpacing") == 40.0f);
  CHECK(settings.at("vectorWarpedGridVoxelSpacing") == 6.0f);
  CHECK(settings.at("vectorWarpedGridMillimeterSpacing") == 14.0f);
  CHECK(settings.at("vectorWarpedGridSpacingMode") == "voxels");
  CHECK(settings.at("vectorWarpedGridLineThickness") == 2.25f);
  CHECK(settings.at("vectorWarpedGridScaleFactor") == 1.75f);
  CHECK(settings.at("vectorWarpedGridForegroundColor") == json::array({0.1f, 0.2f, 0.3f, 0.4f}));
  CHECK(settings.at("vectorWarpedGridBackgroundColor") == json::array({0.5f, 0.6f, 0.7f, 0.8f}));
  CHECK(settings.at("vectorPlanarProjectionSignedColors") == false);
  CHECK(settings.at("vectorLogJacobianDeterminant") == true);
  CHECK(settings.at("activeComponent") == 2);
  CHECK(settings.at("activeTimePoint") == 4);
  CHECK(settings.at("timePlaybackLoop") == false);
  CHECK(settings.at("timePlaybackPlaying") == true);
  CHECK(settings.at("timePlaybackSpeed") == 1.5);
  CHECK(settings.at("ignoreAlpha") == true);
  CHECK(settings.at("colorInterpolationMode") == "nearest");
  CHECK(settings.at("componentLevels") == json::array({10.0, 20.0, 30.0}));
  CHECK(settings.at("componentWindows") == json::array({11.0, 22.0, 33.0}));
  CHECK(settings.at("componentVisibility") == json::array({true, false, true}));
  CHECK(settings.at("componentOpacities") == json::array({1.0, 0.25, 0.5}));
  CHECK(settings.at("colorMapIndices") == json::array({1, 2, 3}));
  CHECK(settings.at("colorMapInverted") == json::array({false, true, false}));
  CHECK(settings.at("colorMapContinuous") == json::array({true, false, true}));
  CHECK(settings.at("colorMapLevels") == json::array({8, 9, 10}));
  CHECK(settings.at("interpolationModes") == json::array({"linear", "nearest"}));
  CHECK(settings.at("foregroundThresholdLows") == json::array({4.0, 5.0, 6.0}));
  CHECK(settings.at("foregroundThresholdHighs") == json::array({40.0, 50.0, 60.0}));
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
  CHECK(settings.at("useDistanceMapForRaycasting") == false);
  CHECK(settings.at("isosurfacesVisible") == false);
  CHECK(settings.at("applyImageColormapToIsosurfaces") == true);
  CHECK(settings.at("showIsocontoursIn2D") == false);
  CHECK(settings.at("isocontourLineWidthIn2D") == 3.5);
  CHECK(settings.at("isosurfaceOpacityModulator") == 0.45f);

  const serialize::EntropyProject parsed = root.get<serialize::EntropyProject>();
  REQUIRE(parsed.m_referenceImage.m_settings.has_value());
  const serialize::ImageSettings& parsedSettings = *parsed.m_referenceImage.m_settings;

  CHECK_FALSE(parsedSettings.m_globalVisibility);
  CHECK(parsedSettings.m_globalOpacity == 0.25);
  CHECK(parsedSettings.m_borderColor == glm::vec3{0.4f, 0.5f, 0.6f});
  CHECK_FALSE(parsedSettings.m_lockedToReference);
  CHECK_FALSE(parsedSettings.m_warpEnabled);
  CHECK(parsedSettings.m_warpStrength == 2.5f);
  CHECK(parsedSettings.m_componentRenderMode == serialize::ProjectComponentRenderMode::ComplexPhase);
  CHECK(parsedSettings.m_complexPhaseUnit == serialize::ProjectComplexPhaseUnit::Degrees);
  CHECK(parsedSettings.m_complexPhaseRange == serialize::ProjectComplexPhaseRange::Unsigned);
  CHECK(parsedSettings.m_vectorArrowOverlayVisible);
  CHECK_FALSE(parsedSettings.m_vectorArrowOverlayOnImage);
  CHECK(parsedSettings.m_vectorArrowOverlayDensity == 24.0f);
  CHECK(parsedSettings.m_vectorArrowOverlayVoxelSpacing == 2.5f);
  CHECK(parsedSettings.m_vectorArrowOverlayMillimeterSpacing == 12.5f);
  CHECK(parsedSettings.m_vectorArrowOverlaySpacingMode == serialize::ProjectVectorArrowOverlaySpacingMode::Millimeters);
  CHECK(parsedSettings.m_vectorArrowOverlayColor == glm::vec3{0.7f, 0.8f, 0.9f});
  CHECK(parsedSettings.m_vectorArrowOverlayUseDirectionColor);
  CHECK(parsedSettings.m_vectorArrowOverlayLineThickness == 2.5f);
  CHECK(parsedSettings.m_vectorArrowOverlayOpacity == 0.65f);
  CHECK_FALSE(parsedSettings.m_vectorArrowOverlayScaleByMagnitude);
  CHECK(parsedSettings.m_vectorArrowOverlayScaleFactor == 3.0f);
  CHECK(parsedSettings.m_vectorWarpedGridVisible);
  CHECK_FALSE(parsedSettings.m_vectorWarpedGridOverlayOnImage);
  CHECK(
    parsedSettings.m_vectorWarpedGridConvention == serialize::ProjectVectorWarpedGridConvention::ApparentDeformation);
  CHECK(parsedSettings.m_vectorWarpedGridPixelSpacing == 40.0f);
  CHECK(parsedSettings.m_vectorWarpedGridVoxelSpacing == 6.0f);
  CHECK(parsedSettings.m_vectorWarpedGridMillimeterSpacing == 14.0f);
  CHECK(parsedSettings.m_vectorWarpedGridSpacingMode == serialize::ProjectVectorArrowOverlaySpacingMode::Voxels);
  CHECK(parsedSettings.m_vectorWarpedGridLineThickness == 2.25f);
  CHECK(parsedSettings.m_vectorWarpedGridScaleFactor == 1.75f);
  CHECK(parsedSettings.m_vectorWarpedGridForegroundColor == glm::vec4{0.1f, 0.2f, 0.3f, 0.4f});
  CHECK(parsedSettings.m_vectorWarpedGridBackgroundColor == glm::vec4{0.5f, 0.6f, 0.7f, 0.8f});
  CHECK_FALSE(parsedSettings.m_vectorPlanarProjectionSignedColors);
  CHECK(parsedSettings.m_vectorLogJacobianDeterminant);
  CHECK(parsedSettings.m_activeComponent == 2);
  CHECK(parsedSettings.m_activeTimePoint == 4);
  CHECK_FALSE(parsedSettings.m_timePlaybackLoop);
  CHECK(parsedSettings.m_timePlaybackPlaying);
  CHECK(parsedSettings.m_timePlaybackSpeed == 1.5);
  CHECK(parsedSettings.m_ignoreAlpha);
  CHECK(parsedSettings.m_colorInterpolationMode == InterpolationMode::NearestNeighbor);
  CHECK(parsedSettings.m_componentLevels == std::vector<double>{10.0, 20.0, 30.0});
  CHECK(parsedSettings.m_componentWindows == std::vector<double>{11.0, 22.0, 33.0});
  CHECK(parsedSettings.m_componentThresholdLows == std::vector<double>{1.0, 2.0, 3.0});
  CHECK(parsedSettings.m_componentThresholdHighs == std::vector<double>{9.0, 8.0, 7.0});
  CHECK(parsedSettings.m_componentVisibility == std::vector<bool>{true, false, true});
  CHECK(parsedSettings.m_componentOpacities == std::vector<double>{1.0, 0.25, 0.5});
  CHECK(parsedSettings.m_colorMapIndices == std::vector<std::size_t>{1, 2, 3});
  CHECK(parsedSettings.m_colorMapInverted == std::vector<bool>{false, true, false});
  CHECK(parsedSettings.m_colorMapContinuous == std::vector<bool>{true, false, true});
  CHECK(parsedSettings.m_colorMapLevels == std::vector<std::size_t>{8, 9, 10});
  REQUIRE(parsedSettings.m_colorMapHsvModifiers.size() == 2);
  CHECK(parsedSettings.m_colorMapHsvModifiers.at(0) == glm::vec3{0.1f, 0.2f, 0.3f});
  CHECK(parsedSettings.m_colorMapHsvModifiers.at(1) == glm::vec3{0.4f, 0.5f, 0.6f});
  CHECK(
    parsedSettings.m_interpolationModes ==
    std::vector<InterpolationMode>{InterpolationMode::Linear, InterpolationMode::NearestNeighbor});
  CHECK(parsedSettings.m_foregroundThresholdLows == std::vector<double>{4.0, 5.0, 6.0});
  CHECK(parsedSettings.m_foregroundThresholdHighs == std::vector<double>{40.0, 50.0, 60.0});
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
  CHECK_FALSE(parsedSettings.m_useDistanceMapForRaycasting);
  CHECK_FALSE(parsedSettings.m_isosurfacesVisible);
  CHECK(parsedSettings.m_applyImageColormapToIsosurfaces);
  CHECK_FALSE(parsedSettings.m_showIsocontoursIn2D);
  CHECK(parsedSettings.m_isocontourLineWidthIn2D == 3.5);
  CHECK(parsedSettings.m_isosurfaceOpacityModulator == 0.45f);
}

TEST_CASE("Project serialization preserves inverse and forward warp paths", "[project][serialization]")
{
  const fs::path root = uniqueTempProjectDirectory();
  const fs::path imageFile = root / "moving.nii.gz";
  const fs::path forwardFile = root / "forward.nrrd";
  const fs::path inverseFile = root / "inverse.nrrd";
  const fs::path projectFile = root / "project.json";

  touchFile(imageFile);
  touchFile(forwardFile);
  touchFile(inverseFile);

  serialize::EntropyProject project;
  project.m_referenceImage.m_imageFileName = imageFile;
  project.m_referenceImage.m_inverseWarpFileName = inverseFile;
  project.m_referenceImage.m_forwardWarpFileName = forwardFile;

  const json inlineJson = project;
  CHECK(inlineJson.at("reference").at("inverseWarp") == inverseFile.string());
  CHECK(inlineJson.at("reference").at("forwardWarp") == forwardFile.string());

  REQUIRE(serialize::save(project, projectFile));

  const json savedJson = json::parse(std::ifstream(projectFile));
  CHECK(savedJson.at("reference").at("inverseWarp") == "inverse.nrrd");
  CHECK(savedJson.at("reference").at("forwardWarp") == "forward.nrrd");

  serialize::EntropyProject loaded;
  REQUIRE(serialize::open(loaded, projectFile));
  REQUIRE(loaded.m_referenceImage.m_inverseWarpFileName);
  REQUIRE(loaded.m_referenceImage.m_forwardWarpFileName);
  CHECK(*loaded.m_referenceImage.m_inverseWarpFileName == fs::canonical(inverseFile));
  CHECK(*loaded.m_referenceImage.m_forwardWarpFileName == fs::canonical(forwardFile));
}

TEST_CASE("Project serialization preserves registration result artifacts", "[project][serialization]")
{
  const fs::path root = uniqueTempProjectDirectory();
  const fs::path imageFile = root / "moving.nii.gz";
  const fs::path manifestFile = root / "registration" / "result.json";
  const fs::path warpedFile = root / "registration" / "warped.nii.gz";
  const fs::path inverseFile = root / "registration" / "inverse.nrrd";
  const fs::path forwardFile = root / "registration" / "forward.nrrd";
  const fs::path affineFile = root / "registration" / "affine.mat";
  const fs::path segFile = root / "registration" / "seg.nii.gz";
  const fs::path surfaceFile = root / "registration" / "surface.vtk";
  const fs::path landmarksFile = root / "registration" / "landmarks.json";
  const fs::path projectFile = root / "project.json";

  touchFile(imageFile);
  touchFile(manifestFile);
  touchFile(warpedFile);
  touchFile(inverseFile);
  touchFile(forwardFile);
  touchFile(affineFile);
  touchFile(segFile);
  touchFile(surfaceFile);
  touchFile(landmarksFile);

  serialize::EntropyProject project;
  project.m_referenceImage.m_imageFileName = imageFile;
  project.m_registrationResults.push_back(serialize::RegistrationResult{
    .m_backend = "Greedy",
    .m_fixedImageUid = "fixed",
    .m_movingImageUid = "moving",
    .m_manifestFileName = manifestFile,
    .m_warpedImage = warpedFile,
    .m_inverseWarp = inverseFile,
    .m_forwardWarp = forwardFile,
    .m_affineTransform = affineFile,
    .m_warpedSegmentations = {segFile},
    .m_transformedSurfaces = {surfaceFile},
    .m_transformedLandmarks = {landmarksFile},
    .m_warnings = {"low overlap"}});

  const json inlineJson = project;
  REQUIRE(inlineJson.contains("registrationResults"));
  CHECK(inlineJson.at("registrationResults").at(0).at("backend") == "Greedy");
  CHECK(inlineJson.at("registrationResults").at(0).at("manifest") == manifestFile.string());
  CHECK(inlineJson.at("registrationResults").at(0).at("warnings").at(0) == "low overlap");

  REQUIRE(serialize::save(project, projectFile));

  const json savedJson = json::parse(std::ifstream(projectFile));
  const json& savedResult = savedJson.at("registrationResults").at(0);
  CHECK(savedResult.at("manifest") == "registration/result.json");
  CHECK(savedResult.at("warpedImage") == "registration/warped.nii.gz");
  CHECK(savedResult.at("inverseWarp") == "registration/inverse.nrrd");
  CHECK(savedResult.at("forwardWarp") == "registration/forward.nrrd");
  CHECK(savedResult.at("warpedSegmentations").at(0) == "registration/seg.nii.gz");

  serialize::EntropyProject loaded;
  REQUIRE(serialize::open(loaded, projectFile));
  REQUIRE(loaded.m_registrationResults.size() == 1);
  const serialize::RegistrationResult& loadedResult = loaded.m_registrationResults.front();
  REQUIRE(loadedResult.m_manifestFileName);
  REQUIRE(loadedResult.m_warpedImage);
  REQUIRE(loadedResult.m_inverseWarp);
  CHECK(*loadedResult.m_manifestFileName == fs::canonical(manifestFile));
  CHECK(*loadedResult.m_warpedImage == fs::canonical(warpedFile));
  CHECK(*loadedResult.m_inverseWarp == fs::canonical(inverseFile));
  REQUIRE(loadedResult.m_warpedSegmentations.size() == 1);
  CHECK(loadedResult.m_warpedSegmentations.front() == fs::canonical(segFile));
  REQUIRE(loadedResult.m_warnings.size() == 1);
  CHECK(loadedResult.m_warnings.front() == "low overlap");
}

TEST_CASE("Project serialization preserves segmentation settings", "[project][serialization]")
{
  serialize::EntropyProject project;
  project.m_referenceImage.m_imageFileName = "image.nii.gz";
  project.m_referenceImage.m_segmentations.push_back(serialize::Segmentation{
    .m_segFileName = "seg.nii.gz",
    .m_settings = serialize::SegSettings{
      .m_displayName = "Seg",
      .m_visibility = false,
      .m_opacity = 0.35,
      .m_activeComponent = 0,
      .m_componentVisibility = {false},
      .m_componentOpacities = {0.35},
      .m_labelTableIndices = {3},
      .m_interpolationModes = {InterpolationMode::NearestNeighbor}}});

  const json root = project;
  const json& settings = root.at("reference").at("segmentations").at(0).at("settings");

  CHECK(settings.at("displayName") == "Seg");
  CHECK(settings.at("visibility") == false);
  CHECK(settings.at("opacity") == 0.35);
  CHECK(settings.at("componentVisibility") == json::array({false}));
  CHECK(settings.at("componentOpacities") == json::array({0.35}));
  CHECK(settings.at("labelTableIndices") == json::array({3}));
  CHECK(settings.at("interpolationModes") == json::array({"nearest"}));

  const serialize::EntropyProject parsed = root.get<serialize::EntropyProject>();
  REQUIRE(parsed.m_referenceImage.m_segmentations.size() == 1);
  REQUIRE(parsed.m_referenceImage.m_segmentations.front().m_settings.has_value());
  const serialize::SegSettings& parsedSettings = *parsed.m_referenceImage.m_segmentations.front().m_settings;
  CHECK(parsedSettings.m_displayName == "Seg");
  CHECK_FALSE(parsedSettings.m_visibility);
  CHECK(parsedSettings.m_opacity == 0.35);
  CHECK(parsedSettings.m_componentVisibility == std::vector<bool>{false});
  CHECK(parsedSettings.m_componentOpacities == std::vector<double>{0.35});
  CHECK(parsedSettings.m_labelTableIndices == std::vector<std::size_t>{3});
  CHECK(parsedSettings.m_interpolationModes == std::vector<InterpolationMode>{InterpolationMode::NearestNeighbor});
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
