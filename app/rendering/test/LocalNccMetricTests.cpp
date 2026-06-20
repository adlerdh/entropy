#include "rendering/LocalNccMetric.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

namespace local_ncc = entropy::rendering::local_ncc;

TEST_CASE("local NCC metric maps correlation to dissimilarity", "[rendering][local-ncc]")
{
  CHECK(local_ncc::metricValue(1.0f, local_ncc::Presentation::Dissimilarity, true) == Catch::Approx(0.0f));
  CHECK(local_ncc::metricValue(0.25f, local_ncc::Presentation::Dissimilarity, true) == Catch::Approx(0.75f));
  CHECK(local_ncc::metricValue(-0.5f, local_ncc::Presentation::Dissimilarity, true) == Catch::Approx(1.0f));
}

TEST_CASE("local NCC metric can preserve negative correlation in dissimilarity", "[rendering][local-ncc]")
{
  CHECK(local_ncc::metricValue(1.0f, local_ncc::Presentation::Dissimilarity, false) == Catch::Approx(0.0f));
  CHECK(local_ncc::metricValue(0.0f, local_ncc::Presentation::Dissimilarity, false) == Catch::Approx(0.5f));
  CHECK(local_ncc::metricValue(-1.0f, local_ncc::Presentation::Dissimilarity, false) == Catch::Approx(1.0f));
}

TEST_CASE("local NCC metric can display correlation directly", "[rendering][local-ncc]")
{
  CHECK(local_ncc::metricValue(-1.0f, local_ncc::Presentation::Correlation, true) == Catch::Approx(0.0f));
  CHECK(local_ncc::metricValue(0.0f, local_ncc::Presentation::Correlation, true) == Catch::Approx(0.5f));
  CHECK(local_ncc::metricValue(1.0f, local_ncc::Presentation::Correlation, true) == Catch::Approx(1.0f));
}

TEST_CASE("local NCC metric clamps out-of-range correlation values", "[rendering][local-ncc]")
{
  CHECK(local_ncc::metricValue(-2.0f, local_ncc::Presentation::Correlation, true) == Catch::Approx(0.0f));
  CHECK(local_ncc::metricValue(2.0f, local_ncc::Presentation::Dissimilarity, true) == Catch::Approx(0.0f));
}

TEST_CASE("local NCC patch sample requirement is clamped", "[rendering][local-ncc]")
{
  CHECK(local_ncc::requiredSampleCount(3, 0.75f) == 37);
  CHECK(local_ncc::requiredSampleCount(3, -1.0f) == 1);
  CHECK(local_ncc::requiredSampleCount(3, 2.0f) == 49);
}
