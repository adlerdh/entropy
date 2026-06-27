#include "ImageGenerator.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>
#include <itkImageFileReader.h>
#include <itkMetaDataObject.h>
#include <itkVectorImage.h>

#include <filesystem>

TEST_CASE("Image generator parses richer time-series patterns", "[image-generator][time]")
{
  const auto pulsing = image_generator::parseSpecJson(R"json({
    "output": "pulsing.nrrd",
    "pattern": "pulsing_gaussian"
  })json");
  CHECK(pulsing.pattern == image_generator::Pattern::PulsingGaussian);

  const auto twoBlobs = image_generator::parseSpecJson(R"json({
    "output": "two-blobs.nrrd",
    "pattern": "two_moving_gaussians"
  })json");
  CHECK(twoBlobs.pattern == image_generator::Pattern::TwoMovingGaussians);

  const auto rotating = image_generator::parseSpecJson(R"json({
    "output": "rotating.nrrd",
    "pattern": "rotating_wave"
  })json");
  CHECK(rotating.pattern == image_generator::Pattern::RotatingWave);

  const auto warp = image_generator::parseSpecJson(R"json({
    "output": "warp.nrrd",
    "pixel_kind": "vector",
    "components": 3,
    "pattern": "time_varying_warp_field"
  })json");
  CHECK(warp.pattern == image_generator::Pattern::TimeVaryingWarpField);
}

TEST_CASE("Time-varying warp field changes vector values over time", "[image-generator][time][vector]")
{
  const auto spec = image_generator::parseSpecJson(R"json({
    "output": "warp.nrrd",
    "pixel_kind": "vector",
    "component_type": "float",
    "components": 3,
    "size": [8, 8, 6, 5],
    "pattern": "time_varying_warp_field",
    "amplitude": 6.0
  })json");

  const std::vector<std::size_t> frame0{4, 3, 2, 0};
  const std::vector<std::size_t> frame1{4, 3, 2, 1};
  CHECK(image_generator::expectedComponentValue(spec, frame0, 0) != image_generator::expectedComponentValue(spec, frame1, 0));
  CHECK(image_generator::expectedComponentValue(spec, frame0, 1) != image_generator::expectedComponentValue(spec, frame1, 1));
  CHECK(image_generator::expectedComponentValue(spec, frame0, 2) != image_generator::expectedComponentValue(spec, frame1, 2));
}

TEST_CASE("Image generator parses vector image specifications", "[image-generator]")
{
  const auto spec = image_generator::parseSpecJson(R"json({
    "output": "vector5.nrrd",
    "pixel_kind": "vector",
    "component_type": "float",
    "components": 5,
    "size": [7, 5, 3],
    "spacing": [0.5, 0.75, 2.0],
    "origin": [1.0, 2.0, 3.0],
    "pattern": "component_ramp",
    "metadata": {"kind": "test"}
  })json");

  CHECK(spec.output == "vector5.nrrd");
  CHECK(spec.pixelKind == image_generator::PixelKind::Vector);
  CHECK(spec.componentType == "float");
  CHECK(spec.components == 5);
  CHECK(spec.size == std::vector<std::size_t>{7, 5, 3});
  CHECK(spec.spacing == std::vector<double>{0.5, 0.75, 2.0});
  CHECK(spec.origin == std::vector<double>{1.0, 2.0, 3.0});
  CHECK(spec.pattern == image_generator::Pattern::ComponentRamp);
  CHECK(spec.metadata.at("kind") == "test");
}

TEST_CASE("Image generator validation catches inconsistent component counts", "[image-generator]")
{
  auto spec = image_generator::parseSpecJson(R"json({
    "output": "bad.nrrd",
    "pixel_kind": "scalar",
    "components": 3,
    "size": [4, 4]
  })json");

  CHECK_THROWS_WITH(image_generator::validateSpec(spec), "Scalar images must have exactly one component");

  spec = image_generator::parseSpecJson(R"json({
    "output": "bad.nrrd",
    "pixel_kind": "complex",
    "component_type": "uint16",
    "size": [4, 4]
  })json");

  CHECK_THROWS_WITH(image_generator::validateSpec(spec), "Complex images support float or double component types");
}

