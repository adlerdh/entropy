#include "image/ImageSettings.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <limits>
#include <vector>

namespace
{

ComponentStats
makeStats(long double minValue, long double q1, long double median, long double q99, long double maxValue)
{
  ComponentStats stats;
  stats.onlineStats.min = minValue;
  stats.onlineStats.max = maxValue;
  stats.onlineStats.mean = 0.5L * (minValue + maxValue);
  stats.onlineStats.stdev = 2.0L;
  stats.onlineStats.variance = 4.0L;
  stats.onlineStats.sum = 0.0L;
  stats.onlineStats.count = 9;
  stats.quantiles.fill(minValue);
  stats.quantiles[1] = q1;
  stats.quantiles[25] = 0.5L * (q1 + median);
  stats.quantiles[50] = median;
  stats.quantiles[75] = 0.5L * (median + q99);
  stats.quantiles[99] = q99;
  stats.quantiles[100] = maxValue;
  return stats;
}

ComponentStats makeSparseBinaryStats()
{
  ComponentStats stats;
  stats.onlineStats.min = 0.0L;
  stats.onlineStats.max = 100.0L;
  stats.onlineStats.mean = 2.0L;
  stats.onlineStats.stdev = 14.0L;
  stats.onlineStats.variance = 196.0L;
  stats.onlineStats.sum = 200.0L;
  stats.onlineStats.count = 100;
  stats.quantiles.fill(0.0L);
  stats.quantiles[100] = 100.0L;
  return stats;
}

ImageSettings makeSettings()
{
  return ImageSettings(
    "settings",
    9,
    2,
    ComponentType::UInt16,
    {makeStats(0.0, 10.0, 50.0, 90.0, 100.0), makeStats(-20.0, -10.0, 0.0, 10.0, 20.0)});
}

} // namespace

TEST_CASE("ImageSettings rejects invalid construction arguments", "[image][settings]")
{
  CHECK_THROWS(ImageSettings("zero-pixels", 0, 1, ComponentType::UInt16, {makeStats(0.0, 0.0, 0.0, 0.0, 0.0)}));
  CHECK_THROWS(ImageSettings("bad-components", 4, 2, ComponentType::UInt16, {makeStats(0.0, 0.0, 0.0, 0.0, 0.0)}));
}

