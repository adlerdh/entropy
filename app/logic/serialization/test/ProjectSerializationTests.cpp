#include "logic/serialization/ProjectSerialization.h"

#include <catch2/catch_test_macros.hpp>

#include <glm/mat4x4.hpp>
#include <glm/mat3x3.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#include <filesystem>
#include <fstream>
#include <set>
#include <string>

namespace fs = std::filesystem;

using json = nlohmann::json;

Annotation::Annotation() = default;

void to_json(json& j, const Annotation&)
{
  j = json::object();
}

void from_json(const json&, std::vector<Annotation>& annotations)
{
  annotations.clear();
}

json annotationsToJson(const std::vector<Annotation>& annotations)
{
  json annotationArray = json::array();
  for (std::size_t i = 0; i < annotations.size(); ++i) {
    annotationArray.emplace_back(json::object());
  }
  return json{{"version", {{"major", 1}, {"minor", 0}}}, {"annotations", std::move(annotationArray)}};
}

std::vector<Annotation> annotationsFromJson(const json&)
{
  return {};
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

TEST_CASE("Project serialization omits default settings", "[project][serialization]")
{
  serialize::EntropyProject project;
  project.m_referenceImage.m_imageFileName = "image.nii.gz";
  project.m_referenceImage.m_settings = serialize::ImageSettings{};
  project.m_referenceImage.m_segmentations.push_back(
    serialize::Segmentation{.m_segFileName = "seg.nii.gz", .m_settings = serialize::SegSettings{}});

  const json root = project;

  CHECK(root.at("version").at("major") == 1);
  REQUIRE(root.at("images").size() == 1);
  CHECK(root.at("images").at(0).at("path") == "image.nii.gz");
  CHECK_FALSE(root.contains("settings"));
  CHECK_FALSE(root.at("images").at(0).contains("settings"));
  CHECK_FALSE(root.at("images").at(0).at("segmentations").at(0).contains("settings"));
}

TEST_CASE("Project serialization accepts missing version as current format", "[project][serialization]")
{
  const json root = {{"images", json::array({{{"path", "image.nii.gz"}}})}};

  const serialize::EntropyProject parsed = root.get<serialize::EntropyProject>();
  CHECK(parsed.m_referenceImage.m_imageFileName == fs::path{"image.nii.gz"});
  CHECK(parsed.m_additionalImages.empty());
}

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

  const json savedJson = json::parse(std::ifstream(projectFile));
  CHECK(savedJson.at("version").at("major") == 1);
  CHECK(savedJson.at("images").at(0).at("dicomSource").at("rootPath") == "dicom");
  CHECK_FALSE(savedJson.at("images").at(0).at("dicomSource").contains("root"));
  CHECK(
    savedJson.at("images").at(0).at("dicomSource").at("paths") ==
    json::array({"dicom/slice-001.dcm", "dicom/slice-002.dcm"}));
  CHECK_FALSE(savedJson.at("images").at(0).at("dicomSource").contains("files"));

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

  REQUIRE(root.contains("settings"));
  REQUIRE(root.at("settings").contains("interface"));
  CHECK_FALSE(root.contains("interface"));
  const json& interface = root.at("settings").at("interface");
  CHECK_FALSE(interface.contains("showLayoutTabs"));
  CHECK_FALSE(interface.contains("layoutTabsPosition"));
  CHECK_FALSE(interface.contains("showGlobalTimeControls"));
  CHECK(interface.at("synchronizeTimeSeries") == false);
  CHECK_FALSE(interface.contains("imageValuePrecision"));
  CHECK_FALSE(interface.contains("coordinatesPrecision"));
  CHECK_FALSE(interface.contains("transformPrecision"));
  CHECK_FALSE(interface.contains("percentilePrecision"));

  const serialize::EntropyProject parsed = root.get<serialize::EntropyProject>();

  CHECK(parsed.m_interface.m_synchronizeTimeSeries == false);
}