TEST_CASE("Image generator preserves explicit lower-dimensional time-series metadata", "[image-generator][time]")
{
  const auto spec = image_generator::parseSpecJson(R"json({
    "output": "scalar-2d-time.nrrd",
    "pixel_kind": "scalar",
    "component_type": "float",
    "components": 1,
    "size": [8, 6, 5],
    "spacing": [0.5, 0.75, 2.0],
    "pattern": "moving_gaussian",
    "metadata": {
      "entropy_time_axis": "last",
      "entropy_time_units": "frame"
    }
  })json");

  CHECK(spec.size == std::vector<std::size_t>{8, 6, 5});
  CHECK(spec.spacing == std::vector<double>{0.5, 0.75, 2.0});
  CHECK(spec.metadata.at("entropy_time_axis") == "last");
  CHECK(spec.metadata.at("entropy_time_units") == "frame");
}

TEST_CASE("Image generator writes a small vector image", "[image-generator][itk]")
{
  const std::filesystem::path output = std::filesystem::temp_directory_path() / "entropy-image-generator-vector3.nrrd";

  auto spec = image_generator::parseSpecJson(R"json({
    "pixel_kind": "vector",
    "component_type": "float",
    "components": 3,
    "size": [4, 3],
    "pattern": "component_ramp"
  })json");
  spec.output = output;

  image_generator::writeImage(spec);
  REQUIRE(std::filesystem::exists(output));

  using ImageType = itk::VectorImage<float, 2>;
  auto reader = itk::ImageFileReader<ImageType>::New();
  reader->SetFileName(output.string());
  reader->Update();

  const auto* image = reader->GetOutput();
  CHECK(image->GetNumberOfComponentsPerPixel() == 3);
  CHECK(image->GetLargestPossibleRegion().GetSize()[0] == 4);
  CHECK(image->GetLargestPossibleRegion().GetSize()[1] == 3);
}

TEST_CASE("Image generator writes a small 4D vector time series", "[image-generator][itk][4d]")
{
  const std::filesystem::path output = std::filesystem::temp_directory_path() / "entropy-image-generator-vector4d.nrrd";

  auto spec = image_generator::parseSpecJson(R"json({
    "pixel_kind": "vector",
    "component_type": "float",
    "components": 3,
    "size": [4, 3, 2, 5],
    "spacing": [0.5, 0.75, 1.25, 2.0],
    "pattern": "moving_gaussian"
  })json");
  spec.output = output;

  image_generator::writeImage(spec);
  REQUIRE(std::filesystem::exists(output));

  using ImageType = itk::VectorImage<float, 4>;
  auto reader = itk::ImageFileReader<ImageType>::New();
  reader->SetFileName(output.string());
  reader->Update();

  const auto* image = reader->GetOutput();
  CHECK(image->GetNumberOfComponentsPerPixel() == 3);
  CHECK(image->GetLargestPossibleRegion().GetSize()[0] == 4);
  CHECK(image->GetLargestPossibleRegion().GetSize()[1] == 3);
  CHECK(image->GetLargestPossibleRegion().GetSize()[2] == 2);
  CHECK(image->GetLargestPossibleRegion().GetSize()[3] == 5);
  CHECK(image->GetSpacing()[3] == 2.0);
}

