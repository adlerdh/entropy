#include "image/ImageTimeAxis.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

TEST_CASE("ImageTimeAxis represents single-frame and regular time axes", "[image][time]")
{
  const ImageTimeAxis single;
  CHECK_FALSE(single.isTimeSeries());
  CHECK(single.numTimePoints() == 1);
  CHECK(single.value(0).value() == 0.0);
  CHECK_FALSE(single.value(1).has_value());

  const ImageTimeAxis timeAxis(4, 10.0, 2.5, "ms");
  CHECK(timeAxis.isTimeSeries());
  CHECK(timeAxis.numTimePoints() == 4);
  CHECK(timeAxis.units() == "ms");
  CHECK(timeAxis.spacing().value() == 2.5);
  CHECK(timeAxis.value(0).value() == 10.0);
  CHECK(timeAxis.value(3).value() == 17.5);
  CHECK(timeAxis.clamp(99) == 3);
  CHECK(timeAxis.playbackFramePeriodSeconds() == Catch::Approx(1.0 / 120.0));
  CHECK(timeAxis.playbackFramePeriodSeconds(2.0) == Catch::Approx(1.0 / 120.0));

  CHECK_FALSE(single.spacing().has_value());
  const ImageTimeAxis irregular({0.0, 1.0, 3.0}, "sec");
  CHECK_FALSE(irregular.spacing().has_value());
  CHECK(irregular.playbackFramePeriodSeconds() == 1.0);
  CHECK(ImageTimeAxis(2, 0.0, 1.0, "frame").playbackFramePeriodSeconds() == 0.1);
}

TEST_CASE("ImageTimeAxis computes playback speed limits from the time spacing", "[image][time]")
{
  constexpr double maxFramesPerSecond = 120.0;

  CHECK(ImageTimeAxis(10, 0.0, 0.2, "sec").maxPlaybackSpeedForFrameRate(maxFramesPerSecond) == Catch::Approx(24.0));
  CHECK(ImageTimeAxis(10, 0.0, 2.5, "ms").maxPlaybackSpeedForFrameRate(maxFramesPerSecond) == Catch::Approx(0.3));
  CHECK(ImageTimeAxis(10, 0.0, 2.0, "frame").maxPlaybackSpeedForFrameRate(maxFramesPerSecond) == Catch::Approx(24.0));
  CHECK(ImageTimeAxis(10, 0.0, 0.2, "sec").playbackFramePeriodSeconds(24.0) == Catch::Approx(1.0 / 120.0));
  CHECK(ImageTimeAxis(10, 0.0, 0.2, "sec").playbackFramePeriodSeconds(48.0) == Catch::Approx(1.0 / 120.0));

  CHECK(ImageTimeAxis(10, 0.0, 0.2, "sec").maxPlaybackSpeedForFrameRate(1.0) == Catch::Approx(0.2));
  CHECK(ImageTimeAxis(10, 0.0, 0.001, "sec").maxPlaybackSpeedForFrameRate(1.0) == Catch::Approx(0.001));
  CHECK(ImageTimeAxis(10, 0.0, 0.001, "sec").playbackFramePeriodSeconds(0.001) == Catch::Approx(1.0));
}

TEST_CASE("Time frame addressing supports separate and interleaved component buffers", "[image][time]")
{
  constexpr std::size_t spatialVoxelCount = 20;
  constexpr std::size_t spatialIndex = 7;
  constexpr uint32_t timePoint = 3;

  const auto separate = separateComponentFrameAddress(2, spatialIndex, timePoint, spatialVoxelCount);
  CHECK(separate.first == 2);
  CHECK(separate.second == 67);

  const auto interleaved = interleavedComponentFrameAddress(2, spatialIndex, timePoint, spatialVoxelCount, 4);
  CHECK(interleaved.first == 0);
  CHECK(interleaved.second == 270);
}