TEST_CASE("ImageSettings clamps window values, centers, widths, thresholds, and opacity", "[image][settings]")
{
  ImageSettings settings = makeSettings();

  CHECK(settings.windowValuesLowHigh(0) == std::pair<double, double>{10.0, 90.0});

  settings.setWindowWidth(0, 1000.0);
  CHECK(settings.windowWidth(0) == Catch::Approx(100.0));
  settings.setWindowWidth(0, -5.0);
  CHECK(settings.windowWidth(0) == Catch::Approx(0.0));

  settings.setWindowCenter(0, 1000.0);
  CHECK(settings.windowCenter(0) == Catch::Approx(100.0));
  settings.setWindowCenter(0, -1000.0);
  CHECK(settings.windowCenter(0) == Catch::Approx(0.0));

  settings.setWindowWidth(0, 80.0);
  settings.setWindowCenter(0, 50.0);
  settings.setWindowValueLow(0, -1000.0, true);
  CHECK(settings.windowValuesLowHigh(0).first >= settings.minMaxWindowRange(0).first);
  settings.setWindowValueHigh(0, 1000.0, true);
  CHECK(settings.windowValuesLowHigh(0).second <= settings.minMaxWindowRange(0).second);

  const auto before = settings.windowValuesLowHigh(0);
  settings.setWindowValueLow(0, 200.0, false);
  settings.setWindowValueHigh(0, -200.0, false);
  CHECK(settings.windowValuesLowHigh(0) == before);

  settings.setThresholdLow(0, -100.0);
  CHECK(settings.thresholds(0).first == Catch::Approx(0.0));
  settings.setThresholdHigh(0, 1000.0);
  CHECK(settings.thresholds(0).second == Catch::Approx(100.0));
  settings.setThresholdLow(0, 80.0);
  settings.setThresholdHigh(0, 70.0);
  CHECK(settings.thresholds(0) == std::pair<double, double>{80.0, 100.0});
  CHECK(settings.thresholdsActive(0));

  settings.setOpacity(0, -2.0);
  CHECK(settings.opacity(0) == Catch::Approx(0.0));
  settings.setOpacity(0, 2.0);
  CHECK(settings.opacity(0) == Catch::Approx(1.0));
  settings.setGlobalOpacity(-1.0);
  CHECK(settings.globalOpacity() == Catch::Approx(0.0));
  settings.setGlobalOpacity(4.0);
  CHECK(settings.globalOpacity() == Catch::Approx(1.0));

  CHECK(settings.vectorArrowOverlayDensity() == Catch::Approx(32.0f));
  CHECK(settings.vectorArrowOverlayVoxelSpacing() == Catch::Approx(4.0f));
  CHECK(settings.vectorArrowOverlaySpacingMode() == VectorArrowOverlaySpacingMode::Voxels);
  settings.setVectorArrowOverlayDensity(0.0f);
  CHECK(settings.vectorArrowOverlayDensity() > 0.0f);
  CHECK(settings.vectorArrowOverlayDensity() == Catch::Approx(0.1f));
  settings.setVectorArrowOverlayDensity(-10.0f);
  CHECK(settings.vectorArrowOverlayDensity() == Catch::Approx(0.1f));
  settings.setVectorArrowOverlayDensity(200.0f);
  CHECK(settings.vectorArrowOverlayDensity() == Catch::Approx(100.0f));

  settings.setVectorArrowOverlayVoxelSpacing(0.0f);
  CHECK(settings.vectorArrowOverlayVoxelSpacing() == Catch::Approx(0.1f));
  settings.setVectorArrowOverlayVoxelSpacing(200.0f);
  CHECK(settings.vectorArrowOverlayVoxelSpacing() == Catch::Approx(100.0f));
  settings.setVectorArrowOverlayMillimeterSpacing(0.0f);
  CHECK(settings.vectorArrowOverlayMillimeterSpacing() == Catch::Approx(0.1f));

  settings.setVectorArrowOverlayLineThickness(12.0f);
  CHECK(settings.vectorArrowOverlayLineThickness() == Catch::Approx(4.0f));

  settings.setVectorArrowOverlayOpacity(-1.0f);
  CHECK(settings.vectorArrowOverlayOpacity() == Catch::Approx(0.0f));
  settings.setVectorArrowOverlayOpacity(2.0f);
  CHECK(settings.vectorArrowOverlayOpacity() == Catch::Approx(1.0f));

  settings.setVectorArrowOverlayScaleFactor(20.0f);
  CHECK(settings.vectorArrowOverlayScaleFactor() == Catch::Approx(10.0f));

  CHECK(settings.vectorWarpedGridPixelSpacing() == Catch::Approx(32.0f));
  CHECK(settings.vectorWarpedGridVoxelSpacing() == Catch::Approx(4.0f));
  CHECK(settings.vectorWarpedGridMillimeterSpacing() == Catch::Approx(4.0f));
  CHECK(settings.vectorWarpedGridSpacingMode() == VectorArrowOverlaySpacingMode::Voxels);
  CHECK(settings.vectorWarpedGridForegroundColor() == glm::vec4{209.0f / 255.0f, 79.0f / 255.0f, 1.0f, 1.0f});
  settings.setVectorWarpedGridPixelSpacing(0.0f);
  CHECK(settings.vectorWarpedGridPixelSpacing() == Catch::Approx(1.0f));
  settings.setVectorWarpedGridPixelSpacing(200.0f);
  CHECK(settings.vectorWarpedGridPixelSpacing() == Catch::Approx(100.0f));
  settings.setVectorWarpedGridVoxelSpacing(0.0f);
  CHECK(settings.vectorWarpedGridVoxelSpacing() == Catch::Approx(0.1f));
  settings.setVectorWarpedGridMillimeterSpacing(0.0f);
  CHECK(settings.vectorWarpedGridMillimeterSpacing() == Catch::Approx(0.1f));
  settings.setVectorWarpedGridLineThickness(20.0f);
  CHECK(settings.vectorWarpedGridLineThickness() == Catch::Approx(8.0f));
  settings.setVectorWarpedGridScaleFactor(20.0f);
  CHECK(settings.vectorWarpedGridScaleFactor() == Catch::Approx(10.0f));
  settings.setVectorWarpedGridForegroundColor(glm::vec4{-1.0f, 0.25f, 2.0f, 0.5f});
  CHECK(settings.vectorWarpedGridForegroundColor() == glm::vec4{0.0f, 0.25f, 1.0f, 0.5f});
  settings.setVectorWarpedGridBackgroundColor(glm::vec4{0.2f, -1.0f, 0.4f, 2.0f});
  CHECK(settings.vectorWarpedGridBackgroundColor() == glm::vec4{0.2f, 0.0f, 0.4f, 1.0f});
}

