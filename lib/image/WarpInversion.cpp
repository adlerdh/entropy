#include "image/WarpInversion.h"

#include "image/ImageHeader.h"
#include "image/ImageIoInfo.h"

#include <itkCommand.h>
#include <itkImageRegionConstIteratorWithIndex.h>
#include <itkImageRegionIterator.h>
#include <itkInvertDisplacementFieldImageFilter.h>
#include <itkMultiThreaderBase.h>
#include <itkVector.h>
#include <itkVectorLinearInterpolateImageFunction.h>

#include <glm/glm.hpp>

#include <spdlog/spdlog.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <format>
#include <limits>
#include <memory>
#include <numeric>
#include <vector>

namespace
{
using VectorPixel = itk::Vector<float, 3>;
using FieldImage = itk::Image<VectorPixel, 3>;
using Inverter = itk::InvertDisplacementFieldImageFilter<FieldImage, FieldImage>;

uint32_t componentSizeInBytes(ComponentType componentType)
{
  switch (componentType) {
    case ComponentType::Float32:
      return 4;
    default:
      return 0;
  }
}

VectorPixel makeVectorPixel(float x, float y, float z)
{
  VectorPixel value;
  value[0] = x;
  value[1] = y;
  value[2] = z;
  return value;
}

ImageIoInfo makeWarpIoInfo(const Image& domain, const std::string& displayName)
{
  const ImageHeader& domainHeader = domain.header();
  ImageIoInfo info;
  info.m_fileInfo.m_fileName = displayName + ".nrrd";
  info.m_fileInfo.m_fileTypeString = "Nrrd";
  info.m_fileInfo.m_useCompression = false;

  info.m_componentInfo.m_componentType = ComponentType::Float32;
  info.m_componentInfo.m_componentTypeString = componentTypeString(ComponentType::Float32);
  info.m_componentInfo.m_componentSizeInBytes = componentSizeInBytes(ComponentType::Float32);

  info.m_pixelInfo.m_pixelType = PixelType::Vector;
  info.m_pixelInfo.m_pixelTypeString = "vector";
  info.m_pixelInfo.m_numComponents = 3;
  info.m_pixelInfo.m_pixelStrideInBytes =
    info.m_componentInfo.m_componentSizeInBytes * info.m_pixelInfo.m_numComponents;

  const glm::uvec3 dims = domainHeader.pixelDimensions();
  info.m_sizeInfo.m_imageSizeInPixels = static_cast<std::size_t>(dims.x) * dims.y * dims.z;
  info.m_sizeInfo.m_imageSizeInComponents = info.m_sizeInfo.m_imageSizeInPixels * info.m_pixelInfo.m_numComponents;
  info.m_sizeInfo.m_imageSizeInBytes =
    info.m_sizeInfo.m_imageSizeInComponents * info.m_componentInfo.m_componentSizeInBytes;

  info.m_spaceInfo.m_numDimensions = 3;
  info.m_spaceInfo.m_dimensions = {dims.x, dims.y, dims.z};
  info.m_spaceInfo.m_origin = {
    static_cast<double>(domainHeader.origin().x),
    static_cast<double>(domainHeader.origin().y),
    static_cast<double>(domainHeader.origin().z)};
  info.m_spaceInfo.m_spacing = {
    static_cast<double>(domainHeader.spacing().x),
    static_cast<double>(domainHeader.spacing().y),
    static_cast<double>(domainHeader.spacing().z)};
  info.m_spaceInfo.m_directions = {
    {static_cast<double>(domainHeader.directions()[0].x),
     static_cast<double>(domainHeader.directions()[0].y),
     static_cast<double>(domainHeader.directions()[0].z)},
    {static_cast<double>(domainHeader.directions()[1].x),
     static_cast<double>(domainHeader.directions()[1].y),
     static_cast<double>(domainHeader.directions()[1].z)},
    {static_cast<double>(domainHeader.directions()[2].x),
     static_cast<double>(domainHeader.directions()[2].y),
     static_cast<double>(domainHeader.directions()[2].z)}};

  return info;
}

FieldImage::Pointer makeFieldLikeDomain(const Image& domain)
{
  const ImageHeader& header = domain.header();
  const glm::uvec3 dims = header.pixelDimensions();

  FieldImage::IndexType start;
  start.Fill(0);

  FieldImage::SizeType size;
  size[0] = dims.x;
  size[1] = dims.y;
  size[2] = dims.z;

  FieldImage::RegionType region;
  region.SetIndex(start);
  region.SetSize(size);

  FieldImage::PointType origin;
  FieldImage::SpacingType spacing;
  FieldImage::DirectionType direction;
  for (unsigned int i = 0; i < 3; ++i) {
    origin[i] = header.origin()[static_cast<int>(i)];
    spacing[i] = header.spacing()[static_cast<int>(i)];
    for (unsigned int j = 0; j < 3; ++j) {
      direction[i][j] = header.directions()[static_cast<int>(j)][static_cast<int>(i)];
    }
  }

  auto field = FieldImage::New();
  field->SetRegions(region);
  field->SetOrigin(origin);
  field->SetSpacing(spacing);
  field->SetDirection(direction);
  field->Allocate();
  field->FillBuffer(VectorPixel{0.0f});
  return field;
}

entropy_expected::expected<FieldImage::Pointer, std::string> makeFieldFromImage(const Image& image)
{
  if (!image.hasPixelData()) {
    return entropy_expected::unexpected("Warp field has no loaded pixel data");
  }
  if (image.header().numComponentsPerPixel() < 3) {
    return entropy_expected::unexpected("Warp field must have at least three components");
  }

  FieldImage::Pointer field = makeFieldLikeDomain(image);
  itk::ImageRegionIterator<FieldImage> it(field, field->GetLargestPossibleRegion());
  for (it.GoToBegin(); !it.IsAtEnd(); ++it) {
    const FieldImage::IndexType index = it.GetIndex();
    const uint64_t linearIndex =
      static_cast<uint64_t>(index[0]) +
      static_cast<uint64_t>(image.header().pixelDimensions().x) *
        (static_cast<uint64_t>(index[1]) +
         static_cast<uint64_t>(image.header().pixelDimensions().y) * static_cast<uint64_t>(index[2]));

    const auto dx = image.value<float>(0, linearIndex);
    const auto dy = image.value<float>(1, linearIndex);
    const auto dz = image.value<float>(2, linearIndex);
    if (!dx || !dy || !dz) {
      return entropy_expected::unexpected(std::format("Unable to read warp value at pixel {}", linearIndex));
    }
    it.Set(makeVectorPixel(*dx, *dy, *dz));
  }

  return field;
}

Image makeImageFromField(const FieldImage::Pointer& field, const Image& outputDomain, const std::string& displayName)
{
  const ImageHeader header(makeWarpIoInfo(outputDomain, displayName), makeWarpIoInfo(outputDomain, displayName), false);
  const uint64_t numPixels = header.numPixels();

  std::vector<float> x(numPixels, 0.0f);
  std::vector<float> y(numPixels, 0.0f);
  std::vector<float> z(numPixels, 0.0f);

  itk::ImageRegionConstIteratorWithIndex<FieldImage> it(field, field->GetLargestPossibleRegion());
  for (it.GoToBegin(); !it.IsAtEnd(); ++it) {
    const FieldImage::IndexType index = it.GetIndex();
    const uint64_t linearIndex =
      static_cast<uint64_t>(index[0]) +
      static_cast<uint64_t>(header.pixelDimensions().x) *
        (static_cast<uint64_t>(index[1]) +
         static_cast<uint64_t>(header.pixelDimensions().y) * static_cast<uint64_t>(index[2]));
    const VectorPixel value = it.Get();
    x.at(linearIndex) = value[0];
    y.at(linearIndex) = value[1];
    z.at(linearIndex) = value[2];
  }

  std::vector<const void*> buffers{x.data(), y.data(), z.data()};
  Image image(
    header,
    displayName,
    Image::ImageRepresentation::Image,
    Image::MultiComponentBufferType::SeparateImages,
    buffers);
  image.header().setExistsOnDisk(false);
  return image;
}

double minSpacing(const Image& image)
{
  const glm::vec3 spacing = image.header().spacing();
  return static_cast<double>(std::max(std::min({spacing.x, spacing.y, spacing.z}), std::numeric_limits<float>::min()));
}

std::optional<VectorPixel> interpolateField(const FieldImage* field, const FieldImage::PointType& point)
{
  using Interpolator = itk::VectorLinearInterpolateImageFunction<FieldImage, double>;
  typename Interpolator::Pointer interpolator = Interpolator::New();
  interpolator->SetInputImage(field);
  if (!interpolator->IsInsideBuffer(point)) {
    return std::nullopt;
  }
  return interpolator->Evaluate(point);
}

WarpInversionReport computeReport(
  const FieldImage* sourceField,
  const FieldImage* computedField,
  const Image& outputDomain,
  double itkMeanError,
  double itkMaxError,
  double elapsedSeconds)
{
  WarpInversionReport report;
  report.itkMeanErrorNorm = itkMeanError;
  report.itkMaxErrorNorm = itkMaxError;
  report.elapsedSeconds = elapsedSeconds;

  const double spacingScale = minSpacing(outputDomain);
  itk::ImageRegionConstIteratorWithIndex<FieldImage> it(computedField, computedField->GetLargestPossibleRegion());
  for (it.GoToBegin(); !it.IsAtEnd(); ++it) {
    const VectorPixel computed = it.Get();
    FieldImage::PointType point;
    computedField->TransformIndexToPhysicalPoint(it.GetIndex(), point);

    FieldImage::PointType oppositePoint = point;
    for (unsigned int d = 0; d < 3; ++d) {
      oppositePoint[d] += computed[d];
    }

    const std::optional<VectorPixel> source = interpolateField(sourceField, oppositePoint);
    if (!source) {
      ++report.outsideOppositeField;
      continue;
    }

    double residualSquared = 0.0;
    for (unsigned int d = 0; d < 3; ++d) {
      residualSquared += std::pow(static_cast<double>(computed[d] + (*source)[d]), 2.0);
    }

    const double residualMm = std::sqrt(residualSquared);
    report.meanResidualMm += residualMm;
    report.maxResidualMm = std::max(report.maxResidualMm, residualMm);
    report.meanResidualVoxels += residualMm / spacingScale;
    report.maxResidualVoxels = std::max(report.maxResidualVoxels, residualMm / spacingScale);
    ++report.samples;
  }

  if (report.samples > 0) {
    report.meanResidualMm /= static_cast<double>(report.samples);
    report.meanResidualVoxels /= static_cast<double>(report.samples);
  }

  return report;
}

std::string displayDirectionName(ComputedWarpDirection direction)
{
  return ComputedWarpDirection::Inverse == direction ? "Computed inverse warp" : "Computed forward warp";
}

unsigned int defaultWarpInversionWorkUnits()
{
  const unsigned int available = itk::MultiThreaderBase::GetGlobalDefaultNumberOfThreads();
  return std::max(1u, available > 1u ? available - 1u : available);
}

class ProgressCommand : public itk::Command
{
public:
  using Self = ProgressCommand;
  using Pointer = itk::SmartPointer<Self>;
  itkNewMacro(Self);

