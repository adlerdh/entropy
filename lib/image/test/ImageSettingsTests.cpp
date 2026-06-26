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
  settings.setVectorArrowOverlayDensity(0.0f);
  CHECK(settings.vectorArrowOverlayDensity() > 0.0f);
  CHECK(settings.vectorArrowOverlayDensity() == Catch::Approx(0.1f));
  settings.setVectorArrowOverlayDensity(-10.0f);
  CHECK(settings.vectorArrowOverlayDensity() == Catch::Approx(0.1f));

  settings.setVectorArrowOverlayVoxelSpacing(0.0f);
  CHECK(settings.vectorArrowOverlayVoxelSpacing() == Catch::Approx(0.1f));
  settings.setVectorArrowOverlayVoxelSpacing(20.0f);
  CHECK(settings.vectorArrowOverlayVoxelSpacing() == Catch::Approx(10.0f));
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
  settings.setVectorWarpedGridPixelSpacing(0.0f);
  CHECK(settings.vectorWarpedGridPixelSpacing() == Catch::Approx(1.0f));
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
  settings.setShowEdges(true);
  settings.setShowPixelEdges(true);
  settings.setThresholdEdges(true);
  settings.setThresholdPixelEdges(true);
  settings.setThinPixelEdges(false);
  settings.setUseFreiChen(true);
  settings.setEdgeMagnitude(0.75);
  settings.setPixelEdgeScale(2.5);
  settings.setPixelEdgeThreshold(0.4);
  settings.setWindowedEdges(true);
  settings.setOverlayEdges(true);
  settings.setOverlayPixelEdges(false);
  settings.setColormapEdges(true);
  settings.setEdgeColor(glm::vec3(0.1f, 0.2f, 0.3f));
  settings.setEdgeOpacity(0.6);
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
  CHECK_FALSE(settings.showEdges(1));
  CHECK(settings.showPixelEdges(1));
  CHECK(settings.edgeDetectionMethod(1) == EdgeDetectionMethod::Pixel);
  CHECK(settings.thresholdEdges(1));
  CHECK(settings.thresholdPixelEdges(1));
  CHECK_FALSE(settings.thinPixelEdges(1));
  CHECK(settings.useFreiChen(1));
  CHECK(settings.edgeMagnitude(1) == Catch::Approx(0.75));
  CHECK(settings.pixelEdgeScale(1) == Catch::Approx(2.5));
  CHECK(settings.pixelEdgeThreshold(1) == Catch::Approx(0.4));
  CHECK(settings.windowedEdges(1));
  CHECK(settings.overlayEdges(1));
  CHECK_FALSE(settings.overlayPixelEdges(1));
  CHECK(settings.colormapEdges(1));
  CHECK(settings.edgeColor(1) == glm::vec3(0.1f, 0.2f, 0.3f));
  CHECK(settings.edgeOpacity(1) == Catch::Approx(0.6));
  CHECK(settings.colorMapIndex(1) == 3);
  CHECK(settings.isColorMapInverted(1));
  CHECK(settings.colorMapQuantizationLevels(1) == 16);
  CHECK_FALSE(settings.colorMapContinuous(1));
  CHECK(settings.colorMapHsvModFactors(1) == glm::vec3(0.5f, 0.6f, 0.7f));
  CHECK(settings.labelTableIndex(1) == 2);
  CHECK(settings.interpolationMode(1) == InterpolationMode::NearestNeighbor);
}

TEST_CASE("ImageSettings keeps edge rendering mode mutually exclusive", "[image][settings]")
{
  ImageSettings settings = makeSettings();

  CHECK_FALSE(settings.showAnyEdges());
  CHECK(settings.edgeDetectionMethod() == EdgeDetectionMethod::Voxel);
  CHECK_FALSE(settings.thresholdEdges());
  CHECK_FALSE(settings.thresholdPixelEdges());
  CHECK(settings.thinPixelEdges());
  CHECK_FALSE(settings.overlayEdges());
  CHECK_FALSE(settings.overlayPixelEdges());
  CHECK(settings.pixelEdgeScale() == Catch::Approx(2.0));

  settings.setShowAnyEdges(true);
  CHECK(settings.showAnyEdges());
  CHECK(settings.showEdges());
  CHECK_FALSE(settings.showPixelEdges());

  settings.setShowAnyEdges(false);
  CHECK_FALSE(settings.showAnyEdges());
  CHECK_FALSE(settings.showEdges());
  CHECK_FALSE(settings.showPixelEdges());
  CHECK(settings.edgeDetectionMethod() == EdgeDetectionMethod::Voxel);

  settings.setShowAnyEdges(true);
  CHECK(settings.showEdges());
  CHECK_FALSE(settings.showPixelEdges());

  settings.setShowPixelEdges(true);
  CHECK(settings.edgeDetectionMethod() == EdgeDetectionMethod::Pixel);
  CHECK_FALSE(settings.showEdges());
  CHECK(settings.showPixelEdges());
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

  settings.setUseDistanceMapForRaycasting(false);
  CHECK_FALSE(settings.useDistanceMapForRaycasting());
  settings.setIsosurfacesVisible(false);
  CHECK_FALSE(settings.isosurfacesVisible());
  settings.setApplyImageColormapToIsosurfaces(true);
  CHECK(settings.applyImageColormapToIsosurfaces());
  settings.setShowIsoscontoursIn2D(false);
  CHECK_FALSE(settings.showIsocontoursIn2D());
  settings.setIsosurfaceWidthIn2d(3.5);
  CHECK(settings.isoContourLineWidthIn2D() == Catch::Approx(3.5));
  settings.setIsosurfaceOpacityModulator(0.4f);
  CHECK(settings.isosurfaceOpacityModulator() == Catch::Approx(0.4f));

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
  CHECK(settings.windowQuantilesLowHigh() == std::pair<double, double>{0.0, 0.0});

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

  settings.setShowEdges(true);
  settings.setShowPixelEdges(true);
  settings.setThresholdEdges(true);
  settings.setThresholdPixelEdges(true);
  settings.setThinPixelEdges(false);
  settings.setUseFreiChen(true);
  settings.setEdgeMagnitude(0.55);
  settings.setPixelEdgeScale(20.0);
  settings.setPixelEdgeThreshold(-1.0);
  settings.setWindowedEdges(true);
  settings.setOverlayEdges(true);
  settings.setOverlayPixelEdges(false);
  settings.setColormapEdges(true);
  settings.setEdgeColor(glm::vec3{0.9f, 0.8f, 0.7f});
  settings.setEdgeOpacity(0.35);
  CHECK_FALSE(settings.showEdges());
  CHECK(settings.showPixelEdges());
  CHECK(settings.edgeDetectionMethod() == EdgeDetectionMethod::Pixel);
  CHECK(settings.thresholdEdges());
  CHECK(settings.thresholdPixelEdges());
  CHECK_FALSE(settings.thinPixelEdges());
  CHECK(settings.useFreiChen());
  CHECK(settings.edgeMagnitude() == Catch::Approx(0.55));
  CHECK(settings.pixelEdgeScale() == Catch::Approx(10.0));
  CHECK(settings.pixelEdgeThreshold() == Catch::Approx(0.0));
  CHECK(settings.windowedEdges());
  CHECK(settings.overlayEdges());
  CHECK_FALSE(settings.overlayPixelEdges());
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