TEST_CASE("ImageSettings routes component-specific setters through the active component", "[image][settings]")
{
  ImageSettings settings = makeSettings();

  settings.setActiveComponent(1);
  CHECK(settings.activeComponent() == 1);
  settings.setActiveComponent(99);
  CHECK(settings.activeComponent() == 1);

  settings.setVisibility(false);
  settings.setOpacity(0.25);
  settings.setColorMapIndex(3);
  settings.setColorMapInverted(true);
  settings.setColorMapQuantizationLevels(16);
  settings.setColorMapContinuous(false);
  settings.setColormapHsvModfactors(glm::vec3(0.5f, 0.6f, 0.7f));
  settings.setLabelTableIndex(2);
  settings.setInterpolationMode(InterpolationMode::NearestNeighbor);

  CHECK(settings.visibility(0));
  CHECK_FALSE(settings.visibility(1));
  CHECK(settings.opacity(1) == Catch::Approx(0.25));
  CHECK(settings.colorMapIndex(1) == 3);
  CHECK(settings.isColorMapInverted(1));
  CHECK(settings.colorMapQuantizationLevels(1) == 16);
  CHECK_FALSE(settings.colorMapContinuous(1));
  CHECK(settings.colorMapHsvModFactors(1) == glm::vec3(0.5f, 0.6f, 0.7f));
  CHECK(settings.labelTableIndex(1) == 2);
  CHECK(settings.interpolationMode(1) == InterpolationMode::NearestNeighbor);
}

