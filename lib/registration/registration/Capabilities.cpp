#include "registration/Capabilities.h"

#include <algorithm>
#include <optional>
#include <string>
#include <utility>
#include <vector>

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
      "Iterations per level",
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
    choiceParameter(
      "verbosity",
      "Verbose output",
      "Default (1)",
      {"Quiet (0)", "Default (1)", "Verbose (2)"},
      "Greedy output verbosity: quiet, default progress, or extra verbose optimizer output.",
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
    TransformModel::Deformable,
    TransformModel::AffineDeformable,
    TransformModel::BSplineDisplacement,
    TransformModel::GaussianDisplacement,
    TransformModel::TimeVaryingVelocity};
  capabilities.metrics = {Metric::MSE, Metric::MI, Metric::Mattes, Metric::CC, Metric::Demons, Metric::GC};
  capabilities.features = {
    Feature::FixedMovingImages,
    Feature::FixedMask,
    Feature::MovingMask,
    Feature::AuxiliaryImagePairs,
    Feature::InverseWarpOutput,
    Feature::ForwardWarpOutput,
    Feature::WarpedSegmentationOutput};
  capabilities.interpolations = {Interpolation::Linear, Interpolation::NearestNeighbor, Interpolation::Cubic};
  capabilities.parameters = {
    parameter(
      "iterations",
      "Iterations per level",
      ParameterKind::IntegerVector,
      "128x64x32",
      "Per-level convergence iteration schedule from coarse to fine."),
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
      "antsGradientStep",
      "Transform gradient step",
      ParameterKind::Float,
      "0.1",
      0.0,
      10.0,
      "Optimizer step size used inside the selected ANTs transform bracket expression.",
      true),
    numericParameter(
      "synUpdateFieldVariance",
      "SyN update field variance",
      ParameterKind::Float,
      "3",
      0.0,
      100.0,
      "Gaussian variance for smoothing SyN update fields. This is the second argument in SyN[...].",
      true),
    numericParameter(
      "synTotalFieldVariance",
      "SyN total field variance",
      ParameterKind::Float,
      "0",
      0.0,
      100.0,
      "Gaussian variance for smoothing the total SyN field. Zero disables total-field smoothing.",
      true),
    numericParameter(
      "bsplineSyNUpdateMeshSize",
      "B-spline SyN update mesh size",
      ParameterKind::Integer,
      "26",
      1.0,
      200.0,
      "Update-field mesh size at the base level for BSplineSyN.",
      true),
    numericParameter(
      "bsplineSyNTotalMeshSize",
      "B-spline SyN total mesh size",
      ParameterKind::Integer,
      "0",
      0.0,
      200.0,
      "Total-field mesh size at the base level for BSplineSyN. Zero disables total-field smoothing.",
      true),
    numericParameter(
      "bsplineSyNSplineOrder",
      "B-spline SyN spline order",
      ParameterKind::Integer,
      "3",
      1.0,
      5.0,
      "Spline order used by BSplineSyN.",
      true),
    numericParameter(
      "gaussianDisplacementUpdateVariance",
      "Gaussian displacement update variance",
      ParameterKind::Float,
      "3",
      0.0,
      100.0,
      "Gaussian variance for smoothing GaussianDisplacementField update fields.",
      true),
    numericParameter(
      "gaussianDisplacementTotalVariance",
      "Gaussian displacement total variance",
      ParameterKind::Float,
      "0",
      0.0,
      100.0,
      "Gaussian variance for smoothing the total GaussianDisplacementField. Zero disables total-field smoothing.",
      true),
    numericParameter(
      "timeVaryingVelocityTimeIndices",
      "Velocity field time indices",
      ParameterKind::Integer,
      "4",
      1.0,
      20.0,
      "Number of time indices used by TimeVaryingVelocityField.",
      true),
    numericParameter(
      "timeVaryingVelocityUpdateVariance",
      "Velocity update field variance",
      ParameterKind::Float,
      "3",
      0.0,
      100.0,
      "Spatial Gaussian variance for smoothing TimeVaryingVelocityField update fields.",
      true),
    numericParameter(
      "timeVaryingVelocityUpdateTimeVariance",
      "Velocity update time variance",
      ParameterKind::Float,
      "0",
      0.0,
      100.0,
      "Temporal Gaussian variance for smoothing TimeVaryingVelocityField update fields.",
      true),
    numericParameter(
      "timeVaryingVelocityTotalVariance",
      "Velocity total field variance",
      ParameterKind::Float,
      "0",
      0.0,
      100.0,
      "Spatial Gaussian variance for smoothing the total TimeVaryingVelocityField.",
      true),
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
      "verbose",
      "Verbose output",
      ParameterKind::Boolean,
      "true",
      "Print detailed ANTs optimizer and stage output to the registration log.",
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
    TransformModel::AffineDeformable,
    TransformModel::RigidAffineDeformable};
  capabilities.metrics = {Metric::MSE, Metric::MI, Metric::CC};
  capabilities.features = {
    Feature::FixedMovingImages,
    Feature::FixedMask,
    Feature::MovingMask,
    Feature::InverseWarpOutput,
    Feature::StructuredProgress};
  capabilities.interpolations = {Interpolation::Linear, Interpolation::NearestNeighbor};
  capabilities.parameters = {
    choiceParameter(
      "device",
      "Device",
      "cuda:0",
      {"cuda:0", "cuda:1", "cpu"},
      "PyTorch device used by FireANTs. CUDA is expected for practical performance."),
    parameter(
      "iterations",
      "Iterations per level",
      ParameterKind::IntegerVector,
      "128x64x32",
      "Per-scale optimization iterations from coarse to fine."),
    parameter("scales", "Scales", ParameterKind::IntegerVector, "4x2x1", "Image pyramid scales from coarse to fine."),
    choiceParameter(
      "initialMovingTransform",
      "Moment initialization",
      "None",
      {"None",
       "Center of mass (1)",
       "Principal axes (2)",
       "Principal axes anti-rotation (3)",
       "Principal axes both (4)"},
      "FireANTs --initial-moving-transform moment initialization. Use None when images are already aligned in physical "
      "space."),
    choiceParameter(
      "momentOrder",
      "Moment order",
      "2",
      {"1", "2"},
      "Order of moments used for FireANTs moment initialization. Scale correction requires second-order moments.",
      true),
    parameter(
      "momentPerformScaling",
      "Moment scale correction",
      ParameterKind::Boolean,
      "false",
      "Enable FireANTs perform_scaling for second-order moments when fixed and moving images differ in scale.",
      true),
    choiceParameter(
      "momentOrientation",
      "Moment orientation",
      "rot",
      {"rot", "antirot", "both"},
      "Orientation handling for second-order FireANTs moment initialization.",
      true),
    numericParameter(
      "momentScale",
      "Moment scale",
      ParameterKind::Float,
      "1",
      1.0,
      16.0,
      "Downsampling factor used while computing FireANTs moments.",
      true),
    choiceParameter(
      "momentLossType",
      "Moment loss",
      "cc",
      {"cc", "mse", "mi"},
      "Loss used by FireANTs to disambiguate moment orientation candidates.",
      true),
    numericParameter(
      "convergenceThreshold",
      "Convergence tolerance",
      ParameterKind::Float,
      "1e-6",
      0.0,
      1.0,
      "FireANTs convergence tolerance used in [iterations,tolerance,window].",
      true),
    numericParameter(
      "convergenceWindow",
      "Convergence window",
      ParameterKind::Integer,
      "10",
      1.0,
      100.0,
      "Number of iterations FireANTs checks for convergence.",
      true),
    parameter(
      "stageTransforms",
      "Stage transforms",
      ParameterKind::Text,
      "",
      "Optional semicolon-separated FireANTs transform expressions per stage, for example "
      "Rigid[0.03];Affine[0.03];SyN[0.2].",
      false,
      true),
    parameter(
      "stageMetrics",
      "Stage metrics",
      ParameterKind::Text,
      "",
      "Optional semicolon-separated FireANTs metrics per stage, for example MI;CC;MSE or full metric expressions.",
      false,
      true),
    parameter(
      "stageConvergence",
      "Stage convergence",
      ParameterKind::Text,
      "",
      "Optional semicolon-separated convergence values per stage, for example [100x50,1e-6,10];[80x20,1e-4,10].",
      false,
      true),
    parameter(
      "stageShrinkFactors",
      "Stage shrink factors",
      ParameterKind::Text,
      "",
      "Optional semicolon-separated shrink factor schedules per stage, for example 8x4x2x1;8x4x2x1;4x2x1.",
      false,
      true),
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
    parameter(
      "rigidScaling",
      "Rigid scaling",
      ParameterKind::Boolean,
      "false",
      "Allow the FireANTs Rigid stage to estimate isotropic scale in addition to rotation and translation.",
      true),
    parameter(
      "centerOfFrameInitialization",
      "Center-of-frame init",
      ParameterKind::Boolean,
      "false",
      "Initialize FireANTs rigid/affine translation from the fixed and moving image physical frame centers.",
      true),
    parameter(
      "aroundCenter",
      "Optimize around center",
      ParameterKind::Boolean,
      "true",
      "Apply FireANTs rigid/affine optimization around the fixed image center.",
      true),
    parameter(
      "blurImages",
      "Blur pyramid images",
      ParameterKind::Boolean,
      "true",
      "Apply FireANTs Gaussian blur during rigid/affine image pyramid downsampling.",
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
      "ccKernelType",
      "CC kernel type",
      "rectangular",
      {"rectangular", "triangular", "gaussian"},
      "Window type used by FireANTs local cross-correlation loss.",
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
      "deformableTransform",
      "Deformable transform",
      "Greedy",
      {"Greedy", "SyN"},
      "FireANTs deformable registration class used for deformable stages.",
      true),
    choiceParameter(
      "deformableOptimizer",
      "Deformable optimizer",
      "Use optimizer setting",
      {"Use optimizer setting", "Adam", "SGD", "Levenberg-Marquardt"},
      "Optimizer used by FireANTs Greedy/SyN deformable stages. Use optimizer setting keeps the main optimizer "
      "selection for deformable stages. Levenberg-Marquardt is available for compositive deformable warps.",
      true),
    parameter(
      "levenbergLambdaInit",
      "LM initial damping",
      ParameterKind::Text,
      "1e-2",
      "Initial Levenberg-Marquardt damping lambda. Use a positive value or 'auto'.",
      true),
    numericParameter(
      "levenbergLambdaIncrease",
      "LM damping increase",
      ParameterKind::Float,
      "1.5",
      1.0001,
      10.0,
      "Multiplier applied to Levenberg-Marquardt damping when a step increases the loss; must be greater than 1.",
      true),
    numericParameter(
      "levenbergLambdaDecrease",
      "LM damping decrease",
      ParameterKind::Float,
      "0.975",
      0.0,
      0.999,
      "Multiplier applied to Levenberg-Marquardt damping when a step decreases the loss; must be less than 1.",
      true),
    choiceParameter(
      "deformationModel",
      "Deformation model",
      "compositive",
      {"compositive", "geodesic"},
      "Internal FireANTs deformation model used by Greedy and SyN stages.",
      true),
    numericParameter(
      "integratorN",
      "Integrator steps",
      ParameterKind::Integer,
      "10",
      1.0,
      32.0,
      "Number of integration steps used when the deformation model is geodesic.",
      true),
    choiceParameter(
      "lossReduction",
      "Loss reduction",
      "mean",
      {"mean", "sum"},
      "Loss reduction method passed to FireANTs Greedy/SyN deformable registration.",
      true),
    parameter(
      "greedyFreeform",
      "Greedy free-form warp",
      ParameterKind::Boolean,
      "false",
      "Use FireANTs Greedy free-form compositive warp mode instead of diffeomorphic compositive updates.",
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
      "verbose",
      "Verbose output",
      ParameterKind::Boolean,
      "true",
      "Print detailed FireANTs optimizer and progress output to the registration log.",
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
