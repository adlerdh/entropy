#include "registration/Capabilities.h"

#include <algorithm>

namespace registration
{
namespace
{

ParameterSchema parameter(
  std::string key,
  std::string labelText,
  ParameterKind kind,
  std::string defaultValue,
  std::string tooltip,
  bool advanced = false,
  bool expert = false)
{
  ParameterSchema schema;
  schema.key = std::move(key);
  schema.label = std::move(labelText);
  schema.kind = kind;
  schema.defaultValue = std::move(defaultValue);
  schema.tooltip = std::move(tooltip);
  schema.advanced = advanced;
  schema.expert = expert;
  return schema;
}

ParameterSchema numericParameter(
  std::string key,
  std::string labelText,
  ParameterKind kind,
  std::string defaultValue,
  double minValue,
  double maxValue,
  std::string tooltip,
  bool advanced = false,
  bool expert = false)
{
  ParameterSchema schema = parameter(
    std::move(key),
    std::move(labelText),
    kind,
    std::move(defaultValue),
    std::move(tooltip),
    advanced,
    expert);
  schema.minValue = minValue;
  schema.maxValue = maxValue;
  return schema;
}

ParameterSchema choiceParameter(
  std::string key,
  std::string labelText,
  std::string defaultValue,
  std::vector<std::string> choices,
  std::string tooltip,
  bool advanced = false,
  bool expert = false)
{
  ParameterSchema schema = parameter(
    std::move(key),
    std::move(labelText),
    ParameterKind::Choice,
    std::move(defaultValue),
    std::move(tooltip),
    advanced,
    expert);
  schema.choices = std::move(choices);
  return schema;
}

BackendCapabilities greedyCapabilities()
{
  BackendCapabilities capabilities;
  capabilities.backend = Backend::Greedy;
  capabilities.transformModels = {
    TransformModel::Rigid,
    TransformModel::Similarity,
    TransformModel::Affine,
    TransformModel::Deformable,
    TransformModel::AffineDeformable};
  capabilities.metrics = {Metric::SSD, Metric::MI, Metric::NMI, Metric::NCC, Metric::WNCC};
  capabilities.features = {
    Feature::FixedMovingImages,
    Feature::FixedMask,
    Feature::MovingMask,
    Feature::AuxiliaryImagePairs,
    Feature::SegmentationLabelMetric,
    Feature::LandmarkTransform,
    Feature::SurfaceTransform,
    Feature::InverseWarpOutput,
    Feature::ForwardWarpOutput,
    Feature::WarpedSegmentationOutput};
  capabilities.interpolations = {Interpolation::Linear, Interpolation::NearestNeighbor, Interpolation::Label};
  capabilities.parameters = {
    parameter(
      "iterations",
      "Iterations per level (coarse to fine):",
      ParameterKind::IntegerVector,
      "128x64x32",
      "Multi-resolution iteration schedule from coarse to fine."),
    numericParameter(
      "wnccRadius",
      "NCC/WNCC radius",
      ParameterKind::Integer,
      "2",
      1.0,
      12.0,
      "Patch radius used by NCC and WNCC metrics. Larger values smooth the metric response."),
    choiceParameter(
      "smoothingUnits",
      "Smoothing units",
      "vox",
      {"vox", "mm"},
      "Units for deformable update smoothing."),
    parameter(
      "smoothingSigma",
      "Smoothing sigma",
      ParameterKind::FloatVector,
      "1.732,0.7071",
      "Greedy update smoothing sigmas for deformable transformation regularity.",
      true),
    numericParameter(
      "stepSize",
      "Step size",
      ParameterKind::Float,
      "1.0",
      0.0,
      10.0,
      "Greedy deformable update step size (-e).",
      true),
    choiceParameter(
      "velocityModel",
      "Velocity model",
      "Stationary velocity (sv)",
      {"Stationary velocity (sv)",
       "Accurate stationary velocity (svlb)",
       "Incompressible stationary velocity (sv-incompr)"},
      "Greedy deformable transformation update model: stationary velocity, accurate stationary velocity, or "
      "incompressible.",
      true),
    numericParameter(
      "inverseExponentiation",
      "Exponentiation depth",
      ParameterKind::Integer,
      "4",
      0.0,
      12.0,
      "Greedy exponent used for warp inversion, root computation, and stationary velocity fields.",
      true),
    parameter(
      "fixedMaskTrim",
      "Fixed mask trim",
      ParameterKind::Text,
      "",
      "Optional trim radius for Greedy -gm-trim, for example 8x8x8.",
      true),
    numericParameter(
      "affineJitter",
      "Affine jitter",
      ParameterKind::Float,
      "0.5",
      0.0,
      10.0,
      "Small random jitter applied to affine metric sample points.",
      true),
    numericParameter(
      "affineSearchIterations",
      "Affine search iterations",
      ParameterKind::Integer,
      "0",
      0.0,
      10000.0,
      "Number of random rigid-transform search iterations before affine optimization.",
      true),
    choiceParameter(
      "affineSearchRotation",
      "Affine search rotation",
      "Any rotation (any)",
      {"Any rotation (any)", "Any rotation or flip (flip)"},
      "Greedy -search rotation range: degrees, any, or flip.",
      true),
    parameter(
      "affineSearchTranslation",
      "Affine search translation",
      ParameterKind::Text,
      "50",
      "Greedy -search translation range in physical units.",
      true),
    choiceParameter(
      "affineMoments",
      "Affine moments initialization",
      "Disabled (off)",
      {"Disabled (off)", "First moments (1)", "Second moments (2)"},
      "Initialize affine registration by matching first or second moments instead of image centers/current affine.",
      true),
    choiceParameter(
      "affineDeterminant",
      "Affine determinant",
      "None (free)",
      {"None (free)", "No flip (1)", "Flip (-1)"},
      "Optionally force the affine determinant sign: free, no flip, or flip.",
      true),
    numericParameter(
      "threads",
      "Threads",
      ParameterKind::Integer,
      "0",
      0.0,
      256.0,
      "Maximum CPU threads. Zero lets Greedy choose.",
      true),
    numericParameter(
      "warpPrecision",
      "Warp precision",
      ParameterKind::Float,
      "0.1",
      0.0,
      10.0,
      "Saved warp precision in voxels. Zero disables compression.",
      true),
    parameter(
      "singlePrecision",
      "Single precision",
      ParameterKind::Boolean,
      "false",
      "Use single precision for faster registration with slightly reduced numeric precision.",
      true),
    parameter(
      "extraArgs",
      "Extra arguments",
      ParameterKind::Text,
      "",
      "Raw Greedy command-line arguments appended in Expert mode.",
      false,
      true)};
  return capabilities;
}

BackendCapabilities antsCapabilities()
{
  BackendCapabilities capabilities;
  capabilities.backend = Backend::ANTs;
  capabilities.transformModels = {
    TransformModel::Translation,
    TransformModel::Rigid,
    TransformModel::Similarity,
    TransformModel::Affine,
    TransformModel::RigidAffine,
    TransformModel::Deformable,
    TransformModel::AffineDeformable,
    TransformModel::BSplineDisplacement,
    TransformModel::GaussianDisplacement,
    TransformModel::TimeVaryingVelocity};
  capabilities.metrics = {
    Metric::MSE,
    Metric::MI,
    Metric::Mattes,
    Metric::CC,
    Metric::Demons,
    Metric::GC,
    Metric::LabelGuided,
    Metric::PointSet};
  capabilities.features = {
    Feature::FixedMovingImages,
    Feature::FixedMask,
    Feature::MovingMask,
    Feature::AuxiliaryImagePairs,
    Feature::SegmentationLabelMetric,
    Feature::LandmarkTransform,
    Feature::LandmarkDrivenRegistration,
    Feature::InverseWarpOutput,
    Feature::ForwardWarpOutput,
    Feature::WarpedSegmentationOutput};
  capabilities.interpolations = {Interpolation::Linear, Interpolation::NearestNeighbor, Interpolation::Cubic};
  capabilities.parameters = {
    parameter(
      "iterations",
      "Iterations per level (coarse to fine):",
      ParameterKind::IntegerVector,
      "128x64x32",
      "Per-level convergence iteration schedule."),
    parameter(
      "shrinkFactors",
      "Shrink factors",
      ParameterKind::IntegerVector,
      "4x2x1",
      "Resolution pyramid shrink factors from coarse to fine."),
    parameter(
      "smoothingSigmas",
      "Smoothing sigmas",
      ParameterKind::FloatVector,
      "2x1x0",
      "Per-level image smoothing sigmas."),
    numericParameter(
      "convergenceThreshold",
      "Convergence threshold",
      ParameterKind::Float,
      "1e-6",
      0.0,
      1.0,
      "Stop a stage when metric improvement remains below this threshold.",
      true),
    numericParameter(
      "convergenceWindow",
      "Convergence window",
      ParameterKind::Integer,
      "10",
      1.0,
      100.0,
      "Number of iterations used to test convergence.",
      true),
    choiceParameter(
      "samplingStrategy",
      "Sampling strategy",
      "Regular",
      {"None", "Regular", "Random"},
      "Point sampling strategy for image metrics.",
      true),
    numericParameter(
      "samplingPercentage",
      "Sampling percentage",
      ParameterKind::Float,
      "0.25",
      0.0,
      1.0,
      "Fraction of points sampled for metric evaluation.",
      true),
    parameter(
      "extraArgs",
      "Extra arguments",
      ParameterKind::Text,
      "",
      "Raw antsRegistration command-line arguments appended in Expert mode.",
      false,
      true)};
  return capabilities;
}

BackendCapabilities fireAntsCapabilities()
{
  BackendCapabilities capabilities;
  capabilities.backend = Backend::FireANTs;
  capabilities.transformModels = {
    TransformModel::Rigid,
    TransformModel::Affine,
    TransformModel::RigidAffine,
    TransformModel::Deformable,
    TransformModel::AffineDeformable};
  capabilities.metrics = {Metric::MSE, Metric::MI, Metric::CC, Metric::NCC, Metric::WNCC};
  capabilities.features = {
    Feature::FixedMovingImages,
    Feature::FixedMask,
    Feature::MovingMask,
    Feature::LandmarkTransform,
    Feature::InverseWarpOutput,
    Feature::ForwardWarpOutput,
    Feature::WarpedSegmentationOutput,
    Feature::StructuredProgress};
  capabilities.interpolations = {Interpolation::Linear, Interpolation::NearestNeighbor};
  capabilities.parameters = {
    choiceParameter(
      "device",
      "Device",
      "cuda:0",
      {"cuda:0", "cuda:1", "cpu"},
      "PyTorch device used by FireANTs. CUDA is expected for practical performance."),
    parameter("scales", "Scales", ParameterKind::IntegerVector, "4x2x1", "Image pyramid scales from coarse to fine."),
    parameter(
      "iterations",
      "Iterations per level (coarse to fine):",
      ParameterKind::IntegerVector,
      "128x64x32",
      "Per-scale optimization iterations."),
    choiceParameter("optimizer", "Optimizer", "Adam", {"Adam", "SGD"}, "PyTorch optimizer used by FireANTs.", true),
    numericParameter(
      "learningRate",
      "Learning rate",
      ParameterKind::Float,
      "0.5",
      0.0,
      10.0,
      "Optimizer learning rate.",
      true),
    numericParameter(
      "ccKernelSize",
      "CC kernel size",
      ParameterKind::Integer,
      "5",
      1.0,
      31.0,
      "Local cross-correlation kernel size.",
      true),
    choiceParameter(
      "miKernel",
      "MI kernel",
      "gaussian",
      {"gaussian", "b-spline"},
      "Kernel used by the FireANTs mutual information loss.",
      true),
    numericParameter(
      "miBins",
      "MI bins",
      ParameterKind::Integer,
      "32",
      8.0,
      128.0,
      "Number of histogram bins used by the mutual information loss.",
      true),
    numericParameter(
      "smoothWarpSigma",
      "Smooth warp sigma",
      ParameterKind::Float,
      "0.25",
      0.0,
      10.0,
      "Gaussian smoothing applied to the warp update.",
      true),
    numericParameter(
      "smoothGradSigma",
      "Smooth gradient sigma",
      ParameterKind::Float,
      "0.5",
      0.0,
      10.0,
      "Gaussian smoothing applied to the metric gradient.",
      true),
    choiceParameter(
      "deformationType",
      "Deformable transformation",
      "Greedy",
      {"Greedy", "SyN"},
      "FireANTs deformable transformation model used for deformable stages.",
      true),
    parameter(
      "normalizeImageIntensities",
      "Normalize intensities",
      ParameterKind::Boolean,
      "false",
      "Normalize fixed and moving intensities to [0, 1] before registration.",
      true),
    parameter(
      "winsorizeImageIntensities",
      "Winsorize intensities",
      ParameterKind::FloatVector,
      "",
      "Optional lower,upper quantiles or percentiles used to clamp image intensities before registration.",
      true),
    parameter(
      "extraArgs",
      "Extra arguments",
      ParameterKind::Text,
      "",
      "Raw fireantsRegistration command-line arguments appended in Expert mode.",
      false,
      true)};
  return capabilities;
}

template<typename T>
bool contains(const std::vector<T>& values, T value)
{
  return std::find(values.begin(), values.end(), value) != values.end();
}

} // namespace

BackendCapabilities capabilitiesForBackend(Backend backend)
{
  switch (backend) {
    case Backend::Greedy:
      return greedyCapabilities();
    case Backend::ANTs:
      return antsCapabilities();
    case Backend::FireANTs:
      return fireAntsCapabilities();
  }

  return greedyCapabilities();
}

bool supportsFeature(const BackendCapabilities& capabilities, Feature feature)
{
  return contains(capabilities.features, feature);
}

bool supportsTransformModel(const BackendCapabilities& capabilities, TransformModel model)
{
  return contains(capabilities.transformModels, model);
}

bool supportsMetric(const BackendCapabilities& capabilities, Metric metric)
{
  return contains(capabilities.metrics, metric);
}

} // namespace registration