TEST_CASE("Project serialization preserves project view settings", "[project][serialization]")
{
  serialize::EntropyProject project;
  project.m_referenceImage.m_imageFileName = "image.nii.gz";
  project.m_view.m_showImageBorders = false;
  project.m_view.m_showImageBordersInLightboxViews = false;
  project.m_view.m_showCrosshairs = false;
  project.m_view.m_showCrosshairsInLightboxViews = false;
  project.m_view.m_showAnatomicalLabels = false;
  project.m_view.m_showAnatomicalLabelsInLightboxViews = false;
  project.m_view.m_showScaleBars = true;
  project.m_view.m_showScaleBarsInLightboxViews = true;
  project.m_view.m_anatomicalLabelType = AnatomicalLabelType::Rodent;
  project.m_view.m_lockAnatomicalDirectionsToReferenceImage = true;
  project.m_view.m_crosshairsSnapping = CrosshairsSnapping::ActiveImage;

  const json root = project;

  REQUIRE(root.contains("settings"));
  REQUIRE(root.at("settings").contains("view"));
  CHECK_FALSE(root.contains("view"));
  const json& view = root.at("settings").at("view");
  CHECK(view.at("imageBorders").at("visible") == false);
  CHECK_FALSE(view.at("imageBorders").contains("visibleInLightboxes"));
  CHECK(view.at("crosshairs").at("visible") == false);
  CHECK(view.at("crosshairs").at("visibleInLightboxes") == false);
  CHECK(view.at("crosshairs").at("snapping") == "activeImage");
  CHECK(view.at("anatomicalLabels").at("visible") == false);
  CHECK(view.at("anatomicalLabels").at("visibleInLightboxes") == false);
  CHECK(view.at("anatomicalLabels").at("type") == "rodent");
  CHECK(view.at("anatomicalLabels").at("lockDirectionsToReferenceImage") == true);
  CHECK(view.at("scaleBars").at("visible") == true);
  CHECK(view.at("scaleBars").at("visibleInLightboxes") == true);
  CHECK_FALSE(view.contains("showImageBorders"));
  CHECK_FALSE(view.contains("showImageBordersInLightboxViews"));
  CHECK_FALSE(view.contains("showCrosshairs"));
  CHECK_FALSE(view.contains("showCrosshairsInLightboxViews"));
  CHECK_FALSE(view.contains("showAnatomicalLabels"));
  CHECK_FALSE(view.contains("showAnatomicalLabelsInLightboxViews"));
  CHECK_FALSE(view.contains("showScaleBars"));
  CHECK_FALSE(view.contains("showScaleBarsInLightboxViews"));
  CHECK_FALSE(view.contains("anatomicalLabelType"));
  CHECK_FALSE(view.contains("lockAnatomicalDirectionsToReferenceImage"));
  CHECK_FALSE(view.contains("crosshairsSnapping"));

  const serialize::EntropyProject parsed = root.get<serialize::EntropyProject>();

  CHECK(parsed.m_view.m_showImageBorders == false);
  CHECK(parsed.m_view.m_showImageBordersInLightboxViews == false);
  CHECK(parsed.m_view.m_showCrosshairs == false);
  CHECK(parsed.m_view.m_showCrosshairsInLightboxViews == false);
  CHECK(parsed.m_view.m_showAnatomicalLabels == false);
  CHECK(parsed.m_view.m_showAnatomicalLabelsInLightboxViews == false);
  CHECK(parsed.m_view.m_showScaleBars == true);
  CHECK(parsed.m_view.m_showScaleBarsInLightboxViews == true);
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

  REQUIRE(root.contains("settings"));
  REQUIRE(root.at("settings").contains("rendering"));
  REQUIRE(root.at("settings").at("rendering").contains("comparison"));
  CHECK_FALSE(root.contains("comparison"));
  const json& comparison = root.at("settings").at("rendering").at("comparison");
  CHECK(comparison.at("difference").at("squared") == false);
  CHECK(comparison.at("difference").at("metric").at("colormapIndex") == 9);
  CHECK(comparison.at("difference").at("metric").at("windowSlopeIntercept").at(0) == 2.0f);
  CHECK(comparison.at("localNormalizedCrossCorrelation").at("presentation") == "correlation");
  CHECK(comparison.at("localNormalizedCrossCorrelation").at("invalidStyle") == "gray");
  CHECK(comparison.at("localLinearResidual").at("patchRadius") == 4);
  CHECK_FALSE(comparison.contains("overlay"));
  CHECK(comparison.at("quadrants").at("x") == false);
  CHECK(comparison.at("checkerboard").at("squares") == 31);
  CHECK(comparison.at("flashlight").at("radiusFraction") == 0.55f);

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
  project.m_raycasting.m_backgroundEdgeBrighteningEnabled = false;
  project.m_raycasting.m_showCrosshairsIn3D = true;
  project.m_raycasting.m_crosshairs3DGlyphDiameterVoxelDiagonals = 1.75f;
  project.m_raycasting.m_showThreeDCameraFrustumIn2DViews = true;
  project.m_raycasting.m_reverseThreeDRotateAboutEye = true;
  project.m_raycasting.m_threeDCameraFrustumColor = {1.0f, 0.25f, 0.75f, 0.8f};
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
  project.m_isosurfaces.m_floatingPointInterpolationPolicy = FloatingPointLinearInterpolationPolicy::FloatingPoint;
  project.m_isosurfaces.m_modulateOpacityWithImageOpacity = true;
  project.m_annotationDisplay.m_annotationsOnTop = true;
  project.m_annotationDisplay.m_landmarksOnTop = true;
  project.m_annotationDisplay.m_hideAnnotationVertices = true;

  const json root = project;

  REQUIRE(root.contains("settings"));
  REQUIRE(root.at("settings").contains("rendering"));
  const json& settings = root.at("settings");
  const json& rendering = settings.at("rendering");
  const json& raycasting = rendering.at("volumeRaycasting");
  CHECK_FALSE(root.contains("raycasting"));
  CHECK_FALSE(root.contains("intensityProjection"));
  CHECK_FALSE(root.contains("segmentationDisplay"));
  CHECK_FALSE(root.contains("isosurfaceDisplay"));
  CHECK_FALSE(root.contains("annotationDisplay"));
  CHECK_FALSE(rendering.contains("raycasting"));
  CHECK(raycasting.at("samplingFactor") == 1.25f);
  CHECK_FALSE(raycasting.contains("adaptiveSamplingEnabled"));
  CHECK_FALSE(raycasting.contains("adaptiveSamplingTargetFrameRate"));
  CHECK(raycasting.at("transparentBackgroundWhenNoHit") == false);
  CHECK(raycasting.at("showImageBox") == false);
  CHECK_FALSE(raycasting.contains("showCrosshairsGlyph"));
  CHECK(raycasting.at("crosshairsGlyphDiameterVoxels") == 1.75f);
  CHECK(raycasting.at("showCameraFrustumIn2DViews") == true);
  CHECK(raycasting.at("reverseRotateAboutEye") == true);
  CHECK(raycasting.at("cameraFrustumColor").at(0) == 1.0f);
  CHECK(raycasting.at("cameraFrustumColor").at(1) == 0.25f);
  CHECK(raycasting.at("cameraFrustumColor").at(2) == 0.75f);
  CHECK(raycasting.at("cameraFrustumColor").at(3) == 0.8f);
  CHECK(raycasting.at("renderFrontFaces") == false);
  CHECK_FALSE(raycasting.contains("renderBackFaces"));
  CHECK(raycasting.at("segmentationMasking") == "maskOut");
  CHECK_FALSE(raycasting.contains("backgroundEdgeBrighteningEnabled"));
  CHECK_FALSE(raycasting.contains("showCrosshairsIn3D"));
  CHECK_FALSE(raycasting.contains("crosshairs3DGlyphDiameterVoxelDiagonals"));
  CHECK_FALSE(raycasting.contains("showThreeDCameraFrustumIn2DViews"));
  CHECK_FALSE(raycasting.contains("reverseThreeDRotateAboutEye"));
  CHECK_FALSE(raycasting.contains("threeDCameraFrustumColor"));
  CHECK(rendering.at("intensityProjection").at("useMaximumImageExtent") == true);
  CHECK(rendering.at("intensityProjection").at("slabThicknessMm") == 12.5f);
  CHECK(rendering.at("intensityProjection").at("xrayEnergyKeV") == 120.0f);
  CHECK(rendering.at("segmentation").at("outlineStyle") == "voxel");
  CHECK(rendering.at("segmentation").at("erosionFactor") == 0.8f);
  CHECK(rendering.at("isosurfaces").at("floatingPointInterpolationPolicy") == "floatingPoint");
  CHECK(rendering.at("isosurfaces").at("modulateOpacityWithImageOpacity") == true);
  CHECK(settings.at("annotations").at("rendering").at("annotationsOnTop") == true);
  CHECK(settings.at("annotations").at("rendering").at("landmarksOnTop") == true);
  CHECK(settings.at("annotations").at("rendering").at("hideAnnotationVertices") == true);
  CHECK_FALSE(settings.at("annotations").contains("annotationsOnTop"));
  CHECK_FALSE(settings.at("annotations").contains("landmarksOnTop"));
  CHECK_FALSE(settings.at("annotations").contains("hideAnnotationVertices"));

  const serialize::EntropyProject parsed = root.get<serialize::EntropyProject>();

  CHECK(parsed.m_raycasting.m_samplingFactor == 1.25f);
  CHECK(parsed.m_raycasting.m_transparentBackgroundWhenNoHit == false);
  CHECK(parsed.m_raycasting.m_backgroundEdgeBrighteningEnabled == false);
  CHECK(parsed.m_raycasting.m_showCrosshairsIn3D == true);
  CHECK(parsed.m_raycasting.m_crosshairs3DGlyphDiameterVoxelDiagonals == 1.75f);
  CHECK(parsed.m_raycasting.m_showThreeDCameraFrustumIn2DViews == true);
  CHECK(parsed.m_raycasting.m_reverseThreeDRotateAboutEye == true);
  CHECK(parsed.m_raycasting.m_threeDCameraFrustumColor.x == 1.0f);
  CHECK(parsed.m_raycasting.m_threeDCameraFrustumColor.y == 0.25f);
  CHECK(parsed.m_raycasting.m_threeDCameraFrustumColor.z == 0.75f);
  CHECK(parsed.m_raycasting.m_threeDCameraFrustumColor.w == 0.8f);
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
  CHECK(
    parsed.m_isosurfaces.m_floatingPointInterpolationPolicy == FloatingPointLinearInterpolationPolicy::FloatingPoint);
  CHECK(parsed.m_isosurfaces.m_modulateOpacityWithImageOpacity == true);
  CHECK(parsed.m_annotationDisplay.m_annotationsOnTop == true);
  CHECK(parsed.m_annotationDisplay.m_landmarksOnTop == true);
  CHECK(parsed.m_annotationDisplay.m_hideAnnotationVertices == true);
}

