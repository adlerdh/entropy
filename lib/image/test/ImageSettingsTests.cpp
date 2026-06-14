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
  settings.setThresholdEdges(true);
  settings.setUseFreiChen(true);
  settings.setEdgeMagnitude(0.75);
  settings.setWindowedEdges(true);
  settings.setOverlayEdges(true);
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
  CHECK(settings.showEdges(1));
  CHECK(settings.thresholdEdges(1));
  CHECK(settings.useFreiChen(1));
  CHECK(settings.edgeMagnitude(1) == Catch::Approx(0.75));
  CHECK(settings.windowedEdges(1));
  CHECK(settings.overlayEdges(1));
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