  void SetProgressCallback(std::function<void(double)> callback)
  {
    m_callback = std::move(callback);
  }
  void SetCancelFlag(const std::atomic_bool* cancel)
  {
    m_cancel = cancel;
  }

  void Execute(itk::Object* caller, const itk::EventObject& event) override
  {
    Execute(static_cast<const itk::Object*>(caller), event);
  }

  void Execute(const itk::Object* caller, const itk::EventObject& event) override
  {
    if (!itk::ProgressEvent().CheckEvent(&event)) {
      return;
    }

    const auto* process = dynamic_cast<const itk::ProcessObject*>(caller);
    if (!process) {
      return;
    }

    if (m_callback) {
      m_callback(std::clamp(static_cast<double>(process->GetProgress()), 0.0, 1.0));
    }

    if (m_cancel && m_cancel->load()) {
      const_cast<itk::ProcessObject*>(process)->AbortGenerateDataOn();
    }
  }

private:
  std::function<void(double)> m_callback;
  const std::atomic_bool* m_cancel = nullptr;
};
} // namespace

std::string computedWarpDisplayName(const Image& sourceWarp, ComputedWarpDirection direction)
{
  return std::format("{} - {}", displayDirectionName(direction), sourceWarp.settings().displayName());
}

entropy_expected::expected<WarpInversionResult, std::string> computeMatchingWarp(
  const Image& sourceWarp,
  const Image& outputDomain,
  ComputedWarpDirection direction,
  const WarpInversionOptions& options,
  const std::function<void(double)>& progress,
  const std::atomic_bool* cancel)
{
  const auto sourceField = makeFieldFromImage(sourceWarp);
  if (!sourceField) {
    return entropy_expected::unexpected(sourceField.error());
  }

  FieldImage::Pointer initialEstimate = makeFieldLikeDomain(outputDomain);
  const std::string displayName = computedWarpDisplayName(sourceWarp, direction);

  try {
    const auto start = std::chrono::steady_clock::now();

    typename Inverter::Pointer inverter = Inverter::New();
    inverter->SetDisplacementField(*sourceField);
    inverter->SetInverseFieldInitialEstimate(initialEstimate);
    inverter->SetMaximumNumberOfIterations(options.maxIterations);
    inverter->SetMeanErrorToleranceThreshold(static_cast<float>(options.meanErrorTolerance));
    inverter->SetMaxErrorToleranceThreshold(static_cast<float>(options.maxErrorTolerance));
    inverter->SetEnforceBoundaryCondition(options.enforceBoundaryCondition);
    inverter->SetNumberOfWorkUnits(defaultWarpInversionWorkUnits());

    ProgressCommand::Pointer progressCommand = ProgressCommand::New();
    progressCommand->SetProgressCallback(progress);
    progressCommand->SetCancelFlag(cancel);
    inverter->AddObserver(itk::ProgressEvent(), progressCommand);

    inverter->Update();
    if (cancel && cancel->load()) {
      return entropy_expected::unexpected("Warp inversion was canceled");
    }

    FieldImage::Pointer output = inverter->GetOutput();
    output->DisconnectPipeline();

    const auto finish = std::chrono::steady_clock::now();
    const double elapsedSeconds = std::chrono::duration<double>(finish - start).count();

    WarpInversionReport report = computeReport(
      sourceField->GetPointer(),
      output.GetPointer(),
      outputDomain,
      inverter->GetMeanErrorNorm(),
      inverter->GetMaxErrorNorm(),
      elapsedSeconds);

    if (progress) {
      progress(1.0);
    }

    return WarpInversionResult{makeImageFromField(output, outputDomain, displayName), report, direction};
  }
  catch (const itk::ExceptionObject& e) {
    return entropy_expected::unexpected(std::format("ITK warp inversion failed: {}", e.what()));
  }
  catch (const std::exception& e) {
    return entropy_expected::unexpected(std::format("Warp inversion failed: {}", e.what()));
  }
}
