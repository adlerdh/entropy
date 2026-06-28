#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace registration
{

/**
 * @brief External registration engine selected for a job.
 */
enum class Backend : std::uint8_t
{
  Greedy,
  ANTs,
  FireANTs
};

/**
 * @brief High-level transform model requested by the user.
 */
enum class TransformModel : std::uint8_t
{
  Rigid,
  Affine,
  RigidAffine,
  Deformable,
  AffineDeformable,
  Translation,
  Similarity,
  BSplineDisplacement,
  GaussianDisplacement,
  TimeVaryingVelocity
};

/**
 * @brief Similarity metric or loss used by a registration stage.
 */
enum class Metric : std::uint8_t
{
  SSD,
  MSE,
  MI,
  NMI,
  NCC,
  WNCC,
  CC,
  Mattes,
  Demons,
  GC,
  FusedCC,
  FusedMI,
  MaskedCC,
  MaskedMSE,
  LabelGuided,
  PointSet
};

/**
 * @brief Registration object category used for capability checks.
 */
enum class Feature : std::uint8_t
{
  FixedMovingImages,
  FixedMask,
  MovingMask,
  AuxiliaryImagePairs,
  SegmentationLabelMetric,
  LandmarkTransform,
  LandmarkDrivenRegistration,
  SurfaceTransform,
  SurfaceDrivenRegistration,
  InverseWarpOutput,
  ForwardWarpOutput,
  WarpedSegmentationOutput,
  StructuredProgress
};

/**
 * @brief Parameter value category used to drive reactive UI widgets.
 */
enum class ParameterKind : std::uint8_t
{
  Boolean,
  Integer,
  Float,
  Text,
  Choice,
  IntegerVector,
  FloatVector,
  FilePath,
  DirectoryPath
};

/**
 * @brief Source of a registration mask or label image selected in Entropy.
 */
enum class DataSource : std::uint8_t
{
  None,
  LoadedImage,
  Segmentation,
  LandmarkGroup,
  AnnotationSet,
  Surface,
  ExternalFile
};

/**
 * @brief Interpolation requested for warped image-like outputs.
 */
enum class Interpolation : std::uint8_t
{
  Linear,
  NearestNeighbor,
  Label,
  Cubic
};

/**
 * @brief Normalized registration job status for UI display.
 */
enum class JobStatus : std::uint8_t
{
  Queued,
  PreparingInputs,
  Running,
  WritingOutputs,
  ImportingOutputs,
  Completed,
  Cancelled,
  Failed
};

/**
 * @brief Structured event emitted while a registration job runs.
 */
enum class ProgressEventKind : std::uint8_t
{
  Started,
  StageStarted,
  Progress,
  Artifact,
  Completed,
  Warning,
  Failed,
  Cancelled
};

/**
 * @brief Reference to an Entropy object and its file-backed representation.
 */
struct DataRef
{
  std::string uid;                      //!< Entropy object UID, when available.
  std::filesystem::path fileName;       //!< File path supplied to the backend.
  std::string displayName;              //!< User-facing name.
  DataSource source = DataSource::None; //!< Entropy object category.
};

/**
 * @brief Fixed/moving auxiliary image pair used as an additional registration constraint.
 */
struct AuxiliaryImagePair
{
  DataRef fixed;               //!< Fixed-space auxiliary image.
  DataRef moving;              //!< Moving-space auxiliary image.
  double weight = 1.0;         //!< Backend metric weight when supported.
  Metric metric = Metric::NCC; //!< Metric used for this pair when supported.
};

/**
 * @brief Landmark matching policy for paired landmark inputs.
 */
struct LandmarkOptions
{
  enum class PairingMode : std::uint8_t
  {
    ByName,
    ByOrder
  };

  bool enabled = false;                          //!< True when landmarks are part of the job.
  DataRef fixedLandmarks;                        //!< Fixed/reference landmark set.
  DataRef movingLandmarks;                       //!< Moving landmark set.
  PairingMode pairingMode = PairingMode::ByName; //!< Matching rule.
  std::size_t matchedPairs = 0;                  //!< Count after validation/export.
};

/**
 * @brief Output artifacts requested from a registration backend.
 */
struct OutputRequests
{
  bool loadWarpedImage = true;                  //!< Import the warped moving image.
  bool loadInverseWarp = true;                  //!< Import the fixed-to-moving sampling warp.
  bool loadForwardWarp = true;                  //!< Import the moving-to-fixed point warp when produced.
  bool applyWarpToMovingImage = true;           //!< Assign imported warps to the moving image.
  bool loadWarpedSegmentation = false;          //!< Import warped moving segmentation output.
  bool transformLandmarksAndAnnotations = true; //!< Transform point annotations when a forward warp exists.
  bool transformSurfaces = false;               //!< Import transformed surface outputs.
  bool makeWarpedImageActive = false;           //!< Select the warped image after import.
};

/**
 * @brief Single backend parameter exposed by the registration UI.
 */
struct ParameterSchema
{
  std::string key;                          //!< Stable parameter key.
  std::string label;                        //!< Visible widget label.
  std::string tooltip;                      //!< Detailed explanation shown on hover.
  ParameterKind kind = ParameterKind::Text; //!< Widget/value category.
  std::string defaultValue;                 //!< Backend-neutral textual default.
  std::optional<double> minValue;           //!< Lower numeric bound, if applicable.
  std::optional<double> maxValue;           //!< Upper numeric bound, if applicable.
  std::vector<std::string> choices;         //!< Choice labels for combo/radio widgets.
  bool advanced = false;                    //!< Hide behind Advanced by default.
  bool expert = false;                      //!< Hide behind Expert mode by default.
};

/**
 * @brief Capabilities and parameter schema advertised by a backend.
 */
struct BackendCapabilities
{
  Backend backend = Backend::Greedy;           //!< Backend described by this schema.
  std::vector<TransformModel> transformModels; //!< Supported transform models.
  std::vector<Metric> metrics;                 //!< Supported metrics/losses.
  std::vector<Feature> features;               //!< Supported object/output categories.
  std::vector<Interpolation> interpolations;   //!< Supported output interpolation modes.
  std::vector<ParameterSchema> parameters;     //!< Parameters shown by the reactive UI.
};

/**
 * @brief Backend-neutral registration request.
 */
struct JobSpec
{
  int version = 1;                                                  //!< JSON schema version.
  Backend backend = Backend::Greedy;                                //!< Selected backend.
  int dimension = 3;                                                //!< Spatial dimension passed to external backends.
  DataRef fixedImage;                                               //!< Fixed/reference image.
  DataRef movingImage;                                              //!< Moving image.
  TransformModel transformModel = TransformModel::AffineDeformable; //!< Requested transform model.
  Metric metric = Metric::WNCC;                                     //!< Primary metric/loss.
  std::string iterationSchedule = "100x50x10";                      //!< Multi-resolution iteration schedule.
  bool useImageCentersForInitialization = true;                     //!< Initialize from image centers when supported.
  DataRef fixedMask;                                                //!< Optional fixed mask.
  DataRef movingMask;                                               //!< Optional moving mask.
  std::vector<AuxiliaryImagePair> auxiliaryImagePairs;              //!< Additional image/label constraints.
  LandmarkOptions landmarks;                                        //!< Landmark inputs and matching.
  std::vector<DataRef> surfaces;                                    //!< Surfaces requested as transformed outputs.
  OutputRequests outputs;                                           //!< Requested imports and generated outputs.
  std::filesystem::path outputDirectory;                            //!< Job working/output directory.
  std::string outputPrefix;                                         //!< File prefix for generated artifacts.
  std::vector<std::string> extraArguments;                          //!< Expert raw backend arguments.
};

/**
 * @brief Structured progress event consumed by Entropy's registration jobs UI.
 */
struct ProgressEvent
{
  ProgressEventKind kind = ProgressEventKind::Progress; //!< Event category.
  std::string jobId;                                    //!< Backend-neutral job identifier.
  std::string stageName;                                //!< Current registration stage.
  std::optional<int> stageIndex;                        //!< Zero-based stage index.
  std::optional<int> levelIndex;                        //!< Zero-based pyramid level index.
  std::optional<int> iteration;                         //!< Current iteration.
  std::optional<int> iterations;                        //!< Total iterations in this stage/level.
  std::optional<double> progress;                       //!< Normalized progress in [0, 1].
  std::optional<double> loss;                           //!< Latest metric/loss value.
  std::string message;                                  //!< Human-readable status/warning/error text.
  std::filesystem::path artifactPath;                   //!< Generated artifact path for artifact events.
  std::string artifactKind;                             //!< Artifact role, such as warped_image or inverse_warp.
};

/**
 * @brief Result manifest written after backend completion and imported by Entropy.
 */
struct ResultManifest
{
  int version = 1;                                         //!< JSON schema version.
  bool success = false;                                    //!< True when the backend completed successfully.
  Backend backend = Backend::Greedy;                       //!< Backend that produced the result.
  std::string fixedImageUid;                               //!< Fixed/reference image UID.
  std::string movingImageUid;                              //!< Moving image UID.
  std::filesystem::path warpedImage;                       //!< Warped moving image output.
  std::filesystem::path inverseWarp;                       //!< Fixed-to-moving sampling warp.
  std::filesystem::path forwardWarp;                       //!< Moving-to-fixed point warp.
  std::filesystem::path affineTransform;                   //!< Affine/composite transform path.
  std::vector<std::filesystem::path> warpedSegmentations;  //!< Warped segmentation outputs.
  std::vector<std::filesystem::path> transformedSurfaces;  //!< Transformed surface outputs.
  std::vector<std::filesystem::path> transformedLandmarks; //!< Transformed landmark outputs.
  std::vector<std::string> warnings;                       //!< Non-fatal warnings emitted by backend/import.
  std::string errorMessage;                                //!< Failure summary.
  std::string warpConvention;                              //!< Explicit warp direction/coordinate convention.
  double elapsedSeconds = 0.0;                             //!< Runtime reported by Entropy/backend.
};

std::string_view label(Backend backend);
std::string_view label(TransformModel model);
std::string_view label(Metric metric);
std::string_view label(Feature feature);
std::string_view label(ParameterKind kind);
std::string_view label(DataSource source);
std::string_view label(Interpolation interpolation);
std::string_view label(JobStatus status);
std::string_view label(ProgressEventKind kind);

Backend backendFromString(std::string_view value);
TransformModel transformModelFromString(std::string_view value);
Metric metricFromString(std::string_view value);
Feature featureFromString(std::string_view value);
ParameterKind parameterKindFromString(std::string_view value);
DataSource dataSourceFromString(std::string_view value);
Interpolation interpolationFromString(std::string_view value);
JobStatus jobStatusFromString(std::string_view value);
ProgressEventKind progressEventKindFromString(std::string_view value);

} // namespace registration
