#include "logic/app/UserPreferences.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <nlohmann/json.hpp>

#include <filesystem>

namespace
{

using json = nlohmann::json;

std::filesystem::path tempSettingsFile(const char* name)
{
  const std::filesystem::path directory = std::filesystem::temp_directory_path() / "entropy-user-preferences-tests";
  std::filesystem::create_directories(directory);
  return directory / name;
}

void setNonDefaultSettings(AppSettings& settings)
{
  settings.setSynchronizeZooms(false);
  settings.setCursorSyncEnabled(true);
  settings.setSendCursorSync(false);
  settings.setReceiveCursorSync(false);
  settings.setSendZoomSync(false);
  settings.setReceiveZoomSync(false);
  settings.setSendPanSync(false);
  settings.setReceivePanSync(false);
  settings.setEntropyInstanceSyncEnabled(true);
  settings.setOverlays(false);
  settings.setUiScaleOverride(1.75f);
  settings.setUiFontFamily(UiFontFamily::Cousine);
  settings.setUiColorPreset(UiColorPreset::SoftLight);
  settings.setUiDensityPreset(UiDensityPreset::Comfortable);
  settings.setUiWindowBgOpacity(0.42f);
  settings.setShowLayoutTabs(false);
  settings.setLayoutTabPlacement(UiLayoutTabPlacement::Bottom);
  settings.setShowGlobalTimeControls(false);
  settings.setSynchronizeTimeSeries(false);
  settings.setAutomaticUpdateChecksEnabled(true);
  settings.setReplaceBackgroundWithForeground(true);
  settings.setUse3dBrush(true);
  settings.setUseIsotropicBrush(false);
  settings.setUseVoxelBrushSize(false);
  settings.setUseRoundBrush(false);
  settings.setCrosshairsMoveWithBrush(true);
  settings.setBrushPreviewMode(BrushPreviewMode::Disabled);
  settings.setBrushPreviewVoxels(BrushPreviewVoxels::All);
  settings.setBrushPreviewStyle(BrushPreviewStyle::Outline);
  settings.setBrushPreviewFillOpacity(0.67f);
  settings.setBrushPreviewWhilePainting(false);
  settings.setBrushPreviewOutlineStyle(SegmentationOutlineStyle::ImageVoxel);
  settings.setBrushSizeInVoxels(17);
  settings.setBrushSizeInMm(3.25f);
  settings.setCrosshairsMoveWhileAnnotating(true);
  settings.setLockAnatomicalCoordinateAxesWithReferenceImage(true);

  registration::BackendConfig& registrationConfig = settings.registrationBackendConfig();
  registrationConfig.defaultBackend = registration::Backend::ANTs;
  registrationConfig.greedyExecutable = "/opt/greedy/bin/greedy";
  registrationConfig.antsRegistrationExecutable = "/opt/ants/bin/antsRegistration";
  registrationConfig.antsApplyTransformsExecutable = "/opt/ants/bin/antsApplyTransforms";
  registrationConfig.antsConvertTransformFileExecutable = "/opt/ants/bin/ConvertTransformFile";
  registrationConfig.fireAntsPythonExecutable = "/opt/fireants/bin/python";
  registrationConfig.fireAntsBridgeModule = "entropy_test_fireants_bridge";
  registrationConfig.defaultOutputDirectory = "/tmp/entropy-registration";
  registrationConfig.keepTemporaryFiles = true;
  registrationConfig.maxConcurrentJobs = 3;
  registrationConfig.defaultCpuThreadCount = 7;
  registrationConfig.defaultFireAntsDevice = "cuda:1";
  registrationConfig.showExpertOptionsByDefault = true;
}

user_preferences::RenderPreferences makeNonDefaultRenderPreferences()
{
  user_preferences::RenderPreferences preferences;
  preferences.showImageBorders = false;
  preferences.showImageBordersInLightboxViews = false;
  preferences.crosshairsSnapping = CrosshairsSnapping::ActiveImage;
  preferences.crosshairsColor = {0.1f, 0.2f, 0.3f, 0.4f};
  preferences.showCrosshairs = false;
  preferences.showCrosshairsInLightboxViews = false;
  preferences.background2dColor = {0.2f, 0.3f, 0.4f};
  preferences.background3dColor = {0.3f, 0.4f, 0.5f, 0.6f};
  preferences.anatomicalLabelColor = {0.7f, 0.6f, 0.5f, 0.4f};
  preferences.showAnatomicalLabels = false;
  preferences.showAnatomicalLabelsInLightboxViews = false;
  preferences.anatomicalLabelType = AnatomicalLabelType::Rodent;
  preferences.anatomicalLabelScale = 1.6f;
  preferences.showScaleBars = false;
  preferences.showScaleBarsInLightboxViews = true;
  preferences.scaleBarColor = {0.8f, 0.7f, 0.6f, 0.5f};
  preferences.scaleBarPosition = ScaleBarPosition::Left;
  preferences.scaleBarOrientation = ScaleBarOrientation::Vertical;
  preferences.scaleBarTicks = ScaleBarTicks::Endpoints;
  preferences.scaleBarTargetFraction = 0.45f;
  preferences.scaleBarMarginPx = 44.0f;
  preferences.showLightboxOffsetLabels = true;
  preferences.lightboxOffsetLabelColor = {0.4f, 0.5f, 0.6f, 0.7f};
  preferences.floatingPointLinearInterpolation = true;
  preferences.useMaximumIntensityProjectionExtent = true;
  preferences.intensityProjectionSlabThicknessMm = 12.5f;
  preferences.xrayEnergyKeV = 120.0f;
  preferences.xrayWindow = 0.35f;
  preferences.xrayLevel = 0.65f;
  preferences.isocontourFloatingPointInterpolation = true;
  preferences.modulateIsocontourOpacityWithImageOpacity = false;
  preferences.modulateSegmentationOpacityWithImageOpacity = false;
  preferences.segmentationOutlineStyle = SegmentationOutlineStyle::ImageVoxel;
  preferences.segmentationInteriorOpacity = 0.73f;
  preferences.segmentationErosionFactor = 0.77f;
  preferences.squaredDifference = false;
  preferences.squaredDifferenceMetric.colorMapIndex = 9;
  preferences.squaredDifferenceMetric.slopeIntercept = {2.0f, -1.0f};
  preferences.squaredDifferenceMetric.invertColormap = true;
  preferences.squaredDifferenceMetric.continuousColormap = false;
  preferences.squaredDifferenceMetric.colormapLevels = 11;
  preferences.localNccMetric.colorMapIndex = 8;
  preferences.localNccMetric.slopeIntercept = {1.5f, -0.25f};
  preferences.localNccMetric.invertColormap = true;
  preferences.localNccMetric.continuousColormap = false;
  preferences.localNccMetric.colormapLevels = 12;
  preferences.localNccPatchRadius = 5;
  preferences.localNccSampleSpacing = 2.5f;
  preferences.localNccMinValidFraction = 0.6f;
  preferences.localNccVarianceEpsilon = 0.0025f;
  preferences.localNccIgnoreNegativeCorrelation = false;
  preferences.localNccPresentation = user_preferences::RenderPreferences::LocalNccPresentation::Correlation;
  preferences.localNccInvalidStyle = user_preferences::RenderPreferences::LocalNccInvalidStyle::Gray;
  preferences.localLinearResidualMetric.colorMapIndex = 7;
  preferences.localLinearResidualMetric.slopeIntercept = {3.0f, -0.5f};
  preferences.localLinearResidualMetric.invertColormap = true;
  preferences.localLinearResidualMetric.continuousColormap = false;
  preferences.localLinearResidualMetric.colormapLevels = 13;
  preferences.localLinearResidualPatchRadius = 4;
  preferences.localLinearResidualSampleSpacing = 3.5f;
  preferences.localLinearResidualMinValidFraction = 0.5f;
  preferences.localLinearResidualVarianceEpsilon = 0.0035f;
  preferences.localLinearResidualInvalidStyle = user_preferences::RenderPreferences::LocalNccInvalidStyle::Gray;
  preferences.overlayMagentaCyan = false;
  preferences.quadrants = {false, true};
  preferences.checkerboardSquares = 31;
  preferences.flashlightRadiusFraction = 0.55f;
  preferences.flashlightOverlayMovingImage = false;
  preferences.limitFrameRate = true;
  preferences.targetFrameTimeSeconds = 0.25;
  preferences.raycastSamplingFactor = 1.75f;
  preferences.adaptiveRaycastSamplingEnabled = true;
  preferences.adaptiveRaycastTargetFrameRate = 45.0f;
  preferences.transparentBackgroundWhenNoHit = false;
  preferences.renderFrontFaces = false;
  preferences.renderBackFaces = true;
  preferences.reversePovRotation = true;
  preferences.segmentationMasking = user_preferences::RenderPreferences::SegMaskingForRaycasting::SegMasksOut;
  preferences.asciiEnabled = true;
  preferences.asciiCellSizePx = {10.0f, 20.0f};
  preferences.asciiCharsetIndex = 2;
  preferences.asciiForegroundColor = {0.9f, 0.8f, 0.7f};
  preferences.asciiBackgroundColor = {0.1f, 0.2f, 0.3f};
  preferences.asciiBackgroundAlpha = 0.33f;
  preferences.asciiUseColormapAsForeground = true;
  preferences.asciiSpatialMatching = true;
  preferences.asciiSpatialExponent = 2.5f;
  preferences.annotationsOnTop = true;
  preferences.landmarksOnTop = true;
  preferences.hideAnnotationVertices = true;
  return preferences;
}

user_preferences::PrecisionPreferences makeNonDefaultPrecisionPreferences()
{
  return user_preferences::PrecisionPreferences{
    .imageValuePrecision = 4,
    .coordsPrecision = 5,
    .txPrecision = 6,
    .percentilePrecision = 7,
    .timeValuePrecision = 8};
}

void requireSettingsEqual(const AppSettings& actual, const AppSettings& expected)
{
  CHECK(actual.synchronizeZooms() == expected.synchronizeZooms());
  CHECK(actual.cursorSyncEnabled() == expected.cursorSyncEnabled());
  CHECK(actual.sendCursorSync() == expected.sendCursorSync());
  CHECK(actual.receiveCursorSync() == expected.receiveCursorSync());
  CHECK(actual.sendZoomSync() == expected.sendZoomSync());
  CHECK(actual.receiveZoomSync() == expected.receiveZoomSync());
  CHECK(actual.sendPanSync() == expected.sendPanSync());
  CHECK(actual.receivePanSync() == expected.receivePanSync());
  CHECK(actual.entropyInstanceSyncEnabled() == expected.entropyInstanceSyncEnabled());
  CHECK(actual.overlays() == expected.overlays());
  CHECK(actual.uiScaleOverride() == expected.uiScaleOverride());
  CHECK(actual.uiFontFamily() == expected.uiFontFamily());
  CHECK(actual.uiColorPreset() == expected.uiColorPreset());
  CHECK(actual.uiDensityPreset() == expected.uiDensityPreset());
  CHECK(actual.uiWindowBgOpacity() == Catch::Approx(expected.uiWindowBgOpacity()));
  CHECK(actual.showLayoutTabs() == expected.showLayoutTabs());
  CHECK(actual.layoutTabPlacement() == expected.layoutTabPlacement());
  CHECK(actual.showGlobalTimeControls() == expected.showGlobalTimeControls());
  CHECK(actual.synchronizeTimeSeries() == expected.synchronizeTimeSeries());
  CHECK(actual.automaticUpdateChecksEnabled() == expected.automaticUpdateChecksEnabled());
  CHECK(actual.replaceBackgroundWithForeground() == expected.replaceBackgroundWithForeground());
  CHECK(actual.use3dBrush() == expected.use3dBrush());
  CHECK(actual.useIsotropicBrush() == expected.useIsotropicBrush());
  CHECK(actual.useVoxelBrushSize() == expected.useVoxelBrushSize());
  CHECK(actual.useRoundBrush() == expected.useRoundBrush());
  CHECK(actual.crosshairsMoveWithBrush() == expected.crosshairsMoveWithBrush());
  CHECK(actual.brushPreviewMode() == expected.brushPreviewMode());
  CHECK(actual.brushPreviewVoxels() == expected.brushPreviewVoxels());
  CHECK(actual.brushPreviewStyle() == expected.brushPreviewStyle());
  CHECK(actual.brushPreviewFillOpacity() == Catch::Approx(expected.brushPreviewFillOpacity()));
  CHECK(actual.brushPreviewWhilePainting() == expected.brushPreviewWhilePainting());
  CHECK(actual.brushPreviewOutlineStyle() == expected.brushPreviewOutlineStyle());
  CHECK(actual.brushSizeInVoxels() == expected.brushSizeInVoxels());
  CHECK(actual.brushSizeInMm() == Catch::Approx(expected.brushSizeInMm()));
  CHECK(actual.crosshairsMoveWhileAnnotating() == expected.crosshairsMoveWhileAnnotating());
  CHECK(
    actual.lockAnatomicalCoordinateAxesWithReferenceImage() ==
    expected.lockAnatomicalCoordinateAxesWithReferenceImage());

  const registration::BackendConfig& actualRegistration = actual.registrationBackendConfig();
  const registration::BackendConfig& expectedRegistration = expected.registrationBackendConfig();
  CHECK(actualRegistration.defaultBackend == expectedRegistration.defaultBackend);
  CHECK(actualRegistration.greedyExecutable == expectedRegistration.greedyExecutable);
  CHECK(actualRegistration.antsRegistrationExecutable == expectedRegistration.antsRegistrationExecutable);
  CHECK(actualRegistration.antsApplyTransformsExecutable == expectedRegistration.antsApplyTransformsExecutable);
  CHECK(
    actualRegistration.antsConvertTransformFileExecutable == expectedRegistration.antsConvertTransformFileExecutable);
  CHECK(actualRegistration.fireAntsPythonExecutable == expectedRegistration.fireAntsPythonExecutable);
  CHECK(actualRegistration.fireAntsBridgeModule == expectedRegistration.fireAntsBridgeModule);
  CHECK(actualRegistration.defaultOutputDirectory == expectedRegistration.defaultOutputDirectory);
  CHECK(actualRegistration.keepTemporaryFiles == expectedRegistration.keepTemporaryFiles);
  CHECK(actualRegistration.maxConcurrentJobs == expectedRegistration.maxConcurrentJobs);
  CHECK(actualRegistration.defaultCpuThreadCount == expectedRegistration.defaultCpuThreadCount);
  CHECK(actualRegistration.defaultFireAntsDevice == expectedRegistration.defaultFireAntsDevice);
  CHECK(actualRegistration.showExpertOptionsByDefault == expectedRegistration.showExpertOptionsByDefault);
}

void requireRenderPreferencesEqual(
  const user_preferences::RenderPreferences& actual,
  const user_preferences::RenderPreferences& expected)
{
  CHECK(actual.showImageBorders == expected.showImageBorders);
  CHECK(actual.showImageBordersInLightboxViews == expected.showImageBordersInLightboxViews);
  CHECK(actual.crosshairsSnapping == expected.crosshairsSnapping);
  CHECK(actual.crosshairsColor == expected.crosshairsColor);
  CHECK(actual.showCrosshairs == expected.showCrosshairs);
  CHECK(actual.showCrosshairsInLightboxViews == expected.showCrosshairsInLightboxViews);
  CHECK(actual.background2dColor == expected.background2dColor);
  CHECK(actual.background3dColor == expected.background3dColor);
  CHECK(actual.anatomicalLabelColor == expected.anatomicalLabelColor);
  CHECK(actual.showAnatomicalLabels == expected.showAnatomicalLabels);
  CHECK(actual.showAnatomicalLabelsInLightboxViews == expected.showAnatomicalLabelsInLightboxViews);
  CHECK(actual.anatomicalLabelType == expected.anatomicalLabelType);
  CHECK(actual.anatomicalLabelScale == Catch::Approx(expected.anatomicalLabelScale));
  CHECK(actual.showScaleBars == expected.showScaleBars);
  CHECK(actual.showScaleBarsInLightboxViews == expected.showScaleBarsInLightboxViews);
  CHECK(actual.scaleBarColor == expected.scaleBarColor);
  CHECK(actual.scaleBarPosition == expected.scaleBarPosition);
  CHECK(actual.scaleBarOrientation == expected.scaleBarOrientation);
  CHECK(actual.scaleBarTicks == expected.scaleBarTicks);
  CHECK(actual.scaleBarTargetFraction == Catch::Approx(expected.scaleBarTargetFraction));
  CHECK(actual.scaleBarMarginPx == Catch::Approx(expected.scaleBarMarginPx));
  CHECK(actual.showLightboxOffsetLabels == expected.showLightboxOffsetLabels);
  CHECK(actual.lightboxOffsetLabelColor == expected.lightboxOffsetLabelColor);
  CHECK(actual.floatingPointLinearInterpolation == expected.floatingPointLinearInterpolation);
  CHECK(actual.useMaximumIntensityProjectionExtent == expected.useMaximumIntensityProjectionExtent);
  CHECK(actual.intensityProjectionSlabThicknessMm == Catch::Approx(expected.intensityProjectionSlabThicknessMm));
  CHECK(actual.xrayEnergyKeV == Catch::Approx(expected.xrayEnergyKeV));
  CHECK(actual.xrayWindow == Catch::Approx(expected.xrayWindow));
  CHECK(actual.xrayLevel == Catch::Approx(expected.xrayLevel));
  CHECK(actual.isocontourFloatingPointInterpolation == expected.isocontourFloatingPointInterpolation);
  CHECK(actual.modulateIsocontourOpacityWithImageOpacity == expected.modulateIsocontourOpacityWithImageOpacity);
  CHECK(actual.modulateSegmentationOpacityWithImageOpacity == expected.modulateSegmentationOpacityWithImageOpacity);
  CHECK(actual.segmentationOutlineStyle == expected.segmentationOutlineStyle);
  CHECK(actual.segmentationInteriorOpacity == Catch::Approx(expected.segmentationInteriorOpacity));
  CHECK(actual.segmentationErosionFactor == Catch::Approx(expected.segmentationErosionFactor));
  CHECK(actual.squaredDifference == expected.squaredDifference);
  CHECK(actual.squaredDifferenceMetric.colorMapIndex == expected.squaredDifferenceMetric.colorMapIndex);
  CHECK(actual.squaredDifferenceMetric.slopeIntercept == expected.squaredDifferenceMetric.slopeIntercept);
  CHECK(actual.squaredDifferenceMetric.invertColormap == expected.squaredDifferenceMetric.invertColormap);
  CHECK(actual.squaredDifferenceMetric.continuousColormap == expected.squaredDifferenceMetric.continuousColormap);
  CHECK(actual.squaredDifferenceMetric.colormapLevels == expected.squaredDifferenceMetric.colormapLevels);
  CHECK(actual.localNccMetric.colorMapIndex == expected.localNccMetric.colorMapIndex);
  CHECK(actual.localNccMetric.slopeIntercept == expected.localNccMetric.slopeIntercept);
  CHECK(actual.localNccMetric.invertColormap == expected.localNccMetric.invertColormap);
  CHECK(actual.localNccMetric.continuousColormap == expected.localNccMetric.continuousColormap);
  CHECK(actual.localNccMetric.colormapLevels == expected.localNccMetric.colormapLevels);
  CHECK(actual.localNccPatchRadius == expected.localNccPatchRadius);
  CHECK(actual.localNccSampleSpacing == Catch::Approx(expected.localNccSampleSpacing));
  CHECK(actual.localNccMinValidFraction == Catch::Approx(expected.localNccMinValidFraction));
  CHECK(actual.localNccVarianceEpsilon == Catch::Approx(expected.localNccVarianceEpsilon));
  CHECK(actual.localNccIgnoreNegativeCorrelation == expected.localNccIgnoreNegativeCorrelation);
  CHECK(actual.localNccPresentation == expected.localNccPresentation);
  CHECK(actual.localNccInvalidStyle == expected.localNccInvalidStyle);
  CHECK(actual.localLinearResidualMetric.colorMapIndex == expected.localLinearResidualMetric.colorMapIndex);
  CHECK(actual.localLinearResidualMetric.slopeIntercept == expected.localLinearResidualMetric.slopeIntercept);
  CHECK(actual.localLinearResidualMetric.invertColormap == expected.localLinearResidualMetric.invertColormap);
  CHECK(actual.localLinearResidualMetric.continuousColormap == expected.localLinearResidualMetric.continuousColormap);
  CHECK(actual.localLinearResidualMetric.colormapLevels == expected.localLinearResidualMetric.colormapLevels);
  CHECK(actual.localLinearResidualPatchRadius == expected.localLinearResidualPatchRadius);
  CHECK(actual.localLinearResidualSampleSpacing == Catch::Approx(expected.localLinearResidualSampleSpacing));
  CHECK(actual.localLinearResidualMinValidFraction == Catch::Approx(expected.localLinearResidualMinValidFraction));
  CHECK(actual.localLinearResidualVarianceEpsilon == Catch::Approx(expected.localLinearResidualVarianceEpsilon));
  CHECK(actual.localLinearResidualInvalidStyle == expected.localLinearResidualInvalidStyle);
  CHECK(actual.overlayMagentaCyan == expected.overlayMagentaCyan);
  CHECK(actual.quadrants == expected.quadrants);
  CHECK(actual.checkerboardSquares == expected.checkerboardSquares);
  CHECK(actual.flashlightRadiusFraction == Catch::Approx(expected.flashlightRadiusFraction));
  CHECK(actual.flashlightOverlayMovingImage == expected.flashlightOverlayMovingImage);
  CHECK(actual.limitFrameRate == expected.limitFrameRate);
  CHECK(actual.targetFrameTimeSeconds == Catch::Approx(expected.targetFrameTimeSeconds));
  CHECK(actual.raycastSamplingFactor == Catch::Approx(expected.raycastSamplingFactor));
  CHECK(actual.adaptiveRaycastSamplingEnabled == expected.adaptiveRaycastSamplingEnabled);
  CHECK(actual.adaptiveRaycastTargetFrameRate == Catch::Approx(expected.adaptiveRaycastTargetFrameRate));
  CHECK(actual.transparentBackgroundWhenNoHit == expected.transparentBackgroundWhenNoHit);
  CHECK(actual.renderFrontFaces == expected.renderFrontFaces);
  CHECK(actual.renderBackFaces == expected.renderBackFaces);
  CHECK(actual.reversePovRotation == expected.reversePovRotation);
  CHECK(actual.segmentationMasking == expected.segmentationMasking);
  CHECK(actual.asciiEnabled == expected.asciiEnabled);
  CHECK(actual.asciiCellSizePx == expected.asciiCellSizePx);
  CHECK(actual.asciiCharsetIndex == expected.asciiCharsetIndex);
  CHECK(actual.asciiForegroundColor == expected.asciiForegroundColor);
  CHECK(actual.asciiBackgroundColor == expected.asciiBackgroundColor);
  CHECK(actual.asciiBackgroundAlpha == Catch::Approx(expected.asciiBackgroundAlpha));
  CHECK(actual.asciiUseColormapAsForeground == expected.asciiUseColormapAsForeground);
  CHECK(actual.asciiSpatialMatching == expected.asciiSpatialMatching);
  CHECK(actual.asciiSpatialExponent == Catch::Approx(expected.asciiSpatialExponent));
  CHECK(actual.annotationsOnTop == expected.annotationsOnTop);
  CHECK(actual.landmarksOnTop == expected.landmarksOnTop);
  CHECK(actual.hideAnnotationVertices == expected.hideAnnotationVertices);
}

void requirePrecisionPreferencesEqual(
  const user_preferences::PrecisionPreferences& actual,
  const user_preferences::PrecisionPreferences& expected)
{
  CHECK(actual.imageValuePrecision == expected.imageValuePrecision);
  CHECK(actual.coordsPrecision == expected.coordsPrecision);
  CHECK(actual.txPrecision == expected.txPrecision);
  CHECK(actual.percentilePrecision == expected.percentilePrecision);
  CHECK(actual.timeValuePrecision == expected.timeValuePrecision);
}

void resetProjectOwnedSettings(AppSettings& settings, user_preferences::RenderPreferences& renderPreferences)
{
  settings.setLockAnatomicalCoordinateAxesWithReferenceImage(false);
  settings.setSynchronizeTimeSeries(true);
  renderPreferences.crosshairsSnapping = CrosshairsSnapping::Disabled;
  renderPreferences.showAnatomicalLabels = true;
  renderPreferences.showAnatomicalLabelsInLightboxViews = true;
  renderPreferences.anatomicalLabelType = AnatomicalLabelType::Human;

  const user_preferences::RenderPreferences defaults;
  renderPreferences.squaredDifference = defaults.squaredDifference;
  renderPreferences.squaredDifferenceMetric = defaults.squaredDifferenceMetric;
  renderPreferences.localNccMetric = defaults.localNccMetric;
  renderPreferences.localNccPatchRadius = defaults.localNccPatchRadius;
  renderPreferences.localNccSampleSpacing = defaults.localNccSampleSpacing;
  renderPreferences.localNccMinValidFraction = defaults.localNccMinValidFraction;
  renderPreferences.localNccVarianceEpsilon = defaults.localNccVarianceEpsilon;
  renderPreferences.localNccIgnoreNegativeCorrelation = defaults.localNccIgnoreNegativeCorrelation;
  renderPreferences.localNccPresentation = defaults.localNccPresentation;
  renderPreferences.localNccInvalidStyle = defaults.localNccInvalidStyle;
  renderPreferences.localLinearResidualMetric = defaults.localLinearResidualMetric;
  renderPreferences.localLinearResidualPatchRadius = defaults.localLinearResidualPatchRadius;
  renderPreferences.localLinearResidualSampleSpacing = defaults.localLinearResidualSampleSpacing;
  renderPreferences.localLinearResidualMinValidFraction = defaults.localLinearResidualMinValidFraction;
  renderPreferences.localLinearResidualVarianceEpsilon = defaults.localLinearResidualVarianceEpsilon;
  renderPreferences.localLinearResidualInvalidStyle = defaults.localLinearResidualInvalidStyle;
  renderPreferences.overlayMagentaCyan = defaults.overlayMagentaCyan;
  renderPreferences.quadrants = defaults.quadrants;
  renderPreferences.checkerboardSquares = defaults.checkerboardSquares;
  renderPreferences.flashlightRadiusFraction = defaults.flashlightRadiusFraction;
  renderPreferences.flashlightOverlayMovingImage = defaults.flashlightOverlayMovingImage;
  renderPreferences.useMaximumIntensityProjectionExtent = defaults.useMaximumIntensityProjectionExtent;
  renderPreferences.intensityProjectionSlabThicknessMm = defaults.intensityProjectionSlabThicknessMm;
  renderPreferences.xrayEnergyKeV = defaults.xrayEnergyKeV;
  renderPreferences.xrayWindow = defaults.xrayWindow;
  renderPreferences.xrayLevel = defaults.xrayLevel;
  renderPreferences.raycastSamplingFactor = defaults.raycastSamplingFactor;
  renderPreferences.adaptiveRaycastSamplingEnabled = defaults.adaptiveRaycastSamplingEnabled;
  renderPreferences.adaptiveRaycastTargetFrameRate = defaults.adaptiveRaycastTargetFrameRate;
  renderPreferences.transparentBackgroundWhenNoHit = defaults.transparentBackgroundWhenNoHit;
  renderPreferences.renderFrontFaces = defaults.renderFrontFaces;
  renderPreferences.renderBackFaces = defaults.renderBackFaces;
  renderPreferences.segmentationMasking = defaults.segmentationMasking;
  renderPreferences.modulateSegmentationOpacityWithImageOpacity = defaults.modulateSegmentationOpacityWithImageOpacity;
  renderPreferences.segmentationOutlineStyle = defaults.segmentationOutlineStyle;
  renderPreferences.segmentationInteriorOpacity = defaults.segmentationInteriorOpacity;
  renderPreferences.segmentationErosionFactor = defaults.segmentationErosionFactor;
  renderPreferences.isocontourFloatingPointInterpolation = defaults.isocontourFloatingPointInterpolation;
  renderPreferences.modulateIsocontourOpacityWithImageOpacity = defaults.modulateIsocontourOpacityWithImageOpacity;
  renderPreferences.annotationsOnTop = defaults.annotationsOnTop;
  renderPreferences.landmarksOnTop = defaults.landmarksOnTop;
  renderPreferences.hideAnnotationVertices = defaults.hideAnnotationVertices;
}

} // namespace

