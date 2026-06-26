#include "ImageGenerator.h"

#include <itkImage.h>
#include <itkImageFileWriter.h>
#include <itkImageRegionIterator.h>
#include <itkMetaDataObject.h>
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

  switch (spec.pattern) {
    case Pattern::Ramp:
      return spec.offset + spec.amplitude * (x + 2.0 * y + 3.0 * z + static_cast<double>(component));
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
      return spec.offset + spec.amplitude * std::sin((x + y + z) * 6.283185307179586);
  }
  return spec.offset;
}

std::complex<double> complexValue(const ImageSpec& spec, const std::vector<std::size_t>& index)
{
  const double x = coordinate(index, spec.size, 0);
  const double y = coordinate(index, spec.size, 1);
  const double z = coordinate(index, spec.size, 2);
  const double phase = 6.283185307179586 * (x + 0.5 * y + 0.25 * z);
  const double magnitude =
    std::max(1.0, spec.amplitude * std::exp(-4.0 * ((x - 0.5) * (x - 0.5) + (y - 0.5) * (y - 0.5))));
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
void applyMetadata(typename ImageType::Pointer image, const ImageSpec& spec)
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
      writeComplexImage<T, Dimension>(spec);
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
    default:
      throw std::runtime_error("Only 1D, 2D, and 3D images are supported");
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
  if (spec.size.empty() || spec.size.size() > 3) {
    throw std::runtime_error("Image size must contain one, two, or three dimensions");
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
  if (PixelKind::Complex == spec.pixelKind) {
    const std::string type = lower(spec.componentType);
    if ("float" != type && "float32" != type && "double" != type && "float64" != type) {
      throw std::runtime_error("Complex images support float or double component types");
    }
  }
}

void writeImage(const ImageSpec& spec)
{
  validateSpec(spec);

  const std::string type = lower(spec.componentType);
  if ("uint8" == type || "uchar" == type) {
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