TEST_CASE("Project serialization sanitizes project-wide presentation settings", "[project][serialization]")
{
  const json root = {
    {"version", {{"major", 1}, {"minor", 0}}},
    {"images", json::array({{{"path", "image.nii.gz"}}})},
    {"settings",
     {
       {"rendering",
        {
          {"volumeRaycasting",
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
          {"segmentation",
           {{"modulateOpacityWithImageOpacity", false},
            {"outlineStyle", "bad"},
            {"interiorOpacity", 2.0f},
            {"erosionFactor", 0.0f}}},
          {"isosurfaces",
           {{"floatingPointInterpolationPolicy", "floatingPoint"}, {"modulateOpacityWithImageOpacity", true}}},
        }},
       {"annotations",
        {{"rendering", {{"annotationsOnTop", true}, {"landmarksOnTop", true}, {"hideAnnotationVertices", true}}}}},
     }}};

  const serialize::EntropyProject parsed = root.get<serialize::EntropyProject>();

  CHECK(parsed.m_raycasting.m_samplingFactor == 0.5f);
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
  CHECK(
    parsed.m_isosurfaces.m_floatingPointInterpolationPolicy == FloatingPointLinearInterpolationPolicy::FloatingPoint);
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
  CHECK(inlineJson.at("version").at("major") == 1);
  REQUIRE(inlineJson.at("layouts").is_object());
  CHECK(inlineJson.at("layouts").at("path") == layoutsFile.generic_string());
  CHECK_FALSE(inlineJson.contains("currentLayout"));
  CHECK_FALSE(inlineJson.contains("layoutsPath"));
  CHECK_FALSE(inlineJson.contains("layoutsFile"));
  CHECK_FALSE(inlineJson.at("layouts").contains("embedded"));

  REQUIRE(serialize::save(project, projectFile));

  const json savedJson = json::parse(std::ifstream(projectFile));
  CHECK(savedJson.at("layouts").at("path") == "layouts.json");
  CHECK_FALSE(savedJson.contains("currentLayout"));
  CHECK_FALSE(savedJson.contains("layoutsPath"));
  CHECK_FALSE(savedJson.contains("layoutsFile"));
  CHECK_FALSE(savedJson.at("layouts").contains("embedded"));

  serialize::EntropyProject loaded;
  REQUIRE(serialize::open(loaded, projectFile));
  REQUIRE(loaded.m_layoutsFileName);
  CHECK(*loaded.m_layoutsFileName == fs::canonical(layoutsFile));
}

TEST_CASE(
  "Project serialization embeds layouts when no external layouts file is referenced",
  "[project][serialization]")
{
  const fs::path root = uniqueTempProjectDirectory();
  const fs::path imageFile = root / "image.nii.gz";

  touchFile(imageFile);

  layout::LayoutSpec layout;
  layout.m_displayName = "Review";
  layout.m_kind = 3;

  serialize::EntropyProject project;
  project.m_referenceImage.m_imageFileName = imageFile;
  project.m_currentLayoutIndex = 1;
  project.m_layouts.push_back(layout);

  const json serialized = project;
  CHECK(serialized.at("version").at("major") == 1);
  CHECK_FALSE(serialized.contains("layoutsPath"));
  REQUIRE(serialized.contains("layouts"));
  REQUIRE(serialized.at("layouts").is_object());
  REQUIRE(serialized.at("layouts").at("embedded").is_array());
  REQUIRE(serialized.at("layouts").at("embedded").size() == 1);
  CHECK(serialized.at("layouts").at("embedded").at(0).at("displayName") == "Review");
  CHECK(serialized.at("layouts").at("current") == 1);
  CHECK_FALSE(serialized.contains("currentLayout"));
  CHECK_FALSE(serialized.contains("currentLayoutIndex"));

  serialize::EntropyProject loaded = serialized.get<serialize::EntropyProject>();
  CHECK_FALSE(loaded.m_layoutsFileName);
  REQUIRE(loaded.m_layouts.size() == 1);
  CHECK(loaded.m_layouts.front().m_displayName == "Review");
  REQUIRE(loaded.m_currentLayoutIndex);
  CHECK(*loaded.m_currentLayoutIndex == 1);
}

TEST_CASE("Project serialization preserves removed default layout indices", "[project][serialization]")
{
  serialize::EntropyProject project;
  project.m_referenceImage.m_imageFileName = "image.nii.gz";
  project.m_removedDefaultLayoutIndices = {1, 4};
  project.m_currentLayoutIndex = 3;

  const json serialized = project;
  REQUIRE(serialized.contains("layouts"));
  REQUIRE(serialized.at("layouts").at("removedDefaults").is_array());
  CHECK(serialized.at("layouts").at("removedDefaults") == json::array({1, 4}));
  CHECK(serialized.at("layouts").at("current") == 3);
  CHECK_FALSE(serialized.at("layouts").contains("embedded"));

  const serialize::EntropyProject loaded = serialized.get<serialize::EntropyProject>();
  CHECK(loaded.m_removedDefaultLayoutIndices == std::vector<std::size_t>{1, 4});
  REQUIRE(loaded.m_currentLayoutIndex);
  CHECK(*loaded.m_currentLayoutIndex == 3);
}

TEST_CASE("Project serialization preserves modified default layout overrides", "[project][serialization]")
{
  serialize::EntropyProject project;
  project.m_referenceImage.m_imageFileName = "image.nii.gz";
  project.m_modifiedDefaultLayouts.push_back(serialize::DefaultLayoutOverride{
    .m_index = 2,
    .m_layout = layout::LayoutSpec{.m_kind = 3, .m_grid = layout::GridSpec{}, .m_views = {layout::ViewSpec{}}}});
  project.m_currentLayoutIndex = 2;

  const json serialized = project;
  REQUIRE(serialized.contains("layouts"));
  REQUIRE(serialized.at("layouts").at("modifiedDefaults").is_array());
  REQUIRE(serialized.at("layouts").at("modifiedDefaults").size() == 1);
  CHECK(serialized.at("layouts").at("modifiedDefaults").at(0).at("index") == 2);
  CHECK(serialized.at("layouts").at("modifiedDefaults").at(0).at("layout").at("kind") == "oneUp");
  CHECK(serialized.at("layouts").at("current") == 2);
  CHECK_FALSE(serialized.at("layouts").contains("embedded"));

  const serialize::EntropyProject loaded = serialized.get<serialize::EntropyProject>();
  REQUIRE(loaded.m_modifiedDefaultLayouts.size() == 1);
  CHECK(loaded.m_modifiedDefaultLayouts.front().m_index == 2);
  CHECK(loaded.m_modifiedDefaultLayouts.front().m_layout.m_kind == 3);
  REQUIRE(loaded.m_currentLayoutIndex);
  CHECK(*loaded.m_currentLayoutIndex == 2);
}

TEST_CASE("Project serialization omits layouts when no layout state is provided", "[project][serialization]")
{
  serialize::EntropyProject project;
  project.m_referenceImage.m_imageFileName = "image.nii.gz";

  const json serialized = project;
  CHECK_FALSE(serialized.contains("layouts"));

  const serialize::EntropyProject loaded = serialized.get<serialize::EntropyProject>();
  CHECK_FALSE(loaded.m_layoutsFileName);
  CHECK(loaded.m_layouts.empty());
  CHECK_FALSE(loaded.m_currentLayoutIndex);
}

TEST_CASE(
  "Project serialization preserves image affine transform references and manual transforms",
  "[project][serialization]")
{
  serialize::EntropyProject project;
  project.m_referenceImage.m_imageFileName = "reference.nii.gz";

  serialize::Image image;
  image.m_imageFileName = "moving.nii.gz";
  image.m_initialAffineMatrix = testManualTransformation();
  image.m_manualAffineMatrix = testManualTransformation();
  image.m_manualAffineMatrix->operator[](3).x = 7.0f;
  project.m_additionalImages.push_back(image);

  const json root = project;
  REQUIRE(root.contains("images"));
  REQUIRE(root.at("images").size() == 2);

  const json& serializedImage = root.at("images").at(1);
  CHECK(serializedImage.at("path") == "moving.nii.gz");
  CHECK_FALSE(serializedImage.contains("image"));
  REQUIRE(serializedImage.at("initialAffine").is_object());
  REQUIRE(serializedImage.at("initialAffine").contains("matrix"));
  CHECK_FALSE(serializedImage.at("initialAffine").contains("file"));
  CHECK_FALSE(serializedImage.at("initialAffine").contains("path"));
  REQUIRE(serializedImage.contains("manualAffine"));
  REQUIRE(serializedImage.at("manualAffine").is_object());
  REQUIRE(serializedImage.at("manualAffine").contains("matrix"));
  CHECK_FALSE(serializedImage.at("manualAffine").contains("file"));
  CHECK_FALSE(serializedImage.at("manualAffine").contains("path"));
  CHECK_FALSE(serializedImage.contains("affine"));
  CHECK_FALSE(serializedImage.contains("manualTransformation"));

  const serialize::EntropyProject parsed = root.get<serialize::EntropyProject>();
  REQUIRE(parsed.m_additionalImages.size() == 1);
  CHECK_FALSE(parsed.m_additionalImages.at(0).m_initialAffineFileName.has_value());
  REQUIRE(parsed.m_additionalImages.at(0).m_initialAffineMatrix.has_value());
  checkMat4(*parsed.m_additionalImages.at(0).m_initialAffineMatrix, *image.m_initialAffineMatrix);
  CHECK_FALSE(parsed.m_additionalImages.at(0).m_manualAffineFileName.has_value());
  REQUIRE(parsed.m_additionalImages.at(0).m_manualAffineMatrix.has_value());
  checkMat4(*parsed.m_additionalImages.at(0).m_manualAffineMatrix, *image.m_manualAffineMatrix);
}

TEST_CASE("Project serialization accepts path-backed image affine transforms", "[project][serialization]")
{
  const json root = {
    {"version", {{"major", 1}, {"minor", 0}}},
    {"images",
     json::array(
       {{{"path", "reference.nii.gz"}},
        {{"path", "moving.nii.gz"},
         {"initialAffine", {{"path", "moving-initial.txt"}}},
         {"manualAffine", {{"path", "moving-manual.txt"}}}}})}};

  const serialize::EntropyProject parsed = root.get<serialize::EntropyProject>();
  REQUIRE(parsed.m_additionalImages.size() == 1);
  REQUIRE(parsed.m_additionalImages.at(0).m_initialAffineFileName);
  CHECK(*parsed.m_additionalImages.at(0).m_initialAffineFileName == fs::path{"moving-initial.txt"});
  CHECK_FALSE(parsed.m_additionalImages.at(0).m_initialAffineMatrix);
  REQUIRE(parsed.m_additionalImages.at(0).m_manualAffineFileName);
  CHECK(*parsed.m_additionalImages.at(0).m_manualAffineFileName == fs::path{"moving-manual.txt"});
  CHECK_FALSE(parsed.m_additionalImages.at(0).m_manualAffineMatrix);
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
  image.m_initialAffineFileName = affineFile;
  image.m_manualAffineMatrix = testManualTransformation();
  project.m_additionalImages.push_back(image);

  REQUIRE(serialize::save(project, projectFile));

  const json saved = json::parse(std::ifstream{projectFile});
  CHECK(saved.at("version").at("major") == 1);
  REQUIRE(saved.contains("images"));
  REQUIRE(saved.at("images").at(1).at("initialAffine").is_object());
  CHECK(saved.at("images").at(1).at("initialAffine").at("path") == "moving-affine.txt");
  CHECK_FALSE(saved.at("images").at(1).at("initialAffine").contains("file"));
  REQUIRE(saved.at("images").at(1).at("manualAffine").is_object());
  REQUIRE(saved.at("images").at(1).at("manualAffine").contains("matrix"));
  CHECK_FALSE(saved.at("images").at(1).contains("affine"));
  CHECK_FALSE(saved.at("images").at(1).contains("manualTransformation"));

  serialize::EntropyProject loaded;
  REQUIRE(serialize::open(loaded, projectFile));
  REQUIRE(loaded.m_additionalImages.size() == 1);
  REQUIRE(loaded.m_additionalImages.at(0).m_initialAffineFileName.has_value());
  CHECK(*loaded.m_additionalImages.at(0).m_initialAffineFileName == fs::canonical(affineFile));
  CHECK_FALSE(loaded.m_additionalImages.at(0).m_manualAffineFileName);
  REQUIRE(loaded.m_additionalImages.at(0).m_manualAffineMatrix.has_value());
  checkMat4(*loaded.m_additionalImages.at(0).m_manualAffineMatrix, *image.m_manualAffineMatrix);
}

TEST_CASE("Project serialization supports embedded and external annotations", "[project][serialization]")
{
  serialize::EntropyProject project;
  project.m_referenceImage.m_imageFileName = "reference.nii.gz";
  project.m_referenceImage.m_annotationsFileName = "reference-annotations.json";
  project.m_referenceImage.m_annotations.assign(2, Annotation{});

  const json root = project;
  const json& reference = root.at("images").at(0);
  REQUIRE(reference.at("annotations").is_object());
  CHECK(reference.at("annotations").at("path") == "reference-annotations.json");
  CHECK_FALSE(reference.contains("annotationsPath"));
  CHECK_FALSE(reference.contains("annotationsFile"));
  REQUIRE(reference.at("annotations").at("embedded").is_object());
  CHECK(reference.at("annotations").at("embedded").at("version").at("major") == 1);
  REQUIRE(reference.at("annotations").at("embedded").at("annotations").is_array());
  CHECK(reference.at("annotations").at("embedded").at("annotations").size() == 2);

  const serialize::EntropyProject parsed = root.get<serialize::EntropyProject>();
  REQUIRE(parsed.m_referenceImage.m_annotationsFileName);
  CHECK(*parsed.m_referenceImage.m_annotationsFileName == fs::path{"reference-annotations.json"});
}

TEST_CASE("Project serialization preserves embedded and path-backed landmark groups", "[project][serialization]")
{
  serialize::EntropyProject project;
  project.m_referenceImage.m_imageFileName = "reference.nii.gz";

  serialize::LandmarkGroup embedded;
  embedded.m_name = "Reference landmarks";
  embedded.m_coordinateSpace = serialize::ProjectLandmarkCoordinateSpace::Subject;
  embedded.m_points = {
    serialize::LandmarkPoint{.m_index = 0, .m_position = glm::vec3{1.0f, 2.0f, 3.0f}, .m_name = "AC"},
    serialize::LandmarkPoint{.m_index = 1, .m_position = glm::vec3{4.0f, 5.0f, 6.0f}, .m_name = "PC"}};
  embedded.m_visible = false;
  embedded.m_opacity = 0.5f;
  embedded.m_color = glm::vec3{0.2f, 0.3f, 0.4f};
  embedded.m_colorOverride = true;
  embedded.m_textColor = glm::vec3{0.9f, 0.8f, 0.7f};
  embedded.m_renderLandmarkIndices = false;
  embedded.m_renderLandmarkNames = true;
  embedded.m_glyphRadiusFactor = 1.25f;
  project.m_referenceImage.m_landmarkGroups.push_back(embedded);

  serialize::LandmarkGroup fileBacked;
  fileBacked.m_csvFileName = "moving-landmarks.csv";
  fileBacked.m_coordinateSpace = serialize::ProjectLandmarkCoordinateSpace::Voxel;
  project.m_referenceImage.m_landmarkGroups.push_back(fileBacked);

  const json root = project;
  const json& savedLandmarks = root.at("images").at(0).at("landmarks");
  REQUIRE(savedLandmarks.size() == 2);
  CHECK_FALSE(savedLandmarks.at(0).contains("path"));
  CHECK_FALSE(savedLandmarks.at(0).contains("coordinateSpace"));
  CHECK(savedLandmarks.at(0).at("display").at("name") == "Reference landmarks");
  CHECK(savedLandmarks.at(0).at("points").size() == 2);
  CHECK_FALSE(savedLandmarks.at(0).at("points").at(0).contains("index"));
  CHECK(savedLandmarks.at(0).at("points").at(0).at("position") == json::array({1.0f, 2.0f, 3.0f}));
  CHECK(savedLandmarks.at(0).at("points").at(0).at("name") == "AC");
  CHECK(savedLandmarks.at(0).at("display").at("visible") == false);
  CHECK(savedLandmarks.at(0).at("display").at("opacity") == 0.5f);
  CHECK(savedLandmarks.at(0).at("display").at("color") == json::array({0.2f, 0.3f, 0.4f}));
  CHECK(savedLandmarks.at(0).at("display").at("colorOverride") == true);
  CHECK(savedLandmarks.at(0).at("display").at("labels").at("textColor") == json::array({0.9f, 0.8f, 0.7f}));
  CHECK(savedLandmarks.at(0).at("display").at("labels").at("showIndices") == false);
  CHECK(savedLandmarks.at(0).at("display").at("labels").at("showNames") == true);
  CHECK(savedLandmarks.at(0).at("display").at("glyphRadiusFactor") == 1.25f);
  CHECK_FALSE(savedLandmarks.at(0).contains("name"));
  CHECK_FALSE(savedLandmarks.at(0).contains("inVoxelSpace"));
  CHECK_FALSE(savedLandmarks.at(0).at("display").contains("textColor"));
  CHECK_FALSE(savedLandmarks.at(0).at("display").contains("showIndices"));
  CHECK_FALSE(savedLandmarks.at(0).at("display").contains("showNames"));
  CHECK_FALSE(savedLandmarks.at(0).at("display").contains("radiusFactor"));
  CHECK(savedLandmarks.at(1).at("path") == "moving-landmarks.csv");
  CHECK(savedLandmarks.at(1).at("coordinateSpace") == "voxel");

  const serialize::EntropyProject parsed = root.get<serialize::EntropyProject>();
  REQUIRE(parsed.m_referenceImage.m_landmarkGroups.size() == 2);
  const serialize::LandmarkGroup& parsedEmbedded = parsed.m_referenceImage.m_landmarkGroups.at(0);
  CHECK_FALSE(parsedEmbedded.m_csvFileName);
  CHECK(parsedEmbedded.m_name == embedded.m_name);
  CHECK(parsedEmbedded.m_points.size() == 2);
  CHECK(parsedEmbedded.m_points.at(0).m_index == 0);
  CHECK(parsedEmbedded.m_points.at(1).m_index == 1);
  CHECK(parsedEmbedded.m_points.at(1).m_name == "PC");
  CHECK(parsedEmbedded.m_textColor == embedded.m_textColor);
  CHECK(parsedEmbedded.m_coordinateSpace == serialize::ProjectLandmarkCoordinateSpace::Subject);
  CHECK(parsedEmbedded.m_glyphRadiusFactor == 1.25f);
  REQUIRE(parsed.m_referenceImage.m_landmarkGroups.at(1).m_csvFileName);
  CHECK(*parsed.m_referenceImage.m_landmarkGroups.at(1).m_csvFileName == fs::path{"moving-landmarks.csv"});
  CHECK(
    parsed.m_referenceImage.m_landmarkGroups.at(1).m_coordinateSpace ==
    serialize::ProjectLandmarkCoordinateSpace::Voxel);
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
    .m_hasBorderColor = true,
    .m_lockedToReference = false,
    .m_warpEnabled = false,
    .m_warpStrength = 2.5f,
    .m_allowExaggeratedWarp = true,
    .m_level = 42.0,
    .m_window = 12.0,
    .m_thresholdLow = 1.0,
    .m_thresholdHigh = 11.0,
    .m_opacity = 0.5,
    .m_activeComponent = 2,
    .m_activeTimePoint = 4,
    .m_timePlaybackLoop = false,
    .m_timePlaybackPlaying = true,
    .m_timePlaybackSpeed = 1.5,
    .m_componentRenderMode = serialize::ProjectComponentRenderMode::ComplexPhase,
    .m_hasComponentRenderMode = true,
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
    .m_hasEdgeColor = true,
    .m_useDistanceMapForRaycasting = false,
    .m_isosurfacesVisible = false,
    .m_applyImageColormapToIsosurfaces = true,
    .m_showIsocontoursIn2D = false,
    .m_isocontourLineWidthIn2D = 3.5,
    .m_isosurfaceOpacityModulator = 0.45f};

  const json root = project;
  const json& settings = root.at("images").at(0).at("settings");
  const json& display = settings.at("display");
  const json& transform = settings.at("transform");
  const json& time = settings.at("time");
  const json& components = settings.at("components");
  const json& vectorRendering = settings.at("vectorRendering");
  const json& vectorArrows = settings.at("vectorArrows");
  const json& warpedGrid = settings.at("warpedGrid");
  const json& edges = settings.at("edges");
  const json& raycasting = settings.at("raycasting");
  const json& isosurfaces = settings.at("isosurfaces");

  CHECK_FALSE(settings.contains("globalVisibility"));
  CHECK_FALSE(settings.contains("vectorArrowOverlayVisible"));
  CHECK_FALSE(settings.contains("vectorWarpedGridVisible"));
  CHECK_FALSE(settings.contains("componentLevels"));
  CHECK_FALSE(settings.contains("colorMapIndices"));
  CHECK_FALSE(settings.contains("foregroundThresholdLows"));
  CHECK_FALSE(settings.contains("showEdges"));
  CHECK_FALSE(settings.contains("isosurfacesVisible"));

  CHECK(display.at("name") == "Image");
  CHECK(display.at("visible") == false);
  CHECK(display.at("opacity") == 0.25);
  CHECK(display.at("borderColor") == json::array({0.4f, 0.5f, 0.6f}));
  CHECK(transform.at("lockedToReference") == false);
  CHECK(transform.at("warpEnabled") == false);
  CHECK(transform.at("warpStrength") == 2.5f);
  CHECK(transform.at("allowExaggeratedWarp") == true);
  CHECK(time.at("activePoint") == 4);
  CHECK(time.at("playbackLoop") == false);
  CHECK(time.at("playbackPlaying") == true);
  CHECK(time.at("playbackSpeed") == 1.5);
  CHECK(components.at("active") == 2);
  CHECK(components.at("renderMode") == "complexPhase");
  CHECK(components.at("complexPhaseUnit") == "degrees");
  CHECK(components.at("complexPhaseRange") == "unsigned");
  CHECK(components.at("ignoreAlpha") == true);
  CHECK(components.at("colorInterpolationMode") == "nearest");
  REQUIRE(components.at("values").size() == 3);
  CHECK(components.at("values").at(0).at("level") == 10.0);
  CHECK(components.at("values").at(0).at("window") == 11.0);
  CHECK(components.at("values").at(0).at("thresholdLow") == 1.0);
  CHECK(components.at("values").at(0).at("thresholdHigh") == 9.0);
  CHECK_FALSE(components.at("values").at(0).contains("opacity"));
  CHECK(components.at("values").at(0).at("colorMapIndex") == 1);
  CHECK_FALSE(components.at("values").at(0).contains("visible"));
  CHECK_FALSE(components.at("values").at(0).contains("colorMapInverted"));
  CHECK_FALSE(components.at("values").at(0).contains("colorMapContinuous"));
  CHECK_FALSE(components.at("values").at(0).contains("colorMapLevels"));
  CHECK(components.at("values").at(0).at("colorMapHsvModifier") == json::array({0.1f, 0.2f, 0.3f}));
  CHECK_FALSE(components.at("values").at(0).contains("interpolationMode"));
  CHECK(components.at("values").at(0).at("foregroundThresholdLow") == 4.0);
  CHECK(components.at("values").at(0).at("foregroundThresholdHigh") == 40.0);
  CHECK(components.at("values").at(1).at("visible") == false);
  CHECK(components.at("values").at(1).at("colorMapHsvModifier") == json::array({0.4f, 0.5f, 0.6f}));
  CHECK(components.at("values").at(1).at("interpolationMode") == "nearest");
  CHECK(components.at("values").at(2).at("opacity") == 0.5);
  CHECK_FALSE(components.at("values").at(2).contains("colorMapHsvModifier"));
  CHECK_FALSE(components.at("values").at(2).contains("interpolationMode"));
  CHECK(vectorRendering.at("planarProjectionSignedColors") == false);
  CHECK(vectorRendering.at("logJacobianDeterminant") == true);
  CHECK(vectorArrows.at("visible") == true);
  CHECK(vectorArrows.at("onImage") == false);
  CHECK(vectorArrows.at("density") == 24.0f);
  CHECK(vectorArrows.at("voxelSpacing") == 2.5f);
  CHECK(vectorArrows.at("millimeterSpacing") == 12.5f);
  CHECK(vectorArrows.at("spacingMode") == "millimeters");
  CHECK(vectorArrows.at("color") == json::array({0.7f, 0.8f, 0.9f}));
  CHECK(vectorArrows.at("useDirectionColor") == true);
  CHECK(vectorArrows.at("lineThickness") == 2.5f);
  CHECK(vectorArrows.at("opacity") == 0.65f);
  CHECK(vectorArrows.at("scaleByMagnitude") == false);
  CHECK(vectorArrows.at("scaleFactor") == 3.0f);
  CHECK(warpedGrid.at("visible") == true);
  CHECK(warpedGrid.at("onImage") == false);
  CHECK(warpedGrid.at("convention") == "apparentDeformation");
  CHECK(warpedGrid.at("pixelSpacing") == 40.0f);
  CHECK(warpedGrid.at("voxelSpacing") == 6.0f);
  CHECK(warpedGrid.at("millimeterSpacing") == 14.0f);
  CHECK_FALSE(warpedGrid.contains("spacingMode"));
  CHECK(warpedGrid.at("lineThickness") == 2.25f);
  CHECK(warpedGrid.at("scaleFactor") == 1.75f);
  CHECK(warpedGrid.at("foregroundColor") == json::array({0.1f, 0.2f, 0.3f, 0.4f}));
  CHECK(warpedGrid.at("backgroundColor") == json::array({0.5f, 0.6f, 0.7f, 0.8f}));
  CHECK(edges.at("method") == "pixel");
  CHECK(edges.at("visible") == true);
  CHECK_FALSE(edges.contains("hardEdges"));
  CHECK_FALSE(edges.contains("thinPixelEdges"));
  CHECK_FALSE(edges.contains("overlay"));
  CHECK_FALSE(edges.contains("useColormap"));
  CHECK(edges.at("magnitude") == 0.33);
  CHECK(edges.at("pixelScale") == 2.5);
  CHECK(edges.at("pixelThreshold") == 0.44);
  CHECK(edges.at("color") == json::array({0.1f, 0.2f, 0.3f}));
  CHECK(edges.at("opacity") == 0.6);
  CHECK(raycasting.at("useDistanceMap") == false);
  CHECK(isosurfaces.at("visible") == false);
  CHECK(isosurfaces.at("applyImageColormap") == true);
  CHECK(isosurfaces.at("showContours2D") == false);
  CHECK(isosurfaces.at("contourLineWidth2D") == 3.5);
  CHECK(isosurfaces.at("opacityModulator") == 0.45f);

  const serialize::EntropyProject parsed = root.get<serialize::EntropyProject>();
  REQUIRE(parsed.m_referenceImage.m_settings.has_value());
  const serialize::ImageSettings& parsedSettings = *parsed.m_referenceImage.m_settings;

  CHECK_FALSE(parsedSettings.m_globalVisibility);
  CHECK(parsedSettings.m_globalOpacity == 0.25);
  CHECK(parsedSettings.m_borderColor == glm::vec3{0.4f, 0.5f, 0.6f});
  CHECK(parsedSettings.m_hasBorderColor);
  CHECK_FALSE(parsedSettings.m_lockedToReference);
  CHECK_FALSE(parsedSettings.m_warpEnabled);
  CHECK(parsedSettings.m_warpStrength == 2.5f);
  CHECK(parsedSettings.m_allowExaggeratedWarp);
  CHECK(parsedSettings.m_componentRenderMode == serialize::ProjectComponentRenderMode::ComplexPhase);
  CHECK(parsedSettings.m_hasComponentRenderMode);
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
  CHECK(parsedSettings.m_level == 30.0);
  CHECK(parsedSettings.m_window == 33.0);
  CHECK(parsedSettings.m_thresholdLow == 3.0);
  CHECK(parsedSettings.m_thresholdHigh == 7.0);
  CHECK(parsedSettings.m_opacity == 0.5);
  CHECK(parsedSettings.m_ignoreAlpha);
  CHECK(parsedSettings.m_colorInterpolationMode == InterpolationMode::NearestNeighbor);
  CHECK(parsedSettings.m_componentLevels == std::vector<double>{10.0, 20.0, 30.0});
  CHECK(parsedSettings.m_componentWindows == std::vector<double>{11.0, 22.0, 33.0});
  CHECK(parsedSettings.m_componentThresholdLows == std::vector<double>{1.0, 2.0, 3.0});
  CHECK(parsedSettings.m_componentThresholdHighs == std::vector<double>{9.0, 8.0, 7.0});
  CHECK(parsedSettings.m_componentVisibility == std::vector<bool>{true, false});
  CHECK(parsedSettings.m_componentOpacities == std::vector<double>{1.0, 0.25, 0.5});
  CHECK(parsedSettings.m_colorMapIndices == std::vector<std::size_t>{1, 2, 3});
  CHECK(parsedSettings.m_colorMapInverted == std::vector<bool>{false, true});
  CHECK(parsedSettings.m_colorMapContinuous == std::vector<bool>{true, false});
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
  CHECK(parsedSettings.m_hasEdgeColor);
  CHECK(parsedSettings.m_edgeOpacity == 0.6);
  CHECK_FALSE(parsedSettings.m_useDistanceMapForRaycasting);
  CHECK_FALSE(parsedSettings.m_isosurfacesVisible);
  CHECK(parsedSettings.m_applyImageColormapToIsosurfaces);
  CHECK_FALSE(parsedSettings.m_showIsocontoursIn2D);
  CHECK(parsedSettings.m_isocontourLineWidthIn2D == 3.5);
  CHECK(parsedSettings.m_isosurfaceOpacityModulator == 0.45f);
}

