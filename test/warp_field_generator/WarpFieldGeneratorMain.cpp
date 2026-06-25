#include "WarpFieldGenerator.h"

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
  "output": "synthetic_warp.nii.gz",
  "component_type": "float",
  "spatial_dimension": 3,
  "vector_dimension": 3,
  "size": [64, 64, 64],
  "spacing": [1.0, 1.0, 1.0],
  "origin": [-32.0, -32.0, -32.0],
  "direction": [
    1.0, 0.0, 0.0,
    0.0, 1.0, 0.0,
    0.0, 0.0, 1.0
  ],
  "operations": [
    {
      "type": "expansion",
      "center": [0.0, 0.0, 0.0],
      "radii": [18.0, 24.0, 30.0],
      "amplitude": 4.0
    },
    {
      "type": "twist",
      "center": [0.0, 0.0, 0.0],
      "axis": [0.0, 0.0, 1.0],
      "radii": [28.0, 28.0, 28.0],
      "amplitude": 0.5
    }
  ]
})json";

constexpr const char* kExampleTimeSpec = R"json({
  "output": "pulsing_warp_4d.nii.gz",
  "component_type": "float",
  "spatial_dimension": 3,
  "vector_dimension": 3,
  "size": [64, 64, 64],
  "time_points": 12,
  "spacing": [1.0, 1.0, 1.0],
  "origin": [-32.0, -32.0, -32.0],
  "operations": [
    {
      "type": "contraction",
      "center": [0.0, 0.0, 0.0],
      "radii": [22.0, 22.0, 22.0],
      "amplitude": 5.0,
      "time": {
        "mode": "sine",
        "amplitude": 1.0,
        "frequency": 1.0
      }
    },
    {
      "type": "wave",
      "normal": [1.0, 0.0, 0.0],
      "direction": [0.0, 1.0, 0.0],
      "frequency": 0.04,
      "amplitude": 2.0,
      "time": {
        "mode": "ramp",
        "amplitude": 1.0
      }
    }
  ]
})json";
} // namespace

int main(int argc, char** argv)
{
  CLI::App app{"Generate synthetic ITK deformation fields for Entropy testing"};

  std::filesystem::path specFileName;
  std::filesystem::path outputFileName;
  std::string componentType;
  std::vector<std::size_t> size;
  std::vector<double> spacing;
  std::vector<double> origin;
  std::vector<double> direction;
  bool printExample = false;
  bool printTimeExample = false;

  app.add_option("--spec", specFileName, "JSON warp field specification");
  app.add_option("-o,--output", outputFileName, "Output image file override");
  app.add_option("--component-type,--component", componentType, "Component type override: float, double, int16, uint16, int32, uint32");
  app.add_option("--size,--dimensions", size, "Image size override");
  app.add_option("--spacing", spacing, "Image spacing override");
  app.add_option("--origin", origin, "Image origin override");
  app.add_option("--direction", direction, "Row-major image direction matrix override");
  app.add_flag("--print-example", printExample, "Print a 3D example JSON specification and exit");
  app.add_flag("--print-time-example", printTimeExample, "Print a time-dependent 4D example JSON specification and exit");

  CLI11_PARSE(app, argc, argv);

  if (printExample) {
    std::cout << kExampleSpec << '\n';
    return EXIT_SUCCESS;
  }

  if (printTimeExample) {
    std::cout << kExampleTimeSpec << '\n';
    return EXIT_SUCCESS;
  }

  if (specFileName.empty()) {
    std::cerr << "A JSON specification is required. Use --print-example to create a starting point.\n";
    return EXIT_FAILURE;
  }

  try {
    warp_field::WarpFieldSpec spec = warp_field::loadSpecJson(specFileName);

    if (!outputFileName.empty()) {
      spec.output = outputFileName;
    }
    if (!componentType.empty()) {
      spec.componentType = componentType;
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

    warp_field::writeWarpField(spec);
  }
  catch (const std::exception& e) {
    std::cerr << "Failed to generate warp field: " << e.what() << '\n';
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