TEST_CASE("user preferences round-trip every persisted application and rendering setting", "[app][settings]")
{
  AppSettings expectedSettings;
  setNonDefaultSettings(expectedSettings);
  user_preferences::RenderPreferences expectedRenderPreferences = makeNonDefaultRenderPreferences();
  user_preferences::PrecisionPreferences expectedPrecisionPreferences = makeNonDefaultPrecisionPreferences();

  const std::string text =
    user_preferences::toJsonString(expectedSettings, expectedRenderPreferences, expectedPrecisionPreferences);
  resetProjectOwnedSettings(expectedSettings, expectedRenderPreferences);

  AppSettings actualSettings;
  user_preferences::RenderPreferences actualRenderPreferences;
  user_preferences::PrecisionPreferences actualPrecisionPreferences;
  REQUIRE(user_preferences::applyJsonString(actualSettings, actualRenderPreferences, actualPrecisionPreferences, text));

  requireSettingsEqual(actualSettings, expectedSettings);
  requireRenderPreferencesEqual(actualRenderPreferences, expectedRenderPreferences);
  requirePrecisionPreferencesEqual(actualPrecisionPreferences, expectedPrecisionPreferences);

  const json root = json::parse(text);
  CHECK(root.at("format") == "entropy.userSettings");
  CHECK(root.at("version") == 1);
  CHECK(root.at("interface").at("precision").at("imageValues") == 4);
  CHECK(root.at("interface").at("precision").at("coordinates") == 5);
  CHECK(root.at("interface").at("precision").at("transformations") == 6);
  CHECK(root.at("interface").at("precision").at("percentiles") == 7);
  CHECK(root.at("interface").at("precision").at("timeValues") == 8);
  CHECK(root.at("views").at("showOverlays") == false);
  CHECK(root.at("registration").at("defaultBackend") == "ANTs");
  CHECK(root.at("registration").at("greedyExecutable") == "/opt/greedy/bin/greedy");
  CHECK(root.at("registration").at("antsRegistrationExecutable") == "/opt/ants/bin/antsRegistration");
  CHECK(root.at("registration").at("antsApplyTransformsExecutable") == "/opt/ants/bin/antsApplyTransforms");
  CHECK(root.at("registration").at("antsConvertTransformFileExecutable") == "/opt/ants/bin/ConvertTransformFile");
  CHECK(root.at("registration").at("fireAntsPythonExecutable") == "/opt/fireants/bin/python");
  CHECK(root.at("registration").at("fireAntsBridgeModule") == "entropy_test_fireants_bridge");
  CHECK(root.at("registration").at("defaultOutputDirectory") == "/tmp/entropy-registration");
  CHECK(root.at("registration").at("keepTemporaryFiles") == true);
  CHECK(root.at("registration").at("maxConcurrentJobs") == 3);
  CHECK(root.at("registration").at("defaultCpuThreadCount") == 7);
  CHECK(root.at("registration").at("defaultFireAntsDevice") == "cuda:1");
  CHECK(root.at("registration").at("showExpertOptionsByDefault") == true);
  CHECK_FALSE(root.at("synchronization").contains("timeSeries"));
  CHECK(root.at("synchronization").at("entropyInstances").at("enabled") == true);
  CHECK(root.at("system").at("updates").at("automaticChecks") == true);
}