TEST_CASE("Isosurface serialization preserves rim lighting settings", "[project][serialization][isosurface]")
{
  Isosurface surface;
  surface.name = "Cortex";
  surface.value = 42.0;
  surface.color = glm::vec3{0.1f, 0.2f, 0.3f};
  surface.material.ambient = 0.2f;
  surface.material.diffuse = 0.6f;
  surface.material.specular = 0.3f;
  surface.material.shininess = 12.0f;
  surface.opacity = 0.7f;
  surface.fillOpacity = 0.25f;
  surface.visible = false;
  surface.showIn2d = false;
  surface.rimLightingEnabled = true;
  surface.rimOpacityStrength = 0.8f;
  surface.rimEmissionStrength = 1.25f;
  surface.rimPower = 3.5f;

  const json root = surface;

  CHECK(root.at("contourFillOpacity") == 0.25f);
  CHECK(root.at("showContours2D") == false);
  CHECK(root.at("rimLighting").at("enabled") == true);
  CHECK(root.at("rimLighting").at("opacity") == 0.8f);
  CHECK(root.at("rimLighting").at("glow") == 1.25f);
  CHECK(root.at("rimLighting").at("falloff") == 3.5f);
  CHECK_FALSE(root.contains("fillOpacity"));
  CHECK_FALSE(root.contains("showIn2d"));
  CHECK_FALSE(root.contains("rimLightingEnabled"));
  CHECK_FALSE(root.contains("rimOpacityStrength"));
  CHECK_FALSE(root.contains("rimEmissionStrength"));
  CHECK_FALSE(root.contains("rimPower"));
  CHECK_FALSE(root.contains("edgeStrength"));

  const Isosurface parsed = root.get<Isosurface>();
  CHECK(parsed.name == "Cortex");
  CHECK(parsed.value == 42.0);
  CHECK(parsed.color == glm::vec3{0.1f, 0.2f, 0.3f});
  CHECK(parsed.material.ambient == 0.2f);
  CHECK(parsed.material.diffuse == 0.6f);
  CHECK(parsed.material.specular == 0.3f);
  CHECK(parsed.material.shininess == 12.0f);
  CHECK(parsed.opacity == 0.7f);
  CHECK(parsed.fillOpacity == 0.25f);
  CHECK_FALSE(parsed.visible);
  CHECK_FALSE(parsed.showIn2d);
  CHECK(parsed.rimLightingEnabled);
  CHECK(parsed.rimOpacityStrength == 0.8f);
  CHECK(parsed.rimEmissionStrength == 1.25f);
  CHECK(parsed.rimPower == 3.5f);
}