TEST_CASE("Image generator writes canonical 1D time series as 4D NIfTI", "[image-generator][itk][time][nifti]")
{
  const std::filesystem::path output = std::filesystem::temp_directory_path() / "entropy-image-generator-1d-time.nii.gz";

  auto spec = image_generator::parseSpecJson(R"json({
    "pixel_kind": "scalar",
    "component_type": "float",
    "components": 1,
    "size": [5, 1, 1, 4],
    "spacing": [0.5, 1.0, 1.0, 2.0],
    "pattern": "time_ramp"
  })json");
  spec.output = output;

  image_generator::writeImage(spec);
  REQUIRE(std::filesystem::exists(output));

  using ImageType = itk::Image<float, 4>;
  auto reader = itk::ImageFileReader<ImageType>::New();
  reader->SetFileName(output.string());
  reader->Update();

  const auto* image = reader->GetOutput();
  CHECK(image->GetLargestPossibleRegion().GetSize()[0] == 5);
  CHECK(image->GetLargestPossibleRegion().GetSize()[1] == 1);
  CHECK(image->GetLargestPossibleRegion().GetSize()[2] == 1);
  CHECK(image->GetLargestPossibleRegion().GetSize()[3] == 4);
  CHECK(image->GetSpacing()[3] == 2.0);
}

TEST_CASE("Image generator writes canonical 2D time series as 4D NIfTI", "[image-generator][itk][time][nifti]")
{
  const std::filesystem::path output = std::filesystem::temp_directory_path() / "entropy-image-generator-2d-time.nii.gz";

  auto spec = image_generator::parseSpecJson(R"json({
    "pixel_kind": "scalar",
    "component_type": "float",
    "components": 1,
    "size": [4, 3, 1, 5],
    "spacing": [0.5, 0.75, 1.0, 2.0],
    "pattern": "moving_gaussian"
  })json");
  spec.output = output;

  image_generator::writeImage(spec);
  REQUIRE(std::filesystem::exists(output));

  using ImageType = itk::Image<float, 4>;
  auto reader = itk::ImageFileReader<ImageType>::New();
  reader->SetFileName(output.string());
  reader->Update();

  const auto* image = reader->GetOutput();
  CHECK(image->GetLargestPossibleRegion().GetSize()[0] == 4);
  CHECK(image->GetLargestPossibleRegion().GetSize()[1] == 3);
  CHECK(image->GetLargestPossibleRegion().GetSize()[2] == 1);
  CHECK(image->GetLargestPossibleRegion().GetSize()[3] == 5);
  CHECK(image->GetSpacing()[3] == 2.0);
}

TEST_CASE("Image generator writes lower-dimensional time-series metadata", "[image-generator][itk][time]")
{
  const std::filesystem::path output = std::filesystem::temp_directory_path() / "entropy-image-generator-2d-time.nrrd";

  auto spec = image_generator::parseSpecJson(R"json({
    "pixel_kind": "scalar",
    "component_type": "float",
    "components": 1,
    "size": [4, 3, 5],
    "spacing": [0.5, 0.75, 2.0],
    "pattern": "time_ramp",
    "metadata": {
      "entropy_time_axis": "last",
      "entropy_time_units": "frame"
    }
  })json");
  spec.output = output;

  image_generator::writeImage(spec);
  REQUIRE(std::filesystem::exists(output));

  using ImageType = itk::Image<float, 3>;
  auto reader = itk::ImageFileReader<ImageType>::New();
  reader->SetFileName(output.string());
  reader->Update();

  const auto* image = reader->GetOutput();
  CHECK(image->GetLargestPossibleRegion().GetSize()[0] == 4);
  CHECK(image->GetLargestPossibleRegion().GetSize()[1] == 3);
  CHECK(image->GetLargestPossibleRegion().GetSize()[2] == 5);

  std::string timeAxis;
  std::string timeUnits;
  REQUIRE(itk::ExposeMetaData<std::string>(image->GetMetaDataDictionary(), "entropy_time_axis", timeAxis));
  REQUIRE(itk::ExposeMetaData<std::string>(image->GetMetaDataDictionary(), "entropy_time_units", timeUnits));
  CHECK(timeAxis == "last");
  CHECK(timeUnits == "frame");
}
