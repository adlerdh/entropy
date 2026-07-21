#include "image/ImageWindowDefaults.h"

#include <catch2/catch_test_macros.hpp>

#include <limits>

namespace
{

ComponentStats makeStats(long double minValue, long double maxValue)
{
  ComponentStats stats;
  stats.onlineStats.min = minValue;
  stats.onlineStats.max = maxValue;
  stats.onlineStats.count = 4;
  stats.quantiles.fill(minValue);
  stats.quantiles[100] = maxValue;
  return stats;
}

} // namespace

TEST_CASE("raster color window defaults use full unsigned integer channel range", "[image][window-defaults]")
{
  const ComponentStats stats = makeStats(100.0, 500.0);

  const auto uint8Window = image_window_defaults::rasterColorComponentWindow(ComponentType::UInt8, stats);
  REQUIRE(uint8Window);
  CHECK(uint8Window->low == 0.0);
  CHECK(uint8Window->high == static_cast<double>(std::numeric_limits<std::uint8_t>::max()));

  const auto uint16Window = image_window_defaults::rasterColorComponentWindow(ComponentType::UInt16, stats);
  REQUIRE(uint16Window);
  CHECK(uint16Window->low == 0.0);
  CHECK(uint16Window->high == static_cast<double>(std::numeric_limits<std::uint16_t>::max()));

  const auto uint32Window = image_window_defaults::rasterColorComponentWindow(ComponentType::UInt32, stats);
  REQUIRE(uint32Window);
  CHECK(uint32Window->low == 0.0);
  CHECK(uint32Window->high == static_cast<double>(std::numeric_limits<std::uint32_t>::max()));
}

TEST_CASE(
  "raster color window defaults use data range for signed and floating-point channels",
  "[image][window-defaults]")
{
  const ComponentStats stats = makeStats(-2.5, 7.5);

  const auto int16Window = image_window_defaults::rasterColorComponentWindow(ComponentType::Int16, stats);
  REQUIRE(int16Window);
  CHECK(int16Window->low == -2.5);
  CHECK(int16Window->high == 7.5);

  const auto floatWindow = image_window_defaults::rasterColorComponentWindow(ComponentType::Float32, stats);
  REQUIRE(floatWindow);
  CHECK(floatWindow->low == -2.5);
  CHECK(floatWindow->high == 7.5);
}