TEST_CASE("Project serialization preserves image isosurfaces", "[project][serialization][isosurface]")
{
  serialize::EntropyProject project;
  project.m_referenceImage.m_imageFileName = "image.nii.gz";

  serialize::ImageIsosurface imageSurface;
  imageSurface.m_component = 2;
  imageSurface.m_surface.name = "Rim surface";
  imageSurface.m_surface.value = 7.5;
  imageSurface.m_surface.color = glm::vec3{0.25f, 0.5f, 0.75f};
  imageSurface.m_surface.opacity = 0.65f;
  imageSurface.m_surface.fillOpacity = 0.15f;
  imageSurface.m_surface.visible = false;
  imageSurface.m_surface.showIn2d = false;
  imageSurface.m_surface.rimLightingEnabled = true;
  imageSurface.m_surface.rimOpacityStrength = 0.7f;
  imageSurface.m_surface.rimEmissionStrength = 1.4f;
  imageSurface.m_surface.rimPower = 3.0f;
  project.m_referenceImage.m_isosurfaces.push_back(imageSurface);

  const json root = project;
  const json& savedSurface = root.at("images").at(0).at("isosurfaces").at(0);
  CHECK(savedSurface.at("component") == 2);
  CHECK(savedSurface.at("surface").at("name") == "Rim surface");
  CHECK(savedSurface.at("surface").at("contourFillOpacity") == 0.15f);
  CHECK(savedSurface.at("surface").at("showContours2D") == false);
  CHECK(savedSurface.at("surface").at("rimLighting").at("enabled") == true);
  CHECK(savedSurface.at("surface").at("rimLighting").at("opacity") == 0.7f);
  CHECK(savedSurface.at("surface").at("rimLighting").at("glow") == 1.4f);
  CHECK(savedSurface.at("surface").at("rimLighting").at("falloff") == 3.0f);
  CHECK_FALSE(savedSurface.at("surface").contains("fillOpacity"));
  CHECK_FALSE(savedSurface.at("surface").contains("showIn2d"));
  CHECK_FALSE(savedSurface.at("surface").contains("rimLightingEnabled"));

  const serialize::EntropyProject parsed = root.get<serialize::EntropyProject>();
  REQUIRE(parsed.m_referenceImage.m_isosurfaces.size() == 1);
  const serialize::ImageIsosurface& parsedSurface = parsed.m_referenceImage.m_isosurfaces.front();
  CHECK(parsedSurface.m_component == 2);
  CHECK(parsedSurface.m_surface.name == "Rim surface");
  CHECK(parsedSurface.m_surface.value == 7.5);
  CHECK(parsedSurface.m_surface.color == glm::vec3{0.25f, 0.5f, 0.75f});
  CHECK(parsedSurface.m_surface.opacity == 0.65f);
  CHECK(parsedSurface.m_surface.fillOpacity == 0.15f);
  CHECK_FALSE(parsedSurface.m_surface.visible);
  CHECK_FALSE(parsedSurface.m_surface.showIn2d);
  CHECK(parsedSurface.m_surface.rimLightingEnabled);
  CHECK(parsedSurface.m_surface.rimOpacityStrength == 0.7f);
  CHECK(parsedSurface.m_surface.rimEmissionStrength == 1.4f);
  CHECK(parsedSurface.m_surface.rimPower == 3.0f);
}

