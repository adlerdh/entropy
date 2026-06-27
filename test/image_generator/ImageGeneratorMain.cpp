#include "ImageGenerator.h"

#include <CLI/CLI.hpp>

#include <cstdlib>
#include <exception>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

namespace
{
constexpr const char* kExampleSpec = R"json({
  "output": "synthetic_vector3_4d.nrrd",
  "pixel_kind": "vector",
  "component_type": "float",
  "components": 3,
  "size": [32, 32, 16, 5],
  "spacing": [1.0, 1.0, 2.0, 1.0],
  "origin": [-16.0, -16.0, -16.0, 0.0],
  "pattern": "moving_gaussian",
  "metadata": {
    "entropy_test_image": "true",
    "description": "3-component synthetic vector time series"
  }
})json";
} // namespace

int main(int argc, char** argv)
{
  CLI::App app{"Generate synthetic scalar, complex, and multi-component images for Entropy testing"};

  std::filesystem::path specFileName;
  std::filesystem::path outputFileName;
  std::string pixelKind;
  std::string componentType;
  std::string pattern;
  std::size_t components = 0;
  std::vector<std::size_t> size;
  std::vector<double> spacing;
  std::vector<double> origin;
  std::vector<double> direction;
  bool printExample = false;

  app.add_option("--spec", specFileName, "JSON image specification");
  app.add_option("-o,--output", outputFileName, "Output image file override");
  app.add_option("--pixel-kind,--pixel-type", pixelKind, "Pixel kind override: scalar, vector, complex");
  app.add_option("--component-type,--component", componentType, "Component type override");
  app.add_option("--components", components, "Number of vector components");
  app.add_option("--size,--dimensions", size, "Image size override");
  app.add_option("--spacing", spacing, "Image spacing override");
  app.add_option("--origin", origin, "Image origin override");
  app.add_option("--direction", direction, "Row-major image direction matrix override");
  app.add_option(
    "--pattern",
    pattern,
    "Pattern override: ramp, checker, gaussian, component_ramp, complex_phase, time_ramp, temporal_sine, "
    "moving_gaussian, pulsing_gaussian, two_moving_gaussians, rotating_wave, time_varying_warp_field");
  app.add_flag("--print-example", printExample, "Print an example JSON specification and exit");

  CLI11_PARSE(app, argc, argv);

  if (printExample) {
    std::cout << kExampleSpec << '\n';
    return EXIT_SUCCESS;
  }

  if (specFileName.empty()) {
    std::cerr << "A JSON specification is required. Use --print-example to create a starting point.\n";
    return EXIT_FAILURE;
  }

  try {
    image_generator::ImageSpec spec = image_generator::loadSpecJson(specFileName);
    if (!outputFileName.empty()) {
      spec.output = outputFileName;
    }
    if (!pixelKind.empty()) {
      spec.pixelKind = image_generator::parseSpecJson("{\"pixel_kind\":\"" + pixelKind + "\"}").pixelKind;
    }
    if (!componentType.empty()) {
      spec.componentType = componentType;
    }
    if (components > 0) {
      spec.components = components;
    }
    if (!size.empty()) {
      spec.size = size;
    }
    if (!spacing.empty()) {
      spec.spacing = spacing;
    }
    if (!origin.empty()) {
      spec.origin = origin;
    }
    if (!direction.empty()) {
      spec.direction = direction;
    }
    if (!pattern.empty()) {
      spec.pattern = image_generator::parseSpecJson("{\"pattern\":\"" + pattern + "\"}").pattern;
    }

    image_generator::writeImage(spec);
  }
  catch (const std::exception& e) {
    std::cerr << "Failed to generate image: " << e.what() << '\n';
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