TEST_CASE("user preferences file load treats a missing file as defaults-preserving success", "[app][settings]")
{
  AppSettings settings;
  setNonDefaultSettings(settings);
  user_preferences::RenderPreferences renderPreferences = makeNonDefaultRenderPreferences();
  const AppSettings expectedSettings = settings;
  const user_preferences::RenderPreferences expectedRenderPreferences = renderPreferences;

  std::string error = "unchanged";
  const std::filesystem::path fileName = tempSettingsFile("missing-settings.json");
  std::filesystem::remove(fileName);

  REQUIRE(user_preferences::load(settings, renderPreferences, fileName, &error));
  CHECK(error == "unchanged");
  requireSettingsEqual(settings, expectedSettings);
  requireRenderPreferencesEqual(renderPreferences, expectedRenderPreferences);
}

TEST_CASE("user preferences save creates parent directories and load restores them", "[app][settings]")
{
  AppSettings expectedSettings;
  setNonDefaultSettings(expectedSettings);
  user_preferences::RenderPreferences expectedRenderPreferences = makeNonDefaultRenderPreferences();
  user_preferences::PrecisionPreferences expectedPrecisionPreferences = makeNonDefaultPrecisionPreferences();
  const std::filesystem::path fileName = tempSettingsFile("nested/settings.json");
  std::filesystem::remove(fileName);

  REQUIRE(user_preferences::save(expectedSettings, expectedRenderPreferences, expectedPrecisionPreferences, fileName));
  resetProjectOwnedSettings(expectedSettings, expectedRenderPreferences);

  AppSettings actualSettings;
  user_preferences::RenderPreferences actualRenderPreferences;
  user_preferences::PrecisionPreferences actualPrecisionPreferences;
  REQUIRE(user_preferences::load(actualSettings, actualRenderPreferences, actualPrecisionPreferences, fileName));

  requireSettingsEqual(actualSettings, expectedSettings);
  requireRenderPreferencesEqual(actualRenderPreferences, expectedRenderPreferences);
  requirePrecisionPreferencesEqual(actualPrecisionPreferences, expectedPrecisionPreferences);
}