TEST_CASE("Project serialization preserves inverse and forward warp paths", "[project][serialization]")
{
  const fs::path root = uniqueTempProjectDirectory();
  const fs::path imageFile = root / "moving.nii.gz";
  const fs::path referenceImageFile = root / "fixed.nii.gz";
  const fs::path forwardFile = root / "forward.nrrd";
  const fs::path inverseFile = root / "inverse.nrrd";
  const fs::path projectFile = root / "project.json";

  touchFile(imageFile);
  touchFile(referenceImageFile);
  touchFile(forwardFile);
  touchFile(inverseFile);

  serialize::EntropyProject project;
  project.m_referenceImage.m_imageFileName = imageFile;
  project.m_referenceImage.m_inverseWarpFieldPath = inverseFile;
  project.m_referenceImage.m_inverseWarpReferenceImagePath = referenceImageFile;
  project.m_referenceImage.m_forwardWarpFieldPath = forwardFile;

  const json inlineJson = project;
  CHECK(inlineJson.at("images").at(0).at("inverseWarpField").at("path") == inverseFile.generic_string());
  CHECK_FALSE(inlineJson.at("images").at(0).contains("inverseWarp"));
  CHECK_FALSE(inlineJson.at("images").at(0).contains("inverseWarpReferenceImagePath"));
  CHECK(
    inlineJson.at("images").at(0).at("inverseWarpReferenceImage").at("path") == referenceImageFile.generic_string());
  CHECK(inlineJson.at("images").at(0).at("forwardWarpField").at("path") == forwardFile.generic_string());
  CHECK_FALSE(inlineJson.at("images").at(0).contains("forwardWarp"));

  REQUIRE(serialize::save(project, projectFile));

  const json savedJson = json::parse(std::ifstream(projectFile));
  CHECK(savedJson.at("images").at(0).at("inverseWarpField").at("path") == "inverse.nrrd");
  CHECK_FALSE(savedJson.at("images").at(0).contains("inverseWarp"));
  CHECK_FALSE(savedJson.at("images").at(0).contains("inverseWarpReferenceImagePath"));
  CHECK(savedJson.at("images").at(0).at("inverseWarpReferenceImage").at("path") == "fixed.nii.gz");
  CHECK(savedJson.at("images").at(0).at("forwardWarpField").at("path") == "forward.nrrd");
  CHECK_FALSE(savedJson.at("images").at(0).contains("forwardWarp"));

  serialize::EntropyProject loaded;
  REQUIRE(serialize::open(loaded, projectFile));
  REQUIRE(loaded.m_referenceImage.m_inverseWarpFieldPath);
  REQUIRE(loaded.m_referenceImage.m_inverseWarpReferenceImagePath);
  REQUIRE(loaded.m_referenceImage.m_forwardWarpFieldPath);
  CHECK(*loaded.m_referenceImage.m_inverseWarpFieldPath == fs::canonical(inverseFile));
  CHECK(*loaded.m_referenceImage.m_inverseWarpReferenceImagePath == fs::canonical(referenceImageFile));
  CHECK(*loaded.m_referenceImage.m_forwardWarpFieldPath == fs::canonical(forwardFile));
}

