#include "registration/Types.h"

#include <array>
#include <stdexcept>
#include <string>
#include <utility>

namespace registration
{
namespace
{

template<typename Enum, std::size_t Size>
std::string_view enumLabel(Enum value, const std::array<std::pair<Enum, std::string_view>, Size>& entries)
{
  for (const auto& [enumValue, enumLabelText] : entries) {
    if (value == enumValue) {
      return enumLabelText;
    }
  }
  return "Unknown";
}

template<typename Enum, std::size_t Size>
Enum enumFromString(std::string_view value, const std::array<std::pair<Enum, std::string_view>, Size>& entries)
{
  for (const auto& [enumValue, enumLabelText] : entries) {
    if (value == enumLabelText) {
      return enumValue;
    }
  }
  throw std::invalid_argument("Unsupported registration enum value: " + std::string{value});
}

constexpr std::array backendLabels{
  std::pair{Backend::Greedy, std::string_view{"Greedy"}},
  std::pair{Backend::ANTs, std::string_view{"ANTs"}},
  std::pair{Backend::FireANTs, std::string_view{"FireANTs"}}};

constexpr std::array transformModelLabels{
  std::pair{TransformModel::Rigid, std::string_view{"Rigid"}},
  std::pair{TransformModel::Affine, std::string_view{"Affine"}},
  std::pair{TransformModel::RigidAffine, std::string_view{"RigidAffine"}},
  std::pair{TransformModel::Deformable, std::string_view{"Deformable"}},
  std::pair{TransformModel::AffineDeformable, std::string_view{"AffineDeformable"}},
  std::pair{TransformModel::Translation, std::string_view{"Translation"}},
  std::pair{TransformModel::Similarity, std::string_view{"Similarity"}},
  std::pair{TransformModel::BSplineDisplacement, std::string_view{"BSplineDisplacement"}},
  std::pair{TransformModel::GaussianDisplacement, std::string_view{"GaussianDisplacement"}},
  std::pair{TransformModel::TimeVaryingVelocity, std::string_view{"TimeVaryingVelocity"}}};

constexpr std::array metricLabels{
  std::pair{Metric::SSD, std::string_view{"SSD"}},
  std::pair{Metric::MSE, std::string_view{"MSE"}},
  std::pair{Metric::MI, std::string_view{"MI"}},
  std::pair{Metric::NMI, std::string_view{"NMI"}},
  std::pair{Metric::NCC, std::string_view{"NCC"}},
  std::pair{Metric::WNCC, std::string_view{"WNCC"}},
  std::pair{Metric::CC, std::string_view{"CC"}},
  std::pair{Metric::Mattes, std::string_view{"Mattes"}},
  std::pair{Metric::Demons, std::string_view{"Demons"}},
  std::pair{Metric::GC, std::string_view{"GC"}},
  std::pair{Metric::FusedCC, std::string_view{"FusedCC"}},
  std::pair{Metric::FusedMI, std::string_view{"FusedMI"}},
  std::pair{Metric::MaskedCC, std::string_view{"MaskedCC"}},
  std::pair{Metric::MaskedMSE, std::string_view{"MaskedMSE"}},
  std::pair{Metric::LabelGuided, std::string_view{"LabelGuided"}},
  std::pair{Metric::PointSet, std::string_view{"PointSet"}}};

constexpr std::array featureLabels{
  std::pair{Feature::FixedMovingImages, std::string_view{"FixedMovingImages"}},
  std::pair{Feature::FixedMask, std::string_view{"FixedMask"}},
  std::pair{Feature::MovingMask, std::string_view{"MovingMask"}},
  std::pair{Feature::AuxiliaryImagePairs, std::string_view{"AuxiliaryImagePairs"}},
  std::pair{Feature::SegmentationLabelMetric, std::string_view{"SegmentationLabelMetric"}},
  std::pair{Feature::LandmarkTransform, std::string_view{"LandmarkTransform"}},
  std::pair{Feature::LandmarkDrivenRegistration, std::string_view{"LandmarkDrivenRegistration"}},
  std::pair{Feature::SurfaceTransform, std::string_view{"SurfaceTransform"}},
  std::pair{Feature::SurfaceDrivenRegistration, std::string_view{"SurfaceDrivenRegistration"}},
  std::pair{Feature::InverseWarpOutput, std::string_view{"InverseWarpOutput"}},
  std::pair{Feature::ForwardWarpOutput, std::string_view{"ForwardWarpOutput"}},
  std::pair{Feature::WarpedSegmentationOutput, std::string_view{"WarpedSegmentationOutput"}},
  std::pair{Feature::StructuredProgress, std::string_view{"StructuredProgress"}}};

constexpr std::array parameterKindLabels{
  std::pair{ParameterKind::Boolean, std::string_view{"Boolean"}},
  std::pair{ParameterKind::Integer, std::string_view{"Integer"}},
  std::pair{ParameterKind::Float, std::string_view{"Float"}},
  std::pair{ParameterKind::Text, std::string_view{"Text"}},
  std::pair{ParameterKind::Choice, std::string_view{"Choice"}},
  std::pair{ParameterKind::IntegerVector, std::string_view{"IntegerVector"}},
  std::pair{ParameterKind::FloatVector, std::string_view{"FloatVector"}},
  std::pair{ParameterKind::FilePath, std::string_view{"FilePath"}},
  std::pair{ParameterKind::DirectoryPath, std::string_view{"DirectoryPath"}}};

constexpr std::array dataSourceLabels{
  std::pair{DataSource::None, std::string_view{"None"}},
  std::pair{DataSource::LoadedImage, std::string_view{"LoadedImage"}},
  std::pair{DataSource::Segmentation, std::string_view{"Segmentation"}},
  std::pair{DataSource::LandmarkGroup, std::string_view{"LandmarkGroup"}},
  std::pair{DataSource::AnnotationSet, std::string_view{"AnnotationSet"}},
  std::pair{DataSource::Surface, std::string_view{"Surface"}},
  std::pair{DataSource::ExternalFile, std::string_view{"ExternalFile"}}};

constexpr std::array interpolationLabels{
  std::pair{Interpolation::Linear, std::string_view{"Linear"}},
  std::pair{Interpolation::NearestNeighbor, std::string_view{"NearestNeighbor"}},
  std::pair{Interpolation::Label, std::string_view{"Label"}},
  std::pair{Interpolation::Cubic, std::string_view{"Cubic"}}};

constexpr std::array jobStatusLabels{
  std::pair{JobStatus::Queued, std::string_view{"Queued"}},
  std::pair{JobStatus::PreparingInputs, std::string_view{"PreparingInputs"}},
  std::pair{JobStatus::Running, std::string_view{"Running"}},
  std::pair{JobStatus::WritingOutputs, std::string_view{"WritingOutputs"}},
  std::pair{JobStatus::ImportingOutputs, std::string_view{"ImportingOutputs"}},
  std::pair{JobStatus::Completed, std::string_view{"Completed"}},
  std::pair{JobStatus::Cancelled, std::string_view{"Cancelled"}},
  std::pair{JobStatus::Failed, std::string_view{"Failed"}}};

constexpr std::array progressEventKindLabels{
  std::pair{ProgressEventKind::Started, std::string_view{"Started"}},
  std::pair{ProgressEventKind::StageStarted, std::string_view{"StageStarted"}},
  std::pair{ProgressEventKind::Progress, std::string_view{"Progress"}},
  std::pair{ProgressEventKind::Artifact, std::string_view{"Artifact"}},
  std::pair{ProgressEventKind::Completed, std::string_view{"Completed"}},
  std::pair{ProgressEventKind::Warning, std::string_view{"Warning"}},
  std::pair{ProgressEventKind::Failed, std::string_view{"Failed"}},
  std::pair{ProgressEventKind::Cancelled, std::string_view{"Cancelled"}}};

} // namespace

std::string_view label(Backend backend)
{
  return enumLabel(backend, backendLabels);
}
std::string_view label(TransformModel model)
{
  return enumLabel(model, transformModelLabels);
}
std::string_view label(Metric metric)
{
  return enumLabel(metric, metricLabels);
}
std::string_view label(Feature feature)
{
  return enumLabel(feature, featureLabels);
}
std::string_view label(ParameterKind kind)
{
  return enumLabel(kind, parameterKindLabels);
}
std::string_view label(DataSource source)
{
  return enumLabel(source, dataSourceLabels);
}
std::string_view label(Interpolation interpolation)
{
  return enumLabel(interpolation, interpolationLabels);
}
std::string_view label(JobStatus status)
{
  return enumLabel(status, jobStatusLabels);
}
std::string_view label(ProgressEventKind kind)
{
  return enumLabel(kind, progressEventKindLabels);
}

Backend backendFromString(std::string_view value)
{
  return enumFromString(value, backendLabels);
}
TransformModel transformModelFromString(std::string_view value)
{
  return enumFromString(value, transformModelLabels);
}
Metric metricFromString(std::string_view value)
{
  return enumFromString(value, metricLabels);
}
Feature featureFromString(std::string_view value)
{
  return enumFromString(value, featureLabels);
}
ParameterKind parameterKindFromString(std::string_view value)
{
  return enumFromString(value, parameterKindLabels);
}
DataSource dataSourceFromString(std::string_view value)
{
  return enumFromString(value, dataSourceLabels);
}
Interpolation interpolationFromString(std::string_view value)
{
  return enumFromString(value, interpolationLabels);
}
JobStatus jobStatusFromString(std::string_view value)
{
  return enumFromString(value, jobStatusLabels);
}
ProgressEventKind progressEventKindFromString(std::string_view value)
{
  return enumFromString(value, progressEventKindLabels);
}

} // namespace registration
