#include "ImageGenerator.h"

#include <CLI/CLI.hpp>
#include <nlohmann/json.hpp>

#include <filesystem>
#include <fstream>
#include <iostream>
#include <cmath>
#include <stdexcept>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace
{
using image_generator::ImageSpec;
using image_generator::Pattern;
using image_generator::PixelKind;
using json = nlohmann::json;

std::string pixelKindString(PixelKind kind)
{
  switch (kind) {
    case PixelKind::Scalar:
      return "scalar";
    case PixelKind::Vector:
      return "vector";
    case PixelKind::Complex:
      return "complex";
    case PixelKind::RGB:
      return "rgb";
    case PixelKind::RGBA:
      return "rgba";
  }
  return "scalar";
}

std::string patternString(Pattern pattern)
{
  switch (pattern) {
    case Pattern::Ramp:
      return "ramp";
    case Pattern::Checker:
      return "checker";
    case Pattern::Gaussian:
      return "gaussian";
    case Pattern::ComponentRamp:
      return "component_ramp";
    case Pattern::ComplexPhase:
      return "complex_phase";
    case Pattern::TimeRamp:
      return "time_ramp";
    case Pattern::TemporalSine:
      return "temporal_sine";
    case Pattern::MovingGaussian:
      return "moving_gaussian";
    case Pattern::PulsingGaussian:
      return "pulsing_gaussian";
    case Pattern::TwoMovingGaussians:
      return "two_moving_gaussians";
    case Pattern::RotatingWave:
      return "rotating_wave";
    case Pattern::TimeVaryingWarpField:
      return "time_varying_warp_field";
    case Pattern::Sphere:
      return "sphere";
  }
  return "ramp";
}

json specJson(const ImageSpec& spec)
{
  json j;
  j["output"] = spec.output.filename().string();
  j["pixel_kind"] = pixelKindString(spec.pixelKind);
  j["component_type"] = spec.componentType;
  j["components"] = spec.components;
  j["size"] = spec.size;
  j["spacing"] = spec.spacing;
  j["origin"] = spec.origin;
  j["direction"] = spec.direction;
  j["pattern"] = patternString(spec.pattern);
  j["amplitude"] = spec.amplitude;
  j["offset"] = spec.offset;
  if (Pattern::Sphere == spec.pattern) {
    j["sphere_radius"] = spec.sphereRadius;
  }
  j["metadata"] = spec.metadata;
  return j;
}

std::vector<double> identityDirection(std::size_t dim)
{
  std::vector<double> direction(dim * dim, 0.0);
  for (std::size_t i = 0; i < dim; ++i) {
    direction[(i * dim) + i] = 1.0;
  }
  return direction;
}

std::vector<double> rotation2D(double degrees)
{
  constexpr double k_pi = 3.14159265358979323846;
  const double radians = degrees * k_pi / 180.0;
  const double c = std::cos(radians);
  const double s = std::sin(radians);
  return {c, -s, s, c};
}

std::vector<double> oblique3D()
{
  constexpr double k_pi = 3.14159265358979323846;
  const double rz = 22.0 * k_pi / 180.0;
  const double rx = -13.0 * k_pi / 180.0;
  const double cz = std::cos(rz);
  const double sz = std::sin(rz);
  const double cx = std::cos(rx);
  const double sx = std::sin(rx);

  // Row-major Rz * Rx.
  return {
    cz,
    -sz * cx,
    sz * sx,
    sz,
    cz * cx,
    -cz * sx,
    0.0,
    sx,
    cx};
}

ImageSpec makeSpec(
  std::string fileName,
  PixelKind pixelKind,
  std::string componentType,
  std::size_t components,
  std::vector<std::size_t> size,
  std::vector<double> spacing,
  std::vector<double> origin,
  std::vector<double> direction,
  Pattern pattern,
  double amplitude,
  double offset,
  std::string description)
{
  ImageSpec spec;
  spec.output = std::move(fileName);
  spec.pixelKind = pixelKind;
  spec.componentType = std::move(componentType);
  spec.components = components;
  spec.size = std::move(size);
  spec.spacing = std::move(spacing);
  spec.origin = std::move(origin);
  spec.direction = std::move(direction);
  spec.pattern = pattern;
  spec.amplitude = amplitude;
  spec.offset = offset;
  spec.metadata["entropy_example"] = std::move(description);
  spec.metadata["entropy_large_2d_texture_test"] = "true";
  return spec;
}

std::vector<ImageSpec> large2DSpecs()
{
  std::vector<ImageSpec> specs;
  specs.reserve(24);

  specs.push_back(makeSpec(
    "01_scalar_float_1024x768_under3d.nrrd",
    PixelKind::Scalar,
    "float",
    1,
    {1024, 768},
    {0.65, 0.65},
    {-330.0, -250.0},
    identityDirection(2),
    Pattern::Gaussian,
    500.0,
    10.0,
    "true 2D scalar float below common 3D texture limit"));

  specs.push_back(makeSpec(
    "02_scalar_int16_2048x1536_at3dlimit.nii.gz",
    PixelKind::Scalar,
    "int16",
    1,
    {2048, 1536},
    {0.8, 0.8},
    {-819.2, -614.4},
    identityDirection(2),
    Pattern::Checker,
    1200.0,
    1400.0,
    "true 2D signed short at common 3D texture limit"));

  specs.push_back(makeSpec(
    "03_scalar_double_3175x1920_large_xy.nii.gz",
    PixelKind::Scalar,
    "double",
    1,
    {3175, 1920},
    {1.0, 1.0},
    {0.0, 0.0},
    identityDirection(2),
    Pattern::Ramp,
    1000.0,
    0.0,
    "true 2D scalar double exceeding common 3D texture limit"));

  specs.push_back(makeSpec(
    "04_scalar_uint16_1536x3072_large_y.mha",
    PixelKind::Scalar,
    "uint16",
    1,
    {1536, 3072},
    {0.5, 1.25},
    {-384.0, -1920.0},
    identityDirection(2),
    Pattern::Gaussian,
    50000.0,
    128.0,
    "true 2D unsigned short with tall dimension exceeding common 3D texture limit"));

  specs.push_back(makeSpec(
    "05_scalar_uint8_4096x1024_large_x.mhd",
    PixelKind::Scalar,
    "uint8",
    1,
    {4096, 1024},
    {0.25, 0.75},
    {-512.0, -384.0},
    identityDirection(2),
    Pattern::Checker,
    120.0,
    128.0,
    "true 2D unsigned byte with wide dimension exceeding common 3D texture limit"));

  specs.push_back(makeSpec(
    "06_scalar_float_oblique2d_3072x1536.nrrd",
    PixelKind::Scalar,
    "float",
    1,
    {3072, 1536},
    {0.7, 1.1},
    {-1000.0, -600.0},
    rotation2D(18.0),
    Pattern::RotatingWave,
    250.0,
    500.0,
    "true 2D scalar float with in-plane oblique direction"));

  specs.push_back(makeSpec(
    "07_complex_float_3072x1536.nrrd",
    PixelKind::Complex,
    "float",
    1,
    {3072, 1536},
    {0.9, 0.9},
    {-1382.4, -691.2},
    identityDirection(2),
    Pattern::ComplexPhase,
    80.0,
    0.0,
    "true 2D complex float exceeding common 3D texture limit"));

  specs.push_back(makeSpec(
    "08_vector_2comp_float_3072x1536.nrrd",
    PixelKind::Vector,
    "float",
    2,
    {3072, 1536},
    {0.8, 0.8},
    {-1228.8, -614.4},
    identityDirection(2),
    Pattern::ComponentRamp,
    1.0,
    0.0,
    "true 2D two-component vector float exceeding common 3D texture limit"));

  specs.push_back(makeSpec(
    "09_vector_3comp_float_3072x1536_vectorfield.nrrd",
    PixelKind::Vector,
    "float",
    3,
    {3072, 1536},
    {1.0, 1.0},
    {-1536.0, -768.0},
    identityDirection(2),
    Pattern::TimeVaryingWarpField,
    6.0,
    0.0,
    "true 2D three-component vector field exceeding common 3D texture limit"));

  specs.push_back(makeSpec(
    "10_vector_4comp_float_3072x1024.nii.gz",
    PixelKind::Vector,
    "float",
    4,
    {3072, 1024},
    {0.6, 0.6},
    {-921.6, -307.2},
    identityDirection(2),
    Pattern::ComponentRamp,
    1.0,
    0.0,
    "true 2D four-component vector float exceeding common 3D texture limit"));

  specs.push_back(makeSpec(
    "11_vector_5comp_float_2560x1024.nrrd",
    PixelKind::Vector,
    "float",
    5,
    {2560, 1024},
    {1.4, 0.7},
    {-1792.0, -358.4},
    identityDirection(2),
    Pattern::ComponentRamp,
    1.0,
    0.0,
    "true 2D five-component vector float exceeding common 3D texture limit"));

  specs.push_back(makeSpec(
    "12_rgb_uint8_3072x1536.nrrd",
    PixelKind::RGB,
    "uint8",
    3,
    {3072, 1536},
    {1.0, 1.0},
    {-1536.0, -768.0},
    identityDirection(2),
    Pattern::ComponentRamp,
    1.0,
    0.0,
    "true 2D RGB uint8 exceeding common 3D texture limit"));

  specs.push_back(makeSpec(
    "13_rgba_uint8_3072x1536.nrrd",
    PixelKind::RGBA,
    "uint8",
    4,
    {3072, 1536},
    {1.0, 1.0},
    {-1536.0, -768.0},
    identityDirection(2),
    Pattern::ComponentRamp,
    1.0,
    64.0,
    "true 2D RGBA uint8 exceeding common 3D texture limit"));

  specs.push_back(makeSpec(
    "14_slab_xy_float_3072x1536x1_oblique.nrrd",
    PixelKind::Scalar,
    "float",
    1,
    {3072, 1536, 1},
    {0.5, 0.7, 2.0},
    {-768.0, -537.6, 12.0},
    oblique3D(),
    Pattern::Gaussian,
    700.0,
    10.0,
    "3D singleton XY slab with oblique 3D direction exceeding common 3D texture limit"));

  specs.push_back(makeSpec(
    "15_slab_xz_float_3072x1x1536_coronal.nrrd",
    PixelKind::Scalar,
    "float",
    1,
    {3072, 1, 1536},
    {0.8, 2.0, 0.8},
    {-1228.8, 18.0, -614.4},
    identityDirection(3),
    Pattern::RotatingWave,
    300.0,
    400.0,
    "3D singleton Y coronal-like slab exercising x-z 2D texture axes"));

  specs.push_back(makeSpec(
    "16_slab_yz_float_1x3072x1536_sagittal.nrrd",
    PixelKind::Scalar,
    "float",
    1,
    {1, 3072, 1536},
    {2.0, 0.8, 0.8},
    {24.0, -1228.8, -614.4},
    identityDirection(3),
    Pattern::RotatingWave,
    300.0,
    400.0,
    "3D singleton X sagittal-like slab exercising y-z 2D texture axes"));

  specs.push_back(makeSpec(
    "17_time_scalar_float_3072x1024x1x4.nii.gz",
    PixelKind::Scalar,
    "float",
    1,
    {3072, 1024, 1, 4},
    {0.7, 0.7, 1.0, 0.25},
    {-1075.2, -358.4, 0.0, 0.0},
    identityDirection(4),
    Pattern::MovingGaussian,
    1000.0,
    0.0,
    "2D scalar NIfTI time series exceeding common 3D texture limit"));

  specs.push_back(makeSpec(
    "18_time_complex_float_2048x1024x5.nrrd",
    PixelKind::Complex,
    "float",
    1,
    {2048, 1024, 5},
    {1.0, 1.0, 0.5},
    {-1024.0, -512.0, 0.0},
    identityDirection(3),
    Pattern::ComplexPhase,
    100.0,
    0.0,
    "2D complex NRRD time series using entropy_time_axis metadata"));
  specs.back().metadata["entropy_time_axis"] = "last";
  specs.back().metadata["entropy_time_units"] = "sec";

  specs.push_back(makeSpec(
    "19_time_vector3_float_2048x1024x4.nrrd",
    PixelKind::Vector,
    "float",
    3,
    {2048, 1024, 4},
    {1.0, 1.0, 0.2},
    {-1024.0, -512.0, 0.0},
    identityDirection(3),
    Pattern::TimeVaryingWarpField,
    8.0,
    0.0,
    "2D three-component vector-field time series using entropy_time_axis metadata"));
  specs.back().metadata["entropy_time_axis"] = "last";
  specs.back().metadata["entropy_time_units"] = "sec";

  specs.push_back(makeSpec(
    "20_time_vector5_float_1536x768x4.nrrd",
    PixelKind::Vector,
    "float",
    5,
    {1536, 768, 4},
    {1.2, 1.2, 1.0},
    {-921.6, -460.8, 0.0},
    identityDirection(3),
    Pattern::TimeRamp,
    20.0,
    0.0,
    "2D five-component vector time series below common 3D texture limit"));
  specs.back().metadata["entropy_time_axis"] = "last";
  specs.back().metadata["entropy_time_units"] = "frame";

  return specs;
}

void writeSpecJson(const fs::path& fileName, const ImageSpec& spec)
{
  std::ofstream out(fileName);
  if (!out) {
    throw std::runtime_error("Cannot write spec file: " + fileName.string());
  }
  out << specJson(spec).dump(2) << '\n';
}

} // namespace

int main(int argc, char** argv)
{
  fs::path outputDir = fs::current_path() / "large-2d-image-examples";
  bool specsOnly = false;

  CLI::App app{"Generate large planar 2D synthetic images for Entropy rendering tests"};
  app.add_option("-o,--output-dir", outputDir, "Directory for generated images and JSON specs");
  app.add_flag("--specs-only", specsOnly, "Write JSON specs without generating image files");
  CLI11_PARSE(app, argc, argv);

  try {
    fs::create_directories(outputDir);
    const fs::path specDir = outputDir / "specs";
    fs::create_directories(specDir);

    for (ImageSpec spec : large2DSpecs()) {
      const fs::path outputFile = outputDir / spec.output;
      spec.output = outputFile;
      const fs::path specFile = specDir / (outputFile.filename().string() + ".json");
      writeSpecJson(specFile, spec);

      if (!specsOnly) {
        std::cout << "Generating " << outputFile << '\n';
        image_generator::writeImage(spec);
      }
    }
  }
  catch (const std::exception& e) {
    std::cerr << "Failed to generate large 2D images: " << e.what() << '\n';
    return 1;
  }

  return 0;
}