TEST_CASE("Project serialization preserves registration result artifacts", "[project][serialization]")
{
  const fs::path root = uniqueTempProjectDirectory();
  const fs::path fixedImageFile = root / "fixed.nii.gz";
  const fs::path movingImageFile = root / "moving.nii.gz";
  const fs::path manifestFile = root / "registration" / "result.json";
  const fs::path warpedFile = root / "registration" / "warped.nii.gz";
  const fs::path inverseFile = root / "registration" / "inverse.nrrd";
  const fs::path forwardFile = root / "registration" / "forward.nrrd";
  const fs::path affineFile = root / "registration" / "affine.mat";
  const fs::path segFile = root / "registration" / "seg.nii.gz";
  const fs::path surfaceFile = root / "registration" / "surface.vtk";
  const fs::path landmarksFile = root / "registration" / "landmarks.json";
  const fs::path projectFile = root / "project.json";

  touchFile(fixedImageFile);
  touchFile(movingImageFile);
  touchFile(manifestFile);
  touchFile(warpedFile);
  touchFile(inverseFile);
  touchFile(forwardFile);
  touchFile(affineFile);
  touchFile(segFile);
  touchFile(surfaceFile);
  touchFile(landmarksFile);

  serialize::EntropyProject project;
  project.m_referenceImage.m_imageFileName = fixedImageFile;
  project.m_additionalImages.push_back(serialize::Image{.m_imageFileName = movingImageFile});
  project.m_registrationResults.push_back(serialize::RegistrationResult{
    .m_backend = "Greedy",
    .m_fixedImage = fixedImageFile,
    .m_movingImage = movingImageFile,
    .m_manifestFileName = manifestFile,
    .m_warpedImage = warpedFile,
    .m_inverseWarpField = inverseFile,
    .m_forwardWarpField = forwardFile,
    .m_affineTransform = affineFile,
    .m_warpedSegmentations = {segFile},
    .m_transformedSurfaces = {surfaceFile},
    .m_transformedLandmarks = {landmarksFile},
    .m_warnings = {"low overlap"}});

  const json inlineJson = project;
  REQUIRE(inlineJson.contains("registrationResults"));
  CHECK(inlineJson.at("registrationResults").at(0).at("backend") == "Greedy");
  CHECK(inlineJson.at("registrationResults").at(0).at("fixedImage").at("path") == fixedImageFile.generic_string());
  CHECK(inlineJson.at("registrationResults").at(0).at("movingImage").at("path") == movingImageFile.generic_string());
  CHECK(inlineJson.at("registrationResults").at(0).at("manifest").at("path") == manifestFile.generic_string());
  CHECK(inlineJson.at("registrationResults").at(0).at("warnings").at(0) == "low overlap");

  REQUIRE(serialize::save(project, projectFile));

  const json savedJson = json::parse(std::ifstream(projectFile));
  const json& savedResult = savedJson.at("registrationResults").at(0);
  CHECK(savedResult.at("fixedImage").at("path") == "fixed.nii.gz");
  CHECK(savedResult.at("movingImage").at("path") == "moving.nii.gz");
  CHECK_FALSE(savedResult.contains("fixedImageUid"));
  CHECK_FALSE(savedResult.contains("movingImageUid"));
  CHECK(savedResult.at("manifest").at("path") == "registration/result.json");
  CHECK(savedResult.at("warpedImage").at("path") == "registration/warped.nii.gz");
  CHECK(savedResult.at("inverseWarpField").at("path") == "registration/inverse.nrrd");
  CHECK_FALSE(savedResult.contains("inverseWarp"));
  CHECK(savedResult.at("forwardWarpField").at("path") == "registration/forward.nrrd");
  CHECK_FALSE(savedResult.contains("forwardWarp"));
  CHECK(savedResult.at("affine").at("path") == "registration/affine.mat");
  CHECK_FALSE(savedResult.contains("affineTransform"));
  CHECK(savedResult.at("warpedSegmentations").at(0).at("path") == "registration/seg.nii.gz");
  CHECK(savedResult.at("transformedSurfaces").at(0).at("path") == "registration/surface.vtk");
  CHECK(savedResult.at("transformedLandmarks").at(0).at("path") == "registration/landmarks.json");

  serialize::EntropyProject loaded;
  REQUIRE(serialize::open(loaded, projectFile));
  REQUIRE(loaded.m_registrationResults.size() == 1);
  const serialize::RegistrationResult& loadedResult = loaded.m_registrationResults.front();
  REQUIRE(loadedResult.m_fixedImage);
  REQUIRE(loadedResult.m_movingImage);
  REQUIRE(loadedResult.m_manifestFileName);
  REQUIRE(loadedResult.m_warpedImage);
  REQUIRE(loadedResult.m_inverseWarpField);
  REQUIRE(loadedResult.m_forwardWarpField);
  REQUIRE(loadedResult.m_affineTransform);
  CHECK(*loadedResult.m_fixedImage == fs::canonical(fixedImageFile));
  CHECK(*loadedResult.m_movingImage == fs::canonical(movingImageFile));
  CHECK(*loadedResult.m_manifestFileName == fs::canonical(manifestFile));
  CHECK(*loadedResult.m_warpedImage == fs::canonical(warpedFile));
  CHECK(*loadedResult.m_inverseWarpField == fs::canonical(inverseFile));
  CHECK(*loadedResult.m_forwardWarpField == fs::canonical(forwardFile));
  CHECK(*loadedResult.m_affineTransform == fs::canonical(affineFile));
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
      .m_visible = false,
      .m_opacity = 0.35,
      .m_interpolationMode = InterpolationMode::NearestNeighbor,
      .m_labels = serialize::SegmentationLabels{
        .m_count = 300,
        .m_values = {serialize::SegmentationLabel{
          .m_index = 17,
          .m_name = "Hippocampus",
          .m_color = glm::vec4{0.1f, 0.2f, 0.3f, 0.4f},
          .m_visible = false,
          .m_showMesh = true}}}}});

  const json root = project;
  const json& settings = root.at("images").at(0).at("segmentations").at(0).at("settings");

  CHECK(settings.at("display").at("name") == "Seg");
  CHECK(settings.at("display").at("visible") == false);
  CHECK(settings.at("display").at("opacity") == 0.35);
  CHECK_FALSE(settings.contains("labelTableIndex"));
  REQUIRE(settings.at("labels").at("values").size() == 1);
  CHECK(settings.at("labels").at("count") == 300);
  CHECK(settings.at("labels").at("values").at(0).at("index") == 17);
  CHECK(settings.at("labels").at("values").at(0).at("name") == "Hippocampus");
  CHECK(settings.at("labels").at("values").at(0).at("color") == json::array({0.1f, 0.2f, 0.3f, 0.4f}));
  CHECK(settings.at("labels").at("values").at(0).at("visible") == false);
  CHECK(settings.at("labels").at("values").at(0).at("showMesh") == true);
  CHECK_FALSE(settings.contains("interpolationMode"));
  CHECK_FALSE(settings.contains("displayName"));
  CHECK_FALSE(settings.contains("visibility"));
  CHECK_FALSE(settings.contains("activeComponent"));
  CHECK_FALSE(settings.contains("componentVisibility"));
  CHECK_FALSE(settings.contains("componentOpacities"));
  CHECK_FALSE(settings.contains("labelTableIndices"));
  CHECK_FALSE(settings.contains("interpolationModes"));

  const serialize::EntropyProject parsed = root.get<serialize::EntropyProject>();
  REQUIRE(parsed.m_referenceImage.m_segmentations.size() == 1);
  REQUIRE(parsed.m_referenceImage.m_segmentations.front().m_settings.has_value());
  const serialize::SegSettings& parsedSettings = *parsed.m_referenceImage.m_segmentations.front().m_settings;
  CHECK(parsedSettings.m_displayName == "Seg");
  CHECK_FALSE(parsedSettings.m_visible);
  CHECK(parsedSettings.m_opacity == 0.35);
  CHECK(parsedSettings.m_labelTableIndex == 0);
  CHECK(parsedSettings.m_interpolationMode == InterpolationMode::NearestNeighbor);
  REQUIRE(parsedSettings.m_labels.has_value());
  CHECK(parsedSettings.m_labels->m_count == 300);
  REQUIRE(parsedSettings.m_labels->m_values.size() == 1);
  CHECK(parsedSettings.m_labels->m_values.front().m_index == 17);
  CHECK(parsedSettings.m_labels->m_values.front().m_name == "Hippocampus");
  CHECK(parsedSettings.m_labels->m_values.front().m_color == glm::vec4{0.1f, 0.2f, 0.3f, 0.4f});
  CHECK_FALSE(parsedSettings.m_labels->m_values.front().m_visible);
  CHECK(parsedSettings.m_labels->m_values.front().m_showMesh);
}