TEST_CASE("application render preferences ignore project-owned presentation settings", "[app][settings]")
{
  user_preferences::RenderPreferences preferences;
  preferences.raycastSamplingFactor = 0.25f;
  preferences.adaptiveRaycastSamplingEnabled = true;
  preferences.adaptiveRaycastTargetFrameRate = 45.0f;
  preferences.showCrosshairs = false;
  preferences.showCrosshairsInLightboxViews = false;
  preferences.showImageBorders = false;
  preferences.asciiEnabled = true;
  preferences.reversePovRotation = true;

  const user_preferences::RenderPreferences appPreferences =
    user_preferences::applicationRenderPreferences(preferences);

  CHECK(appPreferences.raycastSamplingFactor == user_preferences::RenderPreferences{}.raycastSamplingFactor);
  CHECK(
    appPreferences.adaptiveRaycastSamplingEnabled ==
    user_preferences::RenderPreferences{}.adaptiveRaycastSamplingEnabled);
  CHECK(
    appPreferences.adaptiveRaycastTargetFrameRate ==
    Catch::Approx(user_preferences::RenderPreferences{}.adaptiveRaycastTargetFrameRate));
  CHECK(appPreferences.showCrosshairs == user_preferences::RenderPreferences{}.showCrosshairs);
  CHECK(
    appPreferences.showCrosshairsInLightboxViews ==
    user_preferences::RenderPreferences{}.showCrosshairsInLightboxViews);
  CHECK(appPreferences.showImageBorders == user_preferences::RenderPreferences{}.showImageBorders);
  CHECK(appPreferences.asciiEnabled == true);
  CHECK(appPreferences.reversePovRotation == true);
}