TEST_CASE("ImageSettings stores one validated edge configuration per image", "[image][settings][edges]")
{
  ImageSettings settings = makeSettings();

  CHECK_FALSE(settings.edgesVisible());
  CHECK(settings.edgeDetectionMethod() == EdgeDetectionMethod::Voxel);
  CHECK_FALSE(settings.hardEdges());
  CHECK(settings.thinPixelEdges());
  CHECK_FALSE(settings.overlayEdges());
  CHECK(settings.voxelEdgeScale() == Catch::Approx(4.0));
  CHECK(settings.voxelEdgeThreshold() == Catch::Approx(0.25));
  CHECK(settings.pixelEdgeScale() == Catch::Approx(2.0));
  CHECK(settings.pixelEdgeThreshold() == Catch::Approx(0.2));

  settings.setEdgesVisible(true);
  settings.setEdgeDetectionMethod(EdgeDetectionMethod::ScreenPixel);
  settings.setHardEdges(true);
  settings.setThinPixelEdges(false);
  settings.setVoxelEdgeScale(20.0);
  settings.setVoxelEdgeThreshold(-1.0);
  settings.setPixelEdgeScale(-1.0);
  settings.setPixelEdgeThreshold(2.0);
  settings.setOverlayEdges(true);
  settings.setColormapEdges(true);
  settings.setEdgeColor(glm::vec3{-1.0f, 0.2f, 2.0f});
  settings.setEdgeOpacity(2.0);

  settings.setActiveComponent(1);
  CHECK(settings.edgesVisible());
  CHECK(settings.edgeDetectionMethod() == EdgeDetectionMethod::ScreenPixel);
  CHECK(settings.hardEdges());
  CHECK_FALSE(settings.thinPixelEdges());
  CHECK(settings.voxelEdgeScale() == Catch::Approx(10.0));
  CHECK(settings.voxelEdgeThreshold() == Catch::Approx(0.0));
  CHECK(settings.pixelEdgeScale() == Catch::Approx(0.01));
  CHECK(settings.pixelEdgeThreshold() == Catch::Approx(1.0));
  CHECK(settings.overlayEdges());
  CHECK(settings.colormapEdges());
  CHECK(settings.edgeColor() == glm::vec3{0.0f, 0.2f, 1.0f});
  CHECK(settings.edgeOpacity() == Catch::Approx(1.0));

  settings.setVoxelEdgeScale(std::numeric_limits<double>::quiet_NaN());
  settings.setVoxelEdgeThreshold(std::numeric_limits<double>::infinity());
  settings.setPixelEdgeScale(std::numeric_limits<double>::quiet_NaN());
  settings.setPixelEdgeThreshold(std::numeric_limits<double>::infinity());
  settings.setEdgeColor(glm::vec3{std::numeric_limits<float>::quiet_NaN()});
  settings.setEdgeOpacity(std::numeric_limits<double>::quiet_NaN());
  CHECK(settings.voxelEdgeScale() == Catch::Approx(4.0));
  CHECK(settings.voxelEdgeThreshold() == Catch::Approx(0.25));
  CHECK(settings.pixelEdgeScale() == Catch::Approx(2.0));
  CHECK(settings.pixelEdgeThreshold() == Catch::Approx(0.2));
  CHECK(settings.edgeColor() == glm::vec3{1.0f, 0.0f, 1.0f});
  CHECK(settings.edgeOpacity() == Catch::Approx(1.0));
}

TEST_CASE("ImageSettings copies complete edge configuration", "[image][settings][edges]")
{
  ImageSettings source = makeSettings();
  ImageSettings destination = makeSettings();
  source.setEdgesVisible(true);
  source.setEdgeDetectionMethod(EdgeDetectionMethod::ScreenPixel);
  source.setHardEdges(true);
  source.setThinPixelEdges(false);
  source.setVoxelEdgeScale(3.0);
  source.setVoxelEdgeThreshold(0.4);
  source.setPixelEdgeScale(5.0);
  source.setPixelEdgeThreshold(0.6);
  source.setOverlayEdges(true);
  source.setColormapEdges(true);
  source.setEdgeColor(glm::vec3{0.1f, 0.2f, 0.3f});
  source.setEdgeOpacity(0.7);

  destination.copyEdgeSettingsFrom(source);

  CHECK(destination.edgesVisible());
  CHECK(destination.edgeDetectionMethod() == EdgeDetectionMethod::ScreenPixel);
  CHECK(destination.hardEdges());
  CHECK_FALSE(destination.thinPixelEdges());
  CHECK(destination.voxelEdgeScale() == Catch::Approx(3.0));
  CHECK(destination.voxelEdgeThreshold() == Catch::Approx(0.4));
  CHECK(destination.pixelEdgeScale() == Catch::Approx(5.0));
  CHECK(destination.pixelEdgeThreshold() == Catch::Approx(0.6));
  CHECK(destination.overlayEdges());
  CHECK(destination.colormapEdges());
  CHECK(destination.edgeColor() == glm::vec3{0.1f, 0.2f, 0.3f});
  CHECK(destination.edgeOpacity() == Catch::Approx(0.7));
}

TEST_CASE("ImageSettings updates statistics without resetting visibility when requested", "[image][settings]")
{
  ImageSettings settings = makeSettings();
  settings.setVisibility(0, false);
  settings.setOpacity(0, 0.25);

  settings.updateWithNewComponentStatistics(
    {makeStats(100.0, 110.0, 150.0, 190.0, 200.0), makeStats(0.0, 1.0, 2.0, 3.0, 4.0)},
    false);

  CHECK_FALSE(settings.visibility(0));
  CHECK(settings.opacity(0) == Catch::Approx(0.25));
  CHECK(settings.minMaxImageRange(0) == std::pair<double, double>{100.0, 200.0});
  CHECK(settings.windowValuesLowHigh(0) == std::pair<double, double>{110.0, 190.0});
  CHECK(settings.foregroundThresholds(0) == std::pair<double, double>{150.0, 200.0});
}

