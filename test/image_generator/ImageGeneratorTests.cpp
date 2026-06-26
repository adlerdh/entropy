#include "ImageGenerator.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>
#include <itkImageFileReader.h>
#include <itkVectorImage.h>

#include <filesystem>

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