TEST_CASE("user preferences reject invalid JSON without mutating existing values", "[app][settings]")
{
  AppSettings settings;
  setNonDefaultSettings(settings);
  user_preferences::RenderPreferences renderPreferences = makeNonDefaultRenderPreferences();
  user_preferences::PrecisionPreferences precisionPreferences = makeNonDefaultPrecisionPreferences();
  const AppSettings expectedSettings = settings;
  const user_preferences::RenderPreferences expectedRenderPreferences = renderPreferences;
  const user_preferences::PrecisionPreferences expectedPrecisionPreferences = precisionPreferences;

  std::string error;
  REQUIRE_FALSE(
    user_preferences::applyJsonString(settings, renderPreferences, precisionPreferences, "{ not json", &error));
  CHECK_FALSE(error.empty());
  requireSettingsEqual(settings, expectedSettings);
  requireRenderPreferencesEqual(renderPreferences, expectedRenderPreferences);
  requirePrecisionPreferencesEqual(precisionPreferences, expectedPrecisionPreferences);
}

TEST_CASE("user preferences preserve defaults for missing invalid and legacy fields", "[app][settings]")
{
  AppSettings settings;
  user_preferences::RenderPreferences renderPreferences;
  user_preferences::PrecisionPreferences precisionPreferences;

  const std::string text = R"({
    "interface": {
      "uiScale": 99,
      "font": "notAFont",
      "windowBackgroundOpacity": 0.01,
      "showLayoutTabs": "bad",
      "layoutTabsPosition": "side",
      "precision": {
        "imageValues": 99,
        "coordinates": "bad",
        "transformations": 6,
        "percentiles": 8,
        "timeValues": 99
      }
    },
    "views": {
      "crosshairs": {
        "color": [1, "bad", 3, 4],
        "snapping": "activeImage"
      },
      "anatomicalLabels": {
        "scale": 99
      },
      "scaleBars": {
        "targetLengthFraction": 10,
        "marginPixels": 1
      }
    },
    "segmentation": {
      "brush": {
        "sizeVoxels": 9999
      },
      "brushPreview": {
        "fillOpacity": -1
      }
    },
    "comparison": {
      "localNormalizedCrossCorrelation": {
        "patchRadius": 99,
        "sampleSpacing": 0,
        "minimumValidFraction": 4,
        "varianceEpsilon": 2.5,
        "presentation": "bad",
        "invalidStyle": "bad"
      },
      "localLinearResidual": {
        "patchRadius": 99,
        "sampleSpacing": 0,
        "minimumValidFraction": 4,
        "varianceEpsilon": 3.5,
        "invalidStyle": "bad"
      },
      "checkerboard": {
        "squares": 1
      }
    },
    "rendering": {
      "frameRate": {
        "targetFrameTimeSeconds": 100
      },
      "raycasting": {
        "samplingFactor": 0
      },
      "asciiShading": {
        "charsetIndex": 99,
        "backgroundAlpha": 3,
        "spatialExponent": 99
      }
    },
    "synchronization": {
      "enabled": true,
      "sendCursor": false
    }
  })";

  REQUIRE(user_preferences::applyJsonString(settings, renderPreferences, precisionPreferences, text));

  REQUIRE(settings.uiScaleOverride());
  CHECK(*settings.uiScaleOverride() == Catch::Approx(4.0f));
  CHECK(settings.uiFontFamily() == UiFontFamily::Inter);
  CHECK(settings.uiWindowBgOpacity() == Catch::Approx(0.2f));
  CHECK(precisionPreferences.imageValuePrecision == 9);
  CHECK(precisionPreferences.coordsPrecision == 3);
  CHECK(precisionPreferences.txPrecision == 6);
  CHECK(precisionPreferences.percentilePrecision == 8);
  CHECK(precisionPreferences.timeValuePrecision == 9);
  CHECK(renderPreferences.crosshairsColor == user_preferences::RenderPreferences{}.crosshairsColor);
  CHECK(renderPreferences.crosshairsSnapping == user_preferences::RenderPreferences{}.crosshairsSnapping);
  CHECK(renderPreferences.anatomicalLabelScale == Catch::Approx(2.0f));
  CHECK(renderPreferences.scaleBarTargetFraction == Catch::Approx(1.0f));
  CHECK(renderPreferences.scaleBarMarginPx == Catch::Approx(12.0f));
  CHECK(settings.brushSizeInVoxels() == 511);
  CHECK(settings.brushPreviewFillOpacity() == Catch::Approx(0.0f));
  CHECK(renderPreferences.localNccPatchRadius == user_preferences::RenderPreferences{}.localNccPatchRadius);
  CHECK(
    renderPreferences.localNccSampleSpacing ==
    Catch::Approx(user_preferences::RenderPreferences{}.localNccSampleSpacing));
  CHECK(
    renderPreferences.localNccMinValidFraction ==
    Catch::Approx(user_preferences::RenderPreferences{}.localNccMinValidFraction));
  CHECK(
    renderPreferences.localNccVarianceEpsilon ==
    Catch::Approx(user_preferences::RenderPreferences{}.localNccVarianceEpsilon));
  CHECK(renderPreferences.localNccPresentation == user_preferences::RenderPreferences{}.localNccPresentation);
  CHECK(renderPreferences.localNccInvalidStyle == user_preferences::RenderPreferences{}.localNccInvalidStyle);
  CHECK(
    renderPreferences.localLinearResidualPatchRadius ==
    user_preferences::RenderPreferences{}.localLinearResidualPatchRadius);
  CHECK(
    renderPreferences.localLinearResidualSampleSpacing ==
    Catch::Approx(user_preferences::RenderPreferences{}.localLinearResidualSampleSpacing));
  CHECK(
    renderPreferences.localLinearResidualMinValidFraction ==
    Catch::Approx(user_preferences::RenderPreferences{}.localLinearResidualMinValidFraction));
  CHECK(
    renderPreferences.localLinearResidualVarianceEpsilon ==
    Catch::Approx(user_preferences::RenderPreferences{}.localLinearResidualVarianceEpsilon));
  CHECK(
    renderPreferences.localLinearResidualInvalidStyle ==
    user_preferences::RenderPreferences{}.localLinearResidualInvalidStyle);
  CHECK(renderPreferences.checkerboardSquares == user_preferences::RenderPreferences{}.checkerboardSquares);
  CHECK(renderPreferences.targetFrameTimeSeconds == Catch::Approx(1.0));
  CHECK(
    renderPreferences.raycastSamplingFactor ==
    Catch::Approx(user_preferences::RenderPreferences{}.raycastSamplingFactor));
  CHECK(renderPreferences.asciiCharsetIndex == 2);
  CHECK(renderPreferences.asciiBackgroundAlpha == Catch::Approx(1.0f));
  CHECK(renderPreferences.asciiSpatialExponent == Catch::Approx(4.0f));
  CHECK(settings.cursorSyncEnabled());
  CHECK_FALSE(settings.sendCursorSync());
}

