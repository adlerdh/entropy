#include "ImageGenerator.h"

#include <itkImage.h>
#include <itkImageFileWriter.h>
#include <itkImageRegionIterator.h>
#include <itkMetaDataObject.h>
#include <itkRGBAPixel.h>
#include <itkRGBPixel.h>
#include <itkVectorImage.h>
#include <nlohmann/json.hpp>

#include <algorithm>
#include <cmath>
#include <complex>
#include <cstdint>
#include <fstream>
#include <numeric>
#include <sstream>
#include <stdexcept>

namespace image_generator
{
namespace
{
using json = nlohmann::json;

std::string lower(std::string text)
{
  std::ranges::transform(text, text.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  return text;
}

PixelKind pixelKindFromString(std::string text)
{
  text = lower(std::move(text));
  if ("scalar" == text) {
    return PixelKind::Scalar;
  }
  if ("vector" == text || "multicomponent" == text || "multi-component" == text) {
    return PixelKind::Vector;
  }
  if ("complex" == text) {
    return PixelKind::Complex;
  }
  if ("rgb" == text) {
    return PixelKind::RGB;
  }
  if ("rgba" == text) {
    return PixelKind::RGBA;
  }
  throw std::runtime_error("Unknown pixel kind: " + text);
}

Pattern patternFromString(std::string text)
{
  text = lower(std::move(text));
  if ("ramp" == text) {
    return Pattern::Ramp;
  }
  if ("checker" == text || "checkerboard" == text) {
    return Pattern::Checker;
  }
  if ("gaussian" == text) {
    return Pattern::Gaussian;
  }
  if ("component_ramp" == text || "component-ramp" == text || "componentramp" == text) {
    return Pattern::ComponentRamp;
  }
  if ("complex_phase" == text || "complex-phase" == text || "phase" == text) {
    return Pattern::ComplexPhase;
  }
  if ("time_ramp" == text || "time-ramp" == text || "timeramp" == text) {
    return Pattern::TimeRamp;
  }
  if ("temporal_sine" == text || "temporal-sine" == text || "sine" == text) {
    return Pattern::TemporalSine;
  }
  if ("moving_gaussian" == text || "moving-gaussian" == text || "movinggaussian" == text) {
    return Pattern::MovingGaussian;
  }
  if ("pulsing_gaussian" == text || "pulsing-gaussian" == text || "pulsinggaussian" == text) {
    return Pattern::PulsingGaussian;
  }
  if ("two_moving_gaussians" == text || "two-moving-gaussians" == text || "twomovinggaussians" == text) {
    return Pattern::TwoMovingGaussians;
  }
  if ("rotating_wave" == text || "rotating-wave" == text || "rotatingwave" == text) {
    return Pattern::RotatingWave;
  }
  if ("time_varying_warp_field" == text || "time-varying-warp-field" == text || "timevaryingwarpfield" == text) {
    return Pattern::TimeVaryingWarpField;
  }
  if ("sphere" == text || "binary_sphere" == text || "binary-sphere" == text) {
    return Pattern::Sphere;
  }
  throw std::runtime_error("Unknown pattern: " + text);
}

template<typename T>
std::vector<T> optionalVector(const json& value, const char* key, std::vector<T> fallback)
{
  if (!value.contains(key)) {
    return fallback;
  }
  return value.at(key).get<std::vector<T>>();
}

std::map<std::string, std::string> optionalStringMap(const json& value, const char* key)
{
  std::map<std::string, std::string> out;
  if (!value.contains(key)) {
    return out;
  }

  for (const auto& [name, item] : value.at(key).items()) {
    out[name] = item.is_string() ? item.get<std::string>() : item.dump();
  }
  return out;
}

std::size_t dimension(const ImageSpec& spec)
{
  return spec.size.size();
}

std::vector<double> defaults(std::size_t count, double value)
{
  return std::vector<double>(count, value);
}

std::vector<double> defaultDirection(std::size_t dim)
{
  std::vector<double> direction(dim * dim, 0.0);
  for (std::size_t i = 0; i < dim; ++i) {
    direction[(i * dim) + i] = 1.0;
  }
  return direction;
}

double coordinate(const std::vector<std::size_t>& index, const std::vector<std::size_t>& size, std::size_t axis)
{
  if (axis >= index.size() || size.at(axis) <= 1) {
    return 0.0;
  }
  return static_cast<double>(index.at(axis)) / static_cast<double>(size.at(axis) - 1);
}

double physicalCoordinate(const ImageSpec& spec, const std::vector<std::size_t>& index, std::size_t axis)
{
  if (axis >= spec.size.size()) {
    return 0.0;
  }
  return spec.origin.at(axis) + (static_cast<double>(index.at(axis)) * spec.spacing.at(axis));
}

double physicalCenter(const ImageSpec& spec, std::size_t axis)
{
  if (axis >= spec.size.size()) {
    return 0.0;
  }
  return spec.origin.at(axis) + (0.5 * static_cast<double>(spec.size.at(axis) - 1) * spec.spacing.at(axis));
}

std::size_t linearIndex(const std::vector<std::size_t>& index, const std::vector<std::size_t>& size)
{
  std::size_t stride = 1;
  std::size_t linear = 0;
  for (std::size_t axis = 0; axis < index.size(); ++axis) {
    linear += stride * index.at(axis);
    stride *= size.at(axis);
  }
  return linear;
}

double patternValue(const ImageSpec& spec, const std::vector<std::size_t>& index, std::size_t component)
{
  const double x = coordinate(index, spec.size, 0);
  const double y = coordinate(index, spec.size, 1);
  const double z = coordinate(index, spec.size, 2);
  const double t = coordinate(index, spec.size, 3);
  constexpr double k_pi = 3.14159265358979323846;
  constexpr double k_twoPi = 2.0 * k_pi;

  switch (spec.pattern) {
    case Pattern::Ramp:
      return spec.offset + spec.amplitude * (x + 2.0 * y + 3.0 * z + 4.0 * t + static_cast<double>(component));
    case Pattern::Checker: {
      std::size_t parity = component;
      for (const std::size_t value : index) {
        parity += value / 4u;
      }
      return spec.offset + ((parity % 2u == 0u) ? spec.amplitude : -spec.amplitude);
    }
    case Pattern::Gaussian: {
      const double dx = x - 0.5;
      const double dy = y - 0.5;
      const double dz = z - 0.5;
      const double r2 = dx * dx + dy * dy + dz * dz;
      return spec.offset + (1.0 + static_cast<double>(component)) * spec.amplitude * std::exp(-12.0 * r2);
    }
    case Pattern::ComponentRamp:
      return spec.offset + (100.0 * static_cast<double>(component)) +
             static_cast<double>(linearIndex(index, spec.size));
    case Pattern::ComplexPhase:
      return spec.offset + spec.amplitude * std::sin((x + y + z + t) * k_twoPi);
    case Pattern::TimeRamp:
      return spec.offset + spec.amplitude * (10.0 * t + x + 0.5 * y + 0.25 * z + static_cast<double>(component));
    case Pattern::TemporalSine:
      return spec.offset + spec.amplitude * (1.0 + 0.1 * static_cast<double>(component)) *
                             std::sin(k_twoPi * (t + 0.15 * x + 0.10 * y + 0.05 * z));
    case Pattern::MovingGaussian: {
      const double centerX = 0.25 + 0.5 * t;
      const double centerY = 0.5 + 0.2 * std::sin(k_twoPi * t);
      const double centerZ = 0.5;
      const double dx = x - centerX;
      const double dy = y - centerY;
      const double dz = z - centerZ;
      const double r2 = dx * dx + dy * dy + dz * dz;
      return spec.offset + (1.0 + 0.25 * static_cast<double>(component)) * spec.amplitude * std::exp(-18.0 * r2);
    }
    case Pattern::PulsingGaussian: {
      const double radiusScale = 10.0 + 8.0 * (0.5 + 0.5 * std::sin(k_twoPi * t));
      const double intensityScale = 0.6 + 0.4 * std::cos(k_twoPi * t);
      const double dx = x - 0.5;
      const double dy = y - 0.5;
      const double dz = z - 0.5;
      const double r2 = (dx * dx) + (dy * dy) + (dz * dz);
      return spec.offset + spec.amplitude * intensityScale * std::exp(-radiusScale * r2);
    }
    case Pattern::TwoMovingGaussians: {
      const double aX = 0.25 + 0.50 * t;
      const double aY = 0.35 + 0.18 * std::sin(k_twoPi * t);
      const double aZ = 0.35 + 0.12 * std::cos(k_twoPi * t);
      const double bX = 0.75 - 0.45 * t;
      const double bY = 0.65 + 0.16 * std::cos(k_twoPi * t);
      const double bZ = 0.65 - 0.18 * std::sin(k_twoPi * t);
      const double aR2 = (x - aX) * (x - aX) + (y - aY) * (y - aY) + (z - aZ) * (z - aZ);
      const double bR2 = (x - bX) * (x - bX) + (y - bY) * (y - bY) + (z - bZ) * (z - bZ);
      const double blobA = std::exp(-32.0 * aR2);
      const double blobB = std::exp(-24.0 * bR2);
      return spec.offset + spec.amplitude * (blobA - 0.8 * blobB);
    }
    case Pattern::RotatingWave: {
      const double centeredX = x - 0.5;
      const double centeredY = y - 0.5;
      const double angle = std::atan2(centeredY, centeredX);
      const double radius = std::sqrt(centeredX * centeredX + centeredY * centeredY);
      const double axialEnvelope = std::exp(-5.0 * (z - 0.5) * (z - 0.5));
      return spec.offset + spec.amplitude * axialEnvelope * std::sin(10.0 * radius + 3.0 * angle - k_twoPi * t);
    }
    case Pattern::TimeVaryingWarpField: {
      const double px = (2.0 * x) - 1.0;
      const double py = (2.0 * y) - 1.0;
      const double pz = (2.0 * z) - 1.0;
      const double r2 = (px * px) + (py * py) + (pz * pz);
      const double envelope = std::exp(-2.0 * r2);
      const double pulse = std::sin(k_twoPi * t);
      const double twist = std::cos(k_twoPi * t);
      const double expansion = 0.45 * pulse * envelope;
      const double swirl = 0.35 * twist * envelope;
      const double wave = 0.15 * std::sin(k_twoPi * (x + y + z + t));
      switch (component % 3u) {
        case 0:
          return spec.offset + spec.amplitude * ((expansion * px) - (swirl * py) + wave);
        case 1:
          return spec.offset + spec.amplitude * ((expansion * py) + (swirl * px) - 0.5 * wave);
        default:
          return spec.offset + spec.amplitude * ((0.6 * expansion * pz) + (0.25 * swirl * std::sin(k_pi * x)));
      }
    }
    case Pattern::Sphere: {
      double radiusSquared = 0.0;
      for (std::size_t axis = 0; axis < std::min<std::size_t>(3, spec.size.size()); ++axis) {
        const double delta = physicalCoordinate(spec, index, axis) - physicalCenter(spec, axis);
        radiusSquared += delta * delta;
      }
      return radiusSquared <= spec.sphereRadius * spec.sphereRadius ? spec.offset + spec.amplitude : spec.offset;
    }
  }
  return spec.offset;
}

std::complex<double> complexValue(const ImageSpec& spec, const std::vector<std::size_t>& index)
{
  const double x = coordinate(index, spec.size, 0);
  const double y = coordinate(index, spec.size, 1);
  const double z = coordinate(index, spec.size, 2);
  const double t = coordinate(index, spec.size, 3);
  const double phase = 6.283185307179586 * (x + 0.5 * y + 0.25 * z + t);
  const double magnitude = std::max(
    1.0,
    spec.amplitude * (1.0 + 0.35 * std::sin(6.283185307179586 * t)) *
      std::exp(-4.0 * ((x - 0.5) * (x - 0.5) + (y - 0.5) * (y - 0.5))));
  return {spec.offset + magnitude * std::cos(phase), magnitude * std::sin(phase)};
}

template<unsigned int Dimension>
typename itk::ImageBase<Dimension>::DirectionType directionMatrix(const ImageSpec& spec)
{
  typename itk::ImageBase<Dimension>::DirectionType direction;
  direction.SetIdentity();
  for (unsigned int row = 0; row < Dimension; ++row) {
    for (unsigned int col = 0; col < Dimension; ++col) {
      direction[row][col] = spec.direction[(row * Dimension) + col];
    }
  }
  return direction;
}

template<unsigned int Dimension>
typename itk::ImageRegion<Dimension>::SizeType itkSize(const ImageSpec& spec)
{
  typename itk::ImageRegion<Dimension>::SizeType size;
  for (unsigned int axis = 0; axis < Dimension; ++axis) {
    size[axis] = spec.size.at(axis);
  }
  return size;
}

template<unsigned int Dimension>
typename itk::ImageBase<Dimension>::SpacingType itkSpacing(const ImageSpec& spec)
{
  typename itk::ImageBase<Dimension>::SpacingType spacing;
  for (unsigned int axis = 0; axis < Dimension; ++axis) {
    spacing[axis] = spec.spacing.at(axis);
  }
  return spacing;
}

template<unsigned int Dimension>
typename itk::ImageBase<Dimension>::PointType itkOrigin(const ImageSpec& spec)
{
  typename itk::ImageBase<Dimension>::PointType origin;
  for (unsigned int axis = 0; axis < Dimension; ++axis) {
    origin[axis] = spec.origin.at(axis);
  }
  return origin;
}

template<unsigned int Dimension>
std::vector<std::size_t> vectorIndex(const typename itk::ImageRegion<Dimension>::IndexType& index)
{
  std::vector<std::size_t> out(Dimension, 0);
  for (unsigned int axis = 0; axis < Dimension; ++axis) {
    out[axis] = static_cast<std::size_t>(index[axis]);
  }
  return out;
}

template<typename ImageType>
void applyMetadata(const typename ImageType::Pointer& image, const ImageSpec& spec)
{
  for (const auto& [key, value] : spec.metadata) {
    itk::EncapsulateMetaData<std::string>(image->GetMetaDataDictionary(), key, value);
  }
}

template<typename T, unsigned int Dimension>
void writeScalarImage(const ImageSpec& spec)
{
  using ImageType = itk::Image<T, Dimension>;
  auto image = ImageType::New();
  image->SetRegions(itkSize<Dimension>(spec));
  image->SetSpacing(itkSpacing<Dimension>(spec));
  image->SetOrigin(itkOrigin<Dimension>(spec));
  image->SetDirection(directionMatrix<Dimension>(spec));
  image->Allocate();
  applyMetadata<ImageType>(image, spec);

  itk::ImageRegionIterator<ImageType> it(image, image->GetLargestPossibleRegion());
  for (it.GoToBegin(); !it.IsAtEnd(); ++it) {
    it.Set(static_cast<T>(patternValue(spec, vectorIndex<Dimension>(it.GetIndex()), 0)));
  }

  itk::WriteImage(image, spec.output.string());
}

template<typename T, unsigned int Dimension>
void writeVectorImage(const ImageSpec& spec)
{
  using ImageType = itk::VectorImage<T, Dimension>;
  using PixelType = typename ImageType::PixelType;

  auto image = ImageType::New();
  image->SetNumberOfComponentsPerPixel(static_cast<unsigned int>(spec.components));
  image->SetRegions(itkSize<Dimension>(spec));
  image->SetSpacing(itkSpacing<Dimension>(spec));
  image->SetOrigin(itkOrigin<Dimension>(spec));
  image->SetDirection(directionMatrix<Dimension>(spec));
  image->Allocate();
  applyMetadata<ImageType>(image, spec);

  itk::ImageRegionIterator<ImageType> it(image, image->GetLargestPossibleRegion());
  for (it.GoToBegin(); !it.IsAtEnd(); ++it) {
    const std::vector<std::size_t> index = vectorIndex<Dimension>(it.GetIndex());
    PixelType pixel;
    pixel.SetSize(static_cast<unsigned int>(spec.components));
    for (std::size_t component = 0; component < spec.components; ++component) {
      pixel[component] = static_cast<T>(patternValue(spec, index, component));
    }
    it.Set(pixel);
  }

  itk::WriteImage(image, spec.output.string());
}

template<typename T, unsigned int Dimension>
void writeComplexImage(const ImageSpec& spec)
{
  using ImageType = itk::Image<std::complex<T>, Dimension>;
  auto image = ImageType::New();
  image->SetRegions(itkSize<Dimension>(spec));
  image->SetSpacing(itkSpacing<Dimension>(spec));
  image->SetOrigin(itkOrigin<Dimension>(spec));
  image->SetDirection(directionMatrix<Dimension>(spec));
  image->Allocate();
  applyMetadata<ImageType>(image, spec);

  itk::ImageRegionIterator<ImageType> it(image, image->GetLargestPossibleRegion());
  for (it.GoToBegin(); !it.IsAtEnd(); ++it) {
    const std::complex<double> value = complexValue(spec, vectorIndex<Dimension>(it.GetIndex()));
    it.Set({static_cast<T>(value.real()), static_cast<T>(value.imag())});
  }

  itk::WriteImage(image, spec.output.string());
}

template<typename T, unsigned int Dimension>
void writeRgbImage(const ImageSpec& spec)
{
  using PixelType = itk::RGBPixel<T>;
  using ImageType = itk::Image<PixelType, Dimension>;

  auto image = ImageType::New();
  image->SetRegions(itkSize<Dimension>(spec));
  image->SetSpacing(itkSpacing<Dimension>(spec));
  image->SetOrigin(itkOrigin<Dimension>(spec));
  image->SetDirection(directionMatrix<Dimension>(spec));
  image->Allocate();
  applyMetadata<ImageType>(image, spec);

  itk::ImageRegionIterator<ImageType> it(image, image->GetLargestPossibleRegion());
  for (it.GoToBegin(); !it.IsAtEnd(); ++it) {
    const std::vector<std::size_t> index = vectorIndex<Dimension>(it.GetIndex());
    PixelType pixel;
    pixel.SetRed(static_cast<T>(patternValue(spec, index, 0)));
    pixel.SetGreen(static_cast<T>(patternValue(spec, index, 1)));
    pixel.SetBlue(static_cast<T>(patternValue(spec, index, 2)));
    it.Set(pixel);
  }

  itk::WriteImage(image, spec.output.string());
}

template<typename T, unsigned int Dimension>
void writeRgbaImage(const ImageSpec& spec)
{
  using PixelType = itk::RGBAPixel<T>;
  using ImageType = itk::Image<PixelType, Dimension>;

  auto image = ImageType::New();
  image->SetRegions(itkSize<Dimension>(spec));
  image->SetSpacing(itkSpacing<Dimension>(spec));
  image->SetOrigin(itkOrigin<Dimension>(spec));
  image->SetDirection(directionMatrix<Dimension>(spec));
  image->Allocate();
  applyMetadata<ImageType>(image, spec);

  itk::ImageRegionIterator<ImageType> it(image, image->GetLargestPossibleRegion());
  for (it.GoToBegin(); !it.IsAtEnd(); ++it) {
    const std::vector<std::size_t> index = vectorIndex<Dimension>(it.GetIndex());
    PixelType pixel;
    pixel.SetRed(static_cast<T>(patternValue(spec, index, 0)));
    pixel.SetGreen(static_cast<T>(patternValue(spec, index, 1)));
    pixel.SetBlue(static_cast<T>(patternValue(spec, index, 2)));
    pixel.SetAlpha(static_cast<T>(patternValue(spec, index, 3)));
    it.Set(pixel);
  }

  itk::WriteImage(image, spec.output.string());
}

template<typename T, unsigned int Dimension>
void writeTypedImage(const ImageSpec& spec)
{
  switch (spec.pixelKind) {
    case PixelKind::Scalar:
      writeScalarImage<T, Dimension>(spec);
      return;
    case PixelKind::Vector:
      writeVectorImage<T, Dimension>(spec);
      return;
    case PixelKind::Complex:
      if constexpr (std::is_floating_point_v<T>) {
        writeComplexImage<T, Dimension>(spec);
      }
      else {
        throw std::runtime_error("Complex images require a floating-point component type");
      }
      return;
    case PixelKind::RGB:
      writeRgbImage<T, Dimension>(spec);
      return;
    case PixelKind::RGBA:
      writeRgbaImage<T, Dimension>(spec);
      return;
  }
}

template<typename T>
void writeTypedImage(const ImageSpec& spec)
{
  switch (dimension(spec)) {
    case 1:
      writeTypedImage<T, 1>(spec);
      return;
    case 2:
      writeTypedImage<T, 2>(spec);
      return;
    case 3:
      writeTypedImage<T, 3>(spec);
      return;
    case 4:
      writeTypedImage<T, 4>(spec);
      return;
    default:
      throw std::runtime_error("Only 1D, 2D, 3D, and 4D images are supported");
  }
}

} // namespace

ImageSpec parseSpecJson(const std::string& text)
{
  const json j = json::parse(text);

  ImageSpec spec;
  if (j.contains("output")) {
    spec.output = j.at("output").get<std::string>();
  }
  if (j.contains("pixel_kind")) {
    spec.pixelKind = pixelKindFromString(j.at("pixel_kind").get<std::string>());
  }
  else if (j.contains("pixel_type")) {
    spec.pixelKind = pixelKindFromString(j.at("pixel_type").get<std::string>());
  }
  if (j.contains("component_type")) {
    spec.componentType = j.at("component_type").get<std::string>();
  }
  if (j.contains("components")) {
    spec.components = j.at("components").get<std::size_t>();
  }
  spec.size = optionalVector<std::size_t>(j, "size", spec.size);
  spec.spacing = optionalVector<double>(j, "spacing", defaults(spec.size.size(), 1.0));
  spec.origin = optionalVector<double>(j, "origin", defaults(spec.size.size(), 0.0));
  spec.direction = optionalVector<double>(j, "direction", defaultDirection(spec.size.size()));
  if (j.contains("pattern")) {
    spec.pattern = patternFromString(j.at("pattern").get<std::string>());
  }
  spec.amplitude = j.value("amplitude", spec.amplitude);
  spec.offset = j.value("offset", spec.offset);
  spec.sphereRadius = j.value("sphere_radius", spec.sphereRadius);
  spec.metadata = optionalStringMap(j, "metadata");
  return spec;
}

ImageSpec loadSpecJson(const std::filesystem::path& fileName)
{
  std::ifstream in(fileName);
  if (!in) {
    throw std::runtime_error("Cannot open image specification: " + fileName.string());
  }
  std::ostringstream buffer;
  buffer << in.rdbuf();
  ImageSpec spec = parseSpecJson(buffer.str());
  if (spec.output.empty()) {
    spec.output = fileName;
    spec.output.replace_extension(".nrrd");
  }
  return spec;
}

void validateSpec(const ImageSpec& spec)
{
  if (spec.output.empty()) {
    throw std::runtime_error("Output image file is required");
  }
  if (spec.size.empty() || spec.size.size() > 4) {
    throw std::runtime_error("Image size must contain one, two, three, or four dimensions");
  }
  if (std::ranges::any_of(spec.size, [](std::size_t value) { return value == 0; })) {
    throw std::runtime_error("Image dimensions must be greater than zero");
  }
  if (spec.spacing.size() != spec.size.size()) {
    throw std::runtime_error("Spacing length must match image dimension");
  }
  if (spec.origin.size() != spec.size.size()) {
    throw std::runtime_error("Origin length must match image dimension");
  }
  if (spec.direction.size() != spec.size.size() * spec.size.size()) {
    throw std::runtime_error("Direction must contain dimension x dimension row-major values");
  }
  if (PixelKind::Scalar == spec.pixelKind && spec.components != 1) {
    throw std::runtime_error("Scalar images must have exactly one component");
  }
  if (PixelKind::Complex == spec.pixelKind && spec.components != 1) {
    throw std::runtime_error("Complex images store one complex value per pixel; set components to 1");
  }
  if (PixelKind::Vector == spec.pixelKind && spec.components < 2) {
    throw std::runtime_error("Vector images must have at least two components");
  }
  if (PixelKind::RGB == spec.pixelKind && spec.components != 3) {
    throw std::runtime_error("RGB images must have exactly three components");
  }
  if (PixelKind::RGBA == spec.pixelKind && spec.components != 4) {
    throw std::runtime_error("RGBA images must have exactly four components");
  }
  if (PixelKind::Complex == spec.pixelKind) {
    const std::string type = lower(spec.componentType);
    if ("float" != type && "float32" != type && "double" != type && "float64" != type) {
      throw std::runtime_error("Complex images support float or double component types");
    }
  }
  if (Pattern::Sphere == spec.pattern && spec.sphereRadius <= 0.0) {
    throw std::runtime_error("Sphere radius must be positive");
  }
}

double expectedComponentValue(const ImageSpec& spec, const std::vector<std::size_t>& index, std::size_t component)
{
  return patternValue(spec, index, component);
}

std::complex<double> expectedComplexValue(const ImageSpec& spec, const std::vector<std::size_t>& index)
{
  return complexValue(spec, index);
}

void writeImage(const ImageSpec& spec)
{
  validateSpec(spec);

  const std::string type = lower(spec.componentType);
  if ("int8" == type || "char" == type) {
    writeTypedImage<std::int8_t>(spec);
  }
  else if ("uint8" == type || "uchar" == type) {
    writeTypedImage<std::uint8_t>(spec);
  }
  else if ("int16" == type || "short" == type) {
    writeTypedImage<std::int16_t>(spec);
  }
  else if ("uint16" == type || "ushort" == type) {
    writeTypedImage<std::uint16_t>(spec);
  }
  else if ("int32" == type || "int" == type) {
    writeTypedImage<std::int32_t>(spec);
  }
  else if ("uint32" == type || "uint" == type) {
    writeTypedImage<std::uint32_t>(spec);
  }
  else if ("float" == type || "float32" == type) {
    writeTypedImage<float>(spec);
  }
  else if ("double" == type || "float64" == type) {
    writeTypedImage<double>(spec);
  }
  else {
    throw std::runtime_error("Unsupported component type: " + spec.componentType);
  }
}

} // namespace image_generator