TEST_CASE("ImageSettings falls back to min/max window for sparse binary images", "[image][settings]")
{
  ImageSettings settings("sparse-binary", 100, 1, ComponentType::Float32, {makeSparseBinaryStats()});

  CHECK(settings.minMaxImageRange(0) == std::pair<double, double>{0.0, 100.0});
  CHECK(settings.windowValuesLowHigh(0) == std::pair<double, double>{0.0, 100.0});
  CHECK(settings.windowCenter(0) == Catch::Approx(50.0));
  CHECK(settings.windowWidth(0) == Catch::Approx(100.0));
  CHECK(settings.windowQuantilesLowHigh(0) == std::pair<double, double>{0.0, 1.0});
}

TEST_CASE("ImageSettings can set a display-color window wider than the measured data range", "[image][settings]")
{
  ImageSettings settings(
    "uint16-rgb",
    4,
    3,
    ComponentType::UInt16,
    {makeStats(100.0, 120.0, 200.0, 240.0, 255.0),
     makeStats(50.0, 70.0, 120.0, 160.0, 180.0),
     makeStats(1000.0, 1100.0, 1200.0, 1300.0, 1400.0)});

  settings.setWindowRangeForAllComponents({0.0, 65535.0});

  for (uint32_t component = 0; component < settings.numComponents(); ++component) {
    CHECK(settings.windowValuesLowHigh(component) == std::pair<double, double>{0.0, 65535.0});
    CHECK(settings.windowCenter(component) == Catch::Approx(32767.5));
    CHECK(settings.windowWidth(component) == Catch::Approx(65535.0));
    CHECK(settings.minMaxWindowCenterRange(component) == std::pair<double, double>{0.0, 65535.0});
    CHECK(settings.minMaxWindowWidthRange(component) == std::pair<double, double>{0.0, 65535.0});
    CHECK(settings.windowQuantilesLowHigh(component) == std::pair<double, double>{0.0, 1.0});
  }
}

TEST_CASE("ImageSettings maps native component values to texture values", "[image][settings]")
{
  ImageSettings uint8Settings("uint8", 2, 1, ComponentType::UInt8, {makeStats(0.0, 0.0, 128.0, 255.0, 255.0)});
  CHECK(uint8Settings.mapNativeIntensityToTexture(0.0) == Catch::Approx(0.0));
  CHECK(uint8Settings.mapNativeIntensityToTexture(255.0) == Catch::Approx(1.0));
  CHECK(uint8Settings.slope_native_T_texture() == Catch::Approx(255.0f));

  ImageSettings int8Settings("int8", 2, 1, ComponentType::Int8, {makeStats(-128.0, -127.0, 0.0, 127.0, 127.0)});
  CHECK(int8Settings.mapNativeIntensityToTexture(-128.0) == Catch::Approx(-1.0));
  CHECK(int8Settings.mapNativeIntensityToTexture(127.0) == Catch::Approx(1.0));
  CHECK(int8Settings.slope_native_T_texture() == Catch::Approx(127.0f));

  ImageSettings floatSettings("float", 2, 1, ComponentType::Float32, {makeStats(-1.0, -0.5, 0.0, 0.5, 1.0)});
  CHECK(floatSettings.mapNativeIntensityToTexture(0.25) == Catch::Approx(0.25));
  CHECK(floatSettings.slope_native_T_texture() == Catch::Approx(1.0f));
}