TEST_CASE("default user preference JSON documents built-in defaults", "[app][settings]")
{
  const AppSettings settings;
  const user_preferences::RenderPreferences renderPreferences = user_preferences::defaultRenderPreferences();

  const json root = json::parse(user_preferences::toJsonString(settings, renderPreferences));

  CHECK(root.at("interface").at("uiScale") == "auto");
  CHECK(root.at("interface").at("font") == "inter");
  CHECK(root.at("interface").at("colorScheme") == "entropyDark");
  CHECK(root.at("interface").at("showLayoutTabs") == true);
  CHECK(root.at("interface").at("layoutTabsPosition") == "top");
  CHECK(root.at("interface").at("showGlobalTimeControls") == true);
  CHECK(root.at("interface").at("precision").at("imageValues") == 3);
  CHECK(root.at("interface").at("precision").at("coordinates") == 3);
  CHECK(root.at("interface").at("precision").at("transformations") == 3);
  CHECK(root.at("interface").at("precision").at("percentiles") == 2);
  CHECK(root.at("interface").at("precision").at("timeValues") == 2);
  CHECK(root.at("views").at("showOverlays") == true);
  CHECK(root.at("views").at("showImageBorders") == true);
  CHECK(root.at("views").at("lightbox").at("showImageBorders") == false);
  CHECK(root.at("views").at("crosshairs").at("show") == true);
  CHECK(root.at("views").at("crosshairs").at("showInLightboxViews") == true);
  CHECK_FALSE(root.at("views").at("crosshairs").contains("snapping"));
  CHECK_FALSE(root.at("views").contains("lockAnatomicalDirectionsToReferenceImage"));
  CHECK_FALSE(root.at("views").at("anatomicalLabels").contains("type"));
  CHECK(root.at("views").at("anatomicalLabels").at("scale").get<float>() == Catch::Approx(1.0f));
  CHECK(root.at("views").at("scaleBars").at("show") == true);
  CHECK(root.at("views").at("lightbox").at("showOffsetLabels") == true);
  CHECK_FALSE(root.contains("comparison"));
  CHECK_FALSE(root.at("images").contains("intensityProjectionDefaults"));
  CHECK_FALSE(root.at("segmentation").contains("display"));
  CHECK_FALSE(root.at("rendering").contains("raycasting"));
  CHECK_FALSE(root.at("rendering").contains("isosurfaces"));
  CHECK_FALSE(root.at("annotations").contains("annotationsOnTop"));
  CHECK_FALSE(root.at("annotations").contains("landmarksOnTop"));
  CHECK_FALSE(root.at("annotations").contains("hideAnnotationVertices"));
  CHECK(root.at("segmentation").at("brushPreview").at("mode") == "hover");
  CHECK_FALSE(root.at("synchronization").contains("timeSeries"));
  CHECK(root.at("synchronization").at("itkSnap").at("enabled") == false);
  CHECK(root.at("synchronization").at("entropyInstances").at("enabled") == false);
  CHECK(root.at("system").at("updates").at("automaticChecks") == false);
}