TEST_CASE("Project serialization preserves standard raster spatial metadata", "[project][serialization]")
{
  serialize::EntropyProject project;
  project.m_referenceImage.m_imageFileName = "image.png";
  ImageSpatialMetadata metadata;
  metadata.spacingMm = glm::vec3{0.5f, 0.25f, 1.0f};
  metadata.originMm = glm::vec3{1.0f, 2.0f, 3.0f};
  metadata.directions =
    glm::mat3{glm::vec3{0.0f, 1.0f, 0.0f}, glm::vec3{0.0f, 0.0f, 1.0f}, glm::vec3{1.0f, 0.0f, 0.0f}};
  project.m_referenceImage.m_spatialMetadata = metadata;

  const json root = project;
  const json& spatialMetadata = root.at("images").at(0).at("spatialMetadata");
  CHECK(spatialMetadata.at("spacing") == json::array({0.5f, 0.25f, 1.0f}));
  CHECK_FALSE(spatialMetadata.contains("spacingMm"));
  CHECK(spatialMetadata.at("origin") == json::array({1.0f, 2.0f, 3.0f}));
  CHECK_FALSE(spatialMetadata.contains("originMm"));
  CHECK(spatialMetadata.at("directions").at(0) == json::array({0.0f, 1.0f, 0.0f}));
  CHECK(spatialMetadata.at("directions").at(1) == json::array({0.0f, 0.0f, 1.0f}));
  CHECK(spatialMetadata.at("directions").at(2) == json::array({1.0f, 0.0f, 0.0f}));

  const serialize::EntropyProject parsed = root.get<serialize::EntropyProject>();
  REQUIRE(parsed.m_referenceImage.m_spatialMetadata.has_value());
  CHECK(parsed.m_referenceImage.m_spatialMetadata->spacingMm == metadata.spacingMm);
  CHECK(parsed.m_referenceImage.m_spatialMetadata->originMm == metadata.originMm);
  CHECK(parsed.m_referenceImage.m_spatialMetadata->directions == metadata.directions);
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
  const json& settings = root.at("images").at(0).at("settings");
  CHECK_FALSE(settings.at("edges").contains("method"));
  CHECK(settings.at("edges").at("useColormap") == true);
  CHECK_FALSE(settings.contains("edgeDetectionMethod"));
  CHECK_FALSE(settings.contains("colormapEdges"));

  const serialize::EntropyProject parsed = root.get<serialize::EntropyProject>();
  REQUIRE(parsed.m_referenceImage.m_settings.has_value());
  CHECK(parsed.m_referenceImage.m_settings->m_edgeDetectionMethod == serialize::ProjectEdgeDetectionMethod::Voxel);
  CHECK(parsed.m_referenceImage.m_settings->m_colormapEdges);
}

TEST_CASE("Project serialization preserves sparse image component setting indices", "[project][serialization]")
{
  const json root = {
    {"version", {{"major", 1}, {"minor", 0}}},
    {"images",
     json::array(
       {{{"path", "image.nii.gz"},
         {"settings",
          {{"components",
            {{"active", 1},
             {"values",
              json::array(
                {json::object(),
                 {{"opacity", 0.25}, {"colorMapIndex", 7}, {"interpolationMode", "nearest"}}})}}}}}}})}};

  const serialize::EntropyProject parsed = root.get<serialize::EntropyProject>();
  REQUIRE(parsed.m_referenceImage.m_settings.has_value());
  const serialize::ImageSettings& settings = *parsed.m_referenceImage.m_settings;

  REQUIRE(settings.m_componentOpacities.size() == 2);
  CHECK(settings.m_componentOpacities.at(0) == 1.0);
  CHECK(settings.m_componentOpacities.at(1) == 0.25);
  CHECK(settings.m_componentOpacityIndices == std::set<std::size_t>{1});
  REQUIRE(settings.m_colorMapIndices.size() == 2);
  CHECK(settings.m_colorMapIndices.at(0) == 0);
  CHECK(settings.m_colorMapIndices.at(1) == 7);
  CHECK(settings.m_colorMapIndexIndices == std::set<std::size_t>{1});
  REQUIRE(settings.m_interpolationModes.size() == 2);
  CHECK(settings.m_interpolationModes.at(1) == InterpolationMode::NearestNeighbor);
  CHECK(settings.m_interpolationModeIndices == std::set<std::size_t>{1});
  CHECK(settings.m_opacity == 0.25);
}

TEST_CASE("Project serialization preserves sparse image component threshold indices", "[project][serialization]")
{
  const json root = {
    {"version", {{"major", 1}, {"minor", 0}}},
    {"images",
     json::array(
       {{{"path", "image.nii.gz"},
         {"settings",
          {{"components",
            {{"values",
              json::array(
                {{{"index", 2},
                  {"level", 77.0},
                  {"window", 12.0},
                  {"thresholdLow", 5.0},
                  {"thresholdHigh", 99.0}}})}}}}}}})}};

  const serialize::EntropyProject parsed = root.get<serialize::EntropyProject>();
  REQUIRE(parsed.m_referenceImage.m_settings.has_value());
  const serialize::ImageSettings& settings = *parsed.m_referenceImage.m_settings;

  REQUIRE(settings.m_componentLevels.size() == 3);
  CHECK(settings.m_componentLevelIndices == std::set<std::size_t>{2});
  CHECK(settings.m_componentWindowIndices == std::set<std::size_t>{2});
  CHECK(settings.m_componentThresholdLowIndices == std::set<std::size_t>{2});
  CHECK(settings.m_componentThresholdHighIndices == std::set<std::size_t>{2});
  CHECK(settings.m_componentLevels.at(2) == 77.0);
  CHECK(settings.m_componentWindows.at(2) == 12.0);
  CHECK(settings.m_componentThresholdLows.at(2) == 5.0);
  CHECK(settings.m_componentThresholdHighs.at(2) == 99.0);
}
