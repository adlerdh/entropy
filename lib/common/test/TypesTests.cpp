#include "common/Types.h"

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <string>

namespace
{

constexpr std::array<ComponentType, 14> k_allComponentTypes{
  ComponentType::Int8,
  ComponentType::UInt8,
  ComponentType::Int16,
  ComponentType::UInt16,
  ComponentType::Int32,
  ComponentType::UInt32,
  ComponentType::Float32,
  ComponentType::Float64,
  ComponentType::ULong,
  ComponentType::Long,
  ComponentType::ULongLong,
  ComponentType::LongLong,
  ComponentType::LongDouble,
  ComponentType::Undefined};

constexpr std::array<MouseMode, 11> k_allMouseModes{
  MouseMode::Pointer,
  MouseMode::WindowLevel,
  MouseMode::Segment,
  MouseMode::Annotate,
  MouseMode::CameraTranslate,
  MouseMode::CameraRotate,
  MouseMode::CameraZoom,
  MouseMode::CrosshairsRotate,
  MouseMode::ImageTranslate,
  MouseMode::ImageRotate,
  MouseMode::ImageScale};

} // namespace

TEST_CASE("component type predicates classify supported numeric families", "[common][types]")
{
  for (const ComponentType type : k_allComponentTypes) {
    const bool expectedFloating =
      ComponentType::Float32 == type || ComponentType::Float64 == type || ComponentType::LongDouble == type;
    const bool expectedUnsigned = ComponentType::UInt8 == type || ComponentType::UInt16 == type ||
                                  ComponentType::UInt32 == type || ComponentType::ULong == type ||
                                  ComponentType::ULongLong == type;
    const bool expectedSigned = ComponentType::Int8 == type || ComponentType::Int16 == type ||
                                ComponentType::Int32 == type || ComponentType::Long == type ||
                                ComponentType::LongLong == type;

    CHECK(isComponentFloatingPoint(type) == expectedFloating);
    CHECK(isFloatingType(type) == expectedFloating);
    CHECK(isIntegerType(type) == !expectedFloating);
    CHECK(isComponentUnsignedInt(type) == expectedUnsigned);
    CHECK(isUnsignedIntegerType(type) == expectedUnsigned);
    CHECK(isSignedIntegerType(type) == expectedSigned);
    CHECK_FALSE(componentTypeString(type).empty());
  }
}

TEST_CASE("segmentation component types are restricted to unsigned OpenGL-compatible integers", "[common][types]")
{
  CHECK(isValidSegmentationComponentType(ComponentType::UInt8));
  CHECK(isValidSegmentationComponentType(ComponentType::UInt16));
  CHECK(isValidSegmentationComponentType(ComponentType::UInt32));

  CHECK_FALSE(isValidSegmentationComponentType(ComponentType::Int8));
  CHECK_FALSE(isValidSegmentationComponentType(ComponentType::Float32));
  CHECK_FALSE(isValidSegmentationComponentType(ComponentType::ULong));
  CHECK_FALSE(isValidSegmentationComponentType(ComponentType::Undefined));
}

TEST_CASE("component type strings are stable user-facing descriptions", "[common][types]")
{
  CHECK(componentTypeString(ComponentType::Int8) == "signed 8-bit char (int8)");
  CHECK(componentTypeString(ComponentType::UInt8) == "unsigned 8-bit char (uint8)");
  CHECK(componentTypeString(ComponentType::Float32) == "single 32-bit float (float)");
  CHECK(componentTypeString(ComponentType::Float64) == "double 64-bit float (double)");
  CHECK(componentTypeString(ComponentType::Undefined) == "undefined");
}

TEST_CASE("interpolation mode strings cover all advertised modes", "[common][types]")
{
  REQUIRE(AllInterpolationModes.size() == 3);
  CHECK(typeString(InterpolationMode::NearestNeighbor) == "Nearest neighbor");
  CHECK(typeString(InterpolationMode::Linear) == "Linear");
  CHECK(typeString(InterpolationMode::CubicBsplineConvolution) == "Cubic B-spline convolution");
}

TEST_CASE("mouse modes expose strings and toolbar icons", "[common][types]")
{
  REQUIRE(AllMouseModes.size() == 11);

  for (const MouseMode mode : k_allMouseModes) {
    CHECK_FALSE(typeString(mode).empty());
    CHECK(toolbarButtonIcon(mode) != nullptr);
    CHECK(std::string{toolbarButtonIcon(mode)}.size() > 0);
  }

  CHECK(typeString(MouseMode::Pointer).find("Pointer") != std::string::npos);
  CHECK(typeString(MouseMode::ImageScale).find("Scale image") != std::string::npos);
}

TEST_CASE("small value types default to neutral values", "[common][types]")
{
  const OnlineStats onlineStats;
  const ComponentStats componentStats{};
  CHECK(onlineStats.min == 0.0);
  CHECK(onlineStats.max == 0.0);
  CHECK(onlineStats.mean == 0.0);
  CHECK(onlineStats.stdev == 0.0);
  CHECK(onlineStats.variance == 0.0);
  CHECK(onlineStats.sum == 0.0);
  CHECK(onlineStats.count == 0);
  CHECK(componentStats.onlineStats.count == 0);
  CHECK(componentStats.quantiles.front() == 0.0);
  CHECK(componentStats.quantiles.back() == 0.0);

  const QuantileOfValue quantile;
  CHECK(quantile.lowerQuantile == 0.0);
  CHECK(quantile.upperQuantile == 0.0);
  CHECK(quantile.lowerIndex == 0);
  CHECK(quantile.upperIndex == 0);
  CHECK(quantile.lowerValue == 0.0);
  CHECK(quantile.upperValue == 0.0);
  CHECK_FALSE(quantile.foundValue);

  const AnatomicalLabelPosInfo labelInfo{2};
  CHECK(labelInfo.labelIndex == 2);

  const ViewOffsetSetting offsetSetting;
  CHECK(offsetSetting.m_offsetMode == ViewOffsetMode::None);
  CHECK(offsetSetting.m_absoluteOffset == 0.0f);
  CHECK(offsetSetting.m_relativeOffsetSteps == 0);
  CHECK_FALSE(offsetSetting.m_offsetImage.has_value());

  const FrameBounds frameBounds{{1.0f, 2.0f, 3.0f, 4.0f}};
  CHECK(frameBounds.bounds.xoffset == 1.0f);
  CHECK(frameBounds.bounds.yoffset == 2.0f);
  CHECK(frameBounds.bounds.width == 3.0f);
  CHECK(frameBounds.bounds.height == 4.0f);
}