TEST_CASE("ImageSettings active-component overloads and global display flags round-trip", "[image][settings]")
{
  ImageSettings settings = makeSettings();
  settings.setActiveComponent(1);

  settings.setDisplayName("renamed");
  CHECK(settings.displayName() == "renamed");
  CHECK(settings.componentType() == ComponentType::UInt16);
  CHECK(settings.numComponents() == 2);

  settings.setBorderColor(glm::vec3{0.25f, 0.5f, 0.75f});
  CHECK(settings.borderColor() == glm::vec3{0.25f, 0.5f, 0.75f});
  settings.setLockedToReference(false);
  CHECK_FALSE(settings.isLockedToReference());
  settings.setDisplayImageAsColor(true);
  CHECK(settings.displayImageAsColor());
  settings.setIgnoreAlpha(true);
  CHECK(settings.ignoreAlpha());
  settings.setColorInterpolationMode(InterpolationMode::NearestNeighbor);
  CHECK(settings.colorInterpolationMode() == InterpolationMode::NearestNeighbor);

  settings.setApplyImageColormapToIsosurfaces(true);
  CHECK(settings.applyImageColormapToIsosurfaces());
  settings.setModulateIsosurfaceOpacityWithImageOpacity(true);
  CHECK(settings.modulateIsosurfaceOpacityWithImageOpacity());
  settings.setIsosurfaceWidthIn2d(3.5);
  CHECK(settings.isoContourLineWidthIn2D() == Catch::Approx(3.5));
  settings.setIsosurfaceOpacityModulator(0.4f);
  CHECK(settings.isosurfaceOpacityModulator() == Catch::Approx(0.4f));
  settings.setOpacity(0.5);
  settings.setGlobalOpacity(0.25);
  CHECK(settings.effectiveIsosurfaceOpacityModulator() == Catch::Approx(0.05f));
  settings.setModulateIsosurfaceOpacityWithImageOpacity(false);
  CHECK(settings.effectiveIsosurfaceOpacityModulator() == Catch::Approx(0.4f));

  settings.setGlobalVisibility(false);
  CHECK_FALSE(settings.globalVisibility());
  settings.setGlobalOpacity(0.375);
  CHECK(settings.globalOpacity() == Catch::Approx(0.375));
  settings.setUsingExactQuantiles(true);
  CHECK(settings.usingExactQuantiles());

  settings.setWindowWidth(8.0);
  CHECK(settings.windowWidth() == Catch::Approx(8.0));
  settings.setWindowCenter(3.0);
  CHECK(settings.windowCenter() == Catch::Approx(3.0));
  settings.setWindowValueLow(-1.0, true);
  CHECK(settings.windowValuesLowHigh().first == Catch::Approx(-1.0));
  settings.setWindowValueHigh(4.0, true);
  CHECK(settings.windowValuesLowHigh().second == Catch::Approx(4.0));

  settings.setWindowQuantileLow(0.2, true);
  settings.setWindowQuantileHigh(0.8, true);
  CHECK(settings.windowQuantilesLowHigh() == std::pair<double, double>{0.01, 0.99});

  settings.setThresholdLow(-5.0);
  settings.setThresholdHigh(6.0);
  CHECK(settings.thresholds() == std::pair<double, double>{-5.0, 6.0});
  CHECK(settings.thresholdsActive());
  CHECK(settings.minMaxImageRange() == std::pair<double, double>{-20.0, 20.0});
  CHECK(settings.minMaxWindowWidthRange() == std::pair<double, double>{0.0, 40.0});
  CHECK(settings.minMaxWindowCenterRange() == std::pair<double, double>{-20.0, 20.0});
  CHECK(settings.minMaxWindowRange() == std::pair<double, double>{-40.0, 40.0});
  CHECK(settings.minMaxThresholdRange() == std::pair<double, double>{-20.0, 20.0});
  CHECK(settings.thresholdRange() == std::pair<double, double>{-20.0, 20.0});

  settings.setForegroundThresholdLow(-2.0);
  settings.setForegroundThresholdHigh(12.0);
  CHECK(settings.foregroundThresholds() == std::pair<double, double>{-2.0, 12.0});

  settings.setEdgesVisible(true);
  settings.setEdgeDetectionMethod(EdgeDetectionMethod::ScreenPixel);
  settings.setHardEdges(true);
  settings.setThinPixelEdges(false);
  settings.setVoxelEdgeScale(5.5);
  settings.setVoxelEdgeThreshold(0.55);
  settings.setPixelEdgeScale(20.0);
  settings.setPixelEdgeThreshold(-1.0);
  settings.setOverlayEdges(true);
  settings.setColormapEdges(true);
  settings.setEdgeColor(glm::vec3{0.9f, 0.8f, 0.7f});
  settings.setEdgeOpacity(0.35);
  CHECK(settings.edgesVisible());
  CHECK(settings.edgeDetectionMethod() == EdgeDetectionMethod::ScreenPixel);
  CHECK(settings.hardEdges());
  CHECK_FALSE(settings.thinPixelEdges());
  CHECK(settings.voxelEdgeScale() == Catch::Approx(5.5));
  CHECK(settings.voxelEdgeThreshold() == Catch::Approx(0.55));
  CHECK(settings.pixelEdgeScale() == Catch::Approx(10.0));
  CHECK(settings.pixelEdgeThreshold() == Catch::Approx(0.0));
  CHECK(settings.overlayEdges());
  CHECK(settings.colormapEdges());
  CHECK(settings.edgeColor() == glm::vec3{0.9f, 0.8f, 0.7f});
  CHECK(settings.edgeOpacity() == Catch::Approx(0.35));

  settings.setColorMapIndex(7);
  settings.setColorMapInverted(true);
  settings.setColorMapQuantizationLevels(32);
  settings.setColorMapContinuous(false);
  settings.setColorMapHueModFactor(0.25);
  settings.setColorMapSatModFactor(0.5);
  settings.setColorMapValModFactor(0.75);
  settings.setLabelTableIndex(4);
  settings.setInterpolationMode(InterpolationMode::NearestNeighbor);
  CHECK(settings.colorMapIndex() == 7);
  CHECK(settings.isColorMapInverted());
  CHECK(settings.colorMapQuantizationLevels() == 32);
  CHECK_FALSE(settings.colorMapContinuous());
  CHECK(settings.colorMapHsvModFactors() == glm::vec3{0.25f, 0.5f, 0.75f});
  CHECK(settings.labelTableIndex() == 4);
  CHECK(settings.interpolationMode() == InterpolationMode::NearestNeighbor);

  CHECK(settings.slopeIntercept_normalized_T_native().first > 0.0);
  CHECK(settings.slopeIntercept_normalized_T_texture().first > 0.0);
  CHECK(settings.slopeInterceptVec2_normalized_T_texture().x > 0.0);
  CHECK(settings.largestSlopeInterceptTextureVec2().x > 0.0);
  CHECK(settings.componentStatistics().onlineStats.min == Catch::Approx(-20.0));
  CHECK(settings.histogramSettings().m_intensityRange[0] == Catch::Approx(-20.0));

  auto& histogram = settings.histogramSettings();
  histogram.m_isCumulative = true;
  CHECK(settings.histogramSettings().m_isCumulative);
}

TEST_CASE("ImageSettings clamps warp rendering strength", "[image][settings]")
{
  ImageSettings settings;

  CHECK(settings.warpEnabled());
  CHECK(settings.warpStrength() == Catch::Approx(1.0f));
  CHECK_FALSE(settings.allowExaggeratedWarp());

  settings.setWarpEnabled(false);
  settings.setWarpStrength(-1.0f);
  CHECK_FALSE(settings.warpEnabled());
  CHECK(settings.warpStrength() == Catch::Approx(0.0f));

  settings.setWarpStrength(2.5f);
  CHECK(settings.warpStrength() == Catch::Approx(2.5f));

  settings.setWarpStrength(8.0f);
  CHECK(settings.warpStrength() == Catch::Approx(4.0f));

  settings.setAllowExaggeratedWarp(false);
  CHECK(settings.warpStrength() == Catch::Approx(1.0f));

  settings.setAllowExaggeratedWarp(true);
  settings.setWarpStrength(3.5f);
  CHECK(settings.allowExaggeratedWarp());
  CHECK(settings.warpStrength() == Catch::Approx(3.5f));
}
