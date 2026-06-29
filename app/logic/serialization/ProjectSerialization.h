#pragma once

#include "common/InputParams.h"
#include "common/Types.h"
#include "layout/LayoutSpec.h"
#include <filesystem>
#include "logic/annotation/Annotation.h"
#include "logic/annotation/PointRecord.h"

#include <nlohmann/json.hpp>

#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace serialize
{

/**
 * @brief Serialized edge-detection space for image edge rendering.
 */
enum class ProjectEdgeDetectionMethod : std::uint8_t
{
  Pixel,
  Voxel
};

/**
 * @brief Serialized multi-component image rendering mode.
 */
enum class ProjectComponentRenderMode : std::uint8_t
{
  SingleComponent,
  Color,
  Minimum,
  Mean,
  Maximum,
  Magnitude,
  ComplexPhase,
  ComplexReal,
  ComplexImaginary,
  VectorDirectionColor,
  VectorSignedNormalProjection,
  VectorPlanarProjectionColor,
  VectorJacobianDeterminant,
  VectorGradientMagnitude,
  VectorDivergence,
  VectorCurlMagnitude,
  VectorLaplacianMagnitude
};

/// @brief Serialized complex phase display units.
enum class ProjectComplexPhaseUnit : std::uint8_t
{
  Radians,
  Degrees
};

/// @brief Serialized complex phase display range convention.
enum class ProjectComplexPhaseRange : std::uint8_t
{
  Signed,
  Unsigned
};

/// @brief Serialized vector arrow spacing units.
enum class ProjectVectorArrowOverlaySpacingMode : std::uint8_t
{
  Pixels,
  Voxels,
  Millimeters
};

/// @brief Serialized warped-grid convention for vector fields.
enum class ProjectVectorWarpedGridConvention : std::uint8_t
{
  SamplingField,
  ApparentDeformation
};

/// @brief Serialized local NCC display convention.
enum class ProjectLocalNccPresentation : std::uint8_t
{
  Dissimilarity,
  Correlation
};

/// @brief Serialized display style for invalid local metric patches.
enum class ProjectLocalMetricInvalidStyle : std::uint8_t
{
  Transparent,
  Gray
};

/// @brief Serialized segmentation masking mode for volume raycasting.
enum class ProjectSegmentationRaycastMasking : std::uint8_t
{
  Disabled,
  MaskIn,
  MaskOut
};

/**
 * @brief Serialized colormap/window parameters for comparison metrics.
 */
struct ProjectMetricSettings
{
  std::size_t m_colorMapIndex = 0;        //!< Color map index.
  glm::vec2 m_slopeIntercept{1.0f, 0.0f}; //!< Metric window slope/intercept.
  bool m_invertColormap = false;          //!< Invert the metric color map.
  bool m_continuousColormap = true;       //!< Use continuous colormap interpolation.
  int m_colormapLevels = 8;               //!< Number of discrete color levels.
};

/**
 * @brief Serialized difference metric settings.
 */
struct ProjectDifferenceMetricSettings
{
  bool m_squared = true;          //!< Use squared difference instead of absolute difference.
  ProjectMetricSettings m_metric; //!< Difference metric colormap/window settings.
};

/**
 * @brief Serialized local normalized cross-correlation metric settings.
 */
struct ProjectLocalNccMetricSettings
{
  ProjectMetricSettings m_metric; //!< Local NCC colormap/window settings.
  ProjectLocalNccPresentation m_presentation =
    ProjectLocalNccPresentation::Dissimilarity; //!< Correlation or dissimilarity presentation.
  bool m_negativeCorrelationAsMismatch = true;  //!< Treat negative correlation as mismatch in dissimilarity mode.
  int m_patchRadius = 3;                        //!< Patch radius in samples.
  float m_sampleSpacing = 1.0f;                 //!< Patch sample spacing in voxel steps.
  float m_minimumValidFraction = 0.75f;         //!< Required valid sample fraction.
  float m_varianceEpsilon = 1.0e-5f;            //!< Minimum variance before a patch is invalid.
  ProjectLocalMetricInvalidStyle m_invalidStyle =
    ProjectLocalMetricInvalidStyle::Transparent; //!< Invalid patch display.
};

/**
 * @brief Serialized local linear residual metric settings.
 */
struct ProjectLocalLinearResidualMetricSettings
{
  ProjectMetricSettings m_metric;       //!< Local residual colormap/window settings.
  int m_patchRadius = 3;                //!< Patch radius in samples.
  float m_sampleSpacing = 1.0f;         //!< Patch sample spacing in voxel steps.
  float m_minimumValidFraction = 0.75f; //!< Required valid sample fraction.
  float m_varianceEpsilon = 1.0e-5f;    //!< Minimum variance before a patch is invalid.
  ProjectLocalMetricInvalidStyle m_invalidStyle =
    ProjectLocalMetricInvalidStyle::Transparent; //!< Invalid patch display.
};

/**
 * @brief Project-wide comparison mode and metric settings.
 */
struct ProjectComparisonSettings
{
  ProjectDifferenceMetricSettings m_difference;                   //!< Difference metric settings.
  ProjectLocalNccMetricSettings m_localNcc;                       //!< Local NCC settings.
  ProjectLocalLinearResidualMetricSettings m_localLinearResidual; //!< Local linear residual settings.
  bool m_overlayMagentaCyan = true;                               //!< Overlay color convention.
  glm::ivec2 m_quadrants{true, true};                             //!< Quadrant comparison axes.
  int m_checkerboardSquares = 10;                                 //!< Checkerboard square count.
  float m_flashlightRadiusFraction = 0.15f;                       //!< Flashlight radius as view fraction.
  bool m_flashlightOverlayMovingImage = true;                     //!< Overlay or replace in flashlight mode.
};

/**
 * @brief Project-wide volume raycasting settings.
 */
struct ProjectRaycastingSettings
{
  float m_samplingFactor = 0.5f;                //!< Ray-marching sampling factor.
  bool m_transparentBackgroundWhenNoHit = true; //!< Make missed 3D rays transparent.
  bool m_renderFrontFaces = true;               //!< Render front faces in 3D raycasting.
  bool m_renderBackFaces = true;                //!< Render back faces in 3D raycasting.
  ProjectSegmentationRaycastMasking m_segmentationMasking =
    ProjectSegmentationRaycastMasking::Disabled; //!< Segmentation mask behavior.
};

/**
 * @brief Project-wide intensity projection defaults.
 */
struct ProjectIntensityProjectionSettings
{
  bool m_useMaximumImageExtent = false; //!< Project through the full image extent.
  float m_slabThicknessMm = 10.0f;      //!< Default projection slab thickness.
  float m_xrayEnergyKeV = 80.0f;        //!< X-ray projection photon energy.
  float m_xrayWindow = 1.0f;            //!< X-ray projection contrast window.
  float m_xrayLevel = 0.5f;             //!< X-ray projection contrast level.
};

/**
 * @brief Project-wide segmentation display defaults.
 */
struct ProjectSegmentationDisplaySettings
{
  bool m_modulateOpacityWithImageOpacity = true; //!< Scale segmentation opacity by image opacity.
  SegmentationOutlineStyle m_outlineStyle = SegmentationOutlineStyle::Disabled; //!< Global segmentation outline.
  float m_interiorOpacity = 0.2f;                                               //!< Interior opacity when outlined.
  float m_erosionFactor = 0.5f;                                                 //!< Linear interpolation cutoff.
};

/**
 * @brief Project-wide isosurface rendering settings.
 */
struct ProjectIsosurfaceDisplaySettings
{
  bool m_floatingPointInterpolation = false;      //!< Use floating-point interpolation for isocontours.
  bool m_modulateOpacityWithImageOpacity = false; //!< Scale isocontour opacity by image opacity.
};

/**
 * @brief Project-wide annotation and landmark display settings.
 */
struct ProjectAnnotationDisplaySettings
{
  bool m_annotationsOnTop = false;       //!< Render annotations over all image planes.
  bool m_landmarksOnTop = false;         //!< Render landmarks over all image planes.
  bool m_hideAnnotationVertices = false; //!< Hide annotation polygon vertices.
};

/**
 * @brief Completed registration result saved with a project.
 */
struct RegistrationResult
{
  std::string m_backend;                                     //!< Backend that produced the result.
  std::string m_fixedImageUid;                               //!< Fixed/reference image UID at registration time.
  std::string m_movingImageUid;                              //!< Moving image UID at registration time.
  std::optional<std::filesystem::path> m_manifestFileName;   //!< Result manifest JSON path, when available.
  std::optional<std::filesystem::path> m_warpedImage;        //!< Warped moving image output.
  std::optional<std::filesystem::path> m_inverseWarp;        //!< Fixed-to-moving sampling warp output.
  std::optional<std::filesystem::path> m_forwardWarp;        //!< Moving-to-fixed point warp output.
  std::optional<std::filesystem::path> m_affineTransform;    //!< Affine/composite transform output.
  std::vector<std::filesystem::path> m_warpedSegmentations;  //!< Warped segmentation outputs.
  std::vector<std::filesystem::path> m_transformedSurfaces;  //!< Transformed surface outputs.
  std::vector<std::filesystem::path> m_transformedLandmarks; //!< Transformed landmark outputs.
  std::vector<std::string> m_warnings;                       //!< Non-fatal warnings reported by backend/import.
};

/**
 * @todo Create enum for all image color maps
 * @brief Serialized data for image settings
 */
struct ImageSettings
{
  std::string m_displayName;
  bool m_globalVisibility = true;            //!< Global image visibility.
  double m_globalOpacity = 1.0;              //!< Global opacity multiplier.
  glm::vec3 m_borderColor{1.0f, 0.0f, 1.0f}; //!< Image border RGB color.
  bool m_lockedToReference = true;           //!< Lock image transformations to the reference image.
  bool m_warpEnabled = true;                 //!< Apply assigned warp during image rendering.
  float m_warpStrength = 1.0f;               //!< Warp strength multiplier.
  bool m_allowExaggeratedWarp = false;       //!< Permit warp strength greater than 1.0x.

  double m_level = 0.0f;  //! Window center value in image units
  double m_window = 1.0f; //! Window width in image units

  double m_thresholdLow = 0.0f;  //!< Values below threshold not displayed
  double m_thresholdHigh = 0.0f; //!< Values above threshold not displayed

  double m_opacity = 1.0f; //!< Opacity [0, 1]

  uint32_t m_activeComponent = 0;     //!< Active image component.
  uint32_t m_activeTimePoint = 0;     //!< Active time point for time-series images.
  bool m_timePlaybackLoop = true;     //!< Loop time playback.
  bool m_timePlaybackPlaying = false; //!< Time playback is running.
  double m_timePlaybackSpeed = 1.0;   //!< Time playback speed multiplier.
  ProjectComponentRenderMode m_componentRenderMode =
    ProjectComponentRenderMode::Magnitude;                                       //!< Multi-component render mode.
  ProjectComplexPhaseUnit m_complexPhaseUnit = ProjectComplexPhaseUnit::Radians; //!< Complex phase display units.
  ProjectComplexPhaseRange m_complexPhaseRange =
    ProjectComplexPhaseRange::Signed;                  //!< Complex phase display range convention.
  bool m_vectorArrowOverlayVisible = false;            //!< Show vector-field arrows over slices.
  bool m_vectorArrowOverlayOnImage = true;             //!< Draw vector-field arrows over the rendered image.
  float m_vectorArrowOverlayDensity = 32.0f;           //!< Vector arrow spacing value.
  float m_vectorArrowOverlayVoxelSpacing = 8.0f;       //!< Vector arrow spacing in image voxels.
  float m_vectorArrowOverlayMillimeterSpacing = 10.0f; //!< Vector arrow spacing in subject millimeters.
  ProjectVectorArrowOverlaySpacingMode m_vectorArrowOverlaySpacingMode =
    ProjectVectorArrowOverlaySpacingMode::Voxels;          //!< Vector arrow spacing units.
  glm::vec3 m_vectorArrowOverlayColor{1.0f, 0.86f, 0.31f}; //!< Fixed vector arrow color.
  bool m_vectorArrowOverlayUseDirectionColor = false;      //!< Color arrows by vector direction.
  float m_vectorArrowOverlayLineThickness = 1.4f;          //!< Vector arrow line thickness in pixels.
  float m_vectorArrowOverlayOpacity = 1.0f;                //!< Vector arrow opacity.
  bool m_vectorArrowOverlayScaleByMagnitude = true;        //!< Scale arrow length by vector magnitude.
  float m_vectorArrowOverlayScaleFactor = 1.0f;            //!< Dimensionless vector arrow length multiplier.
  bool m_vectorWarpedGridVisible = false;                  //!< Show warped vector-field grid.
  bool m_vectorWarpedGridOverlayOnImage = true;            //!< Draw warped grid over the rendered image.
  ProjectVectorWarpedGridConvention m_vectorWarpedGridConvention =
    ProjectVectorWarpedGridConvention::SamplingField; //!< Warped grid convention.
  float m_vectorWarpedGridPixelSpacing = 32.0f;       //!< Warped grid spacing in screen pixels.
  float m_vectorWarpedGridVoxelSpacing = 4.0f;        //!< Warped grid spacing in image voxels.
  float m_vectorWarpedGridMillimeterSpacing = 4.0f;   //!< Warped grid spacing in subject millimeters.
  ProjectVectorArrowOverlaySpacingMode m_vectorWarpedGridSpacingMode =
    ProjectVectorArrowOverlaySpacingMode::Voxels;    //!< Warped grid spacing units.
  float m_vectorWarpedGridLineThickness = 1.5f;      //!< Warped grid line thickness in pixels.
  float m_vectorWarpedGridScaleFactor = 1.0f;        //!< Dimensionless warped grid scale multiplier.
  glm::vec4 m_vectorWarpedGridForegroundColor{1.0f}; //!< Warped grid foreground RGBA color.
  glm::vec4 m_vectorWarpedGridBackgroundColor{0.0f}; //!< Warped grid background RGBA color.
  bool m_vectorPlanarProjectionSignedColors = true;  //!< Preserve in-plane vector signs in projection color.
  bool m_vectorLogJacobianDeterminant = false;       //!< Show log deformation Jacobian determinant.
  bool m_ignoreAlpha = false;                        //!< Ignore alpha when rendering four-component images as RGBA.
  InterpolationMode m_colorInterpolationMode = InterpolationMode::Linear; //!< RGB/RGBA interpolation mode.
  std::vector<double> m_componentLevels;                                  //!< Per-component window centers.
  std::vector<double> m_componentWindows;                                 //!< Per-component window widths.
  std::vector<double> m_componentThresholdLows;                           //!< Per-component low thresholds.
  std::vector<double> m_componentThresholdHighs;                          //!< Per-component high thresholds.
  std::vector<bool> m_componentVisibility;                                //!< Per-component visibility, when present.
  std::vector<double> m_componentOpacities;                               //!< Per-component opacity, when present.
  std::vector<std::size_t> m_colorMapIndices;                             //!< Per-component colormap index.
  std::vector<bool> m_colorMapInverted;                                   //!< Per-component colormap inversion.
  std::vector<bool> m_colorMapContinuous;                                 //!< Per-component colormap interpolation.
  std::vector<std::size_t> m_colorMapLevels;                              //!< Per-component discrete colormap levels.
  std::vector<glm::vec3> m_colorMapHsvModifiers;                          //!< Per-component HSV colormap modifiers.
  std::vector<InterpolationMode> m_interpolationModes;                    //!< Per-component scalar interpolation modes.
  std::vector<double> m_foregroundThresholdLows;  //!< Per-component distance-map foreground low thresholds.
  std::vector<double> m_foregroundThresholdHighs; //!< Per-component distance-map foreground high thresholds.

  ProjectEdgeDetectionMethod m_edgeDetectionMethod = ProjectEdgeDetectionMethod::Voxel; //!< Edge sampling space.
  bool m_showEdges = false;                                                             //!< Show edge rendering.
  bool m_thresholdEdges = false;           //!< Show hard thresholded edges.
  bool m_thinPixelEdges = true;            //!< Thin pixel-space edges with non-maximum suppression.
  bool m_overlayEdges = false;             //!< Overlay edges on the image.
  bool m_colormapEdges = false;            //!< Color edges with the image colormap.
  double m_edgeMagnitude = 0.25;           //!< Voxel-space edge scale or threshold.
  double m_pixelEdgeScale = 2.0;           //!< Pixel-space edge magnitude scale.
  double m_pixelEdgeThreshold = 0.2;       //!< Pixel-space edge hard-edge threshold.
  glm::vec3 m_edgeColor{1.0f, 0.0f, 1.0f}; //!< Solid edge RGB color.
  double m_edgeOpacity = 1.0;              //!< Solid edge opacity.

  bool m_useDistanceMapForRaycasting = true;      //!< Use distance maps for image raycasting.
  bool m_isosurfacesVisible = true;               //!< Show image isosurfaces.
  bool m_applyImageColormapToIsosurfaces = false; //!< Color isosurfaces with the image colormap.
  bool m_showIsocontoursIn2D = true;              //!< Show 2D isocontours.
  double m_isocontourLineWidthIn2D = 2.0;         //!< 2D isocontour line width.
  float m_isosurfaceOpacityModulator = 1.0f;      //!< Isosurface opacity multiplier.
};

/**
 * @brief Serialized data for image segmentation settings
 */
struct SegSettings
{
  std::string m_displayName;
  bool m_visibility = true;
  double m_opacity = 1.0f;
  uint32_t m_activeComponent = 0;
  std::vector<bool> m_componentVisibility;
  std::vector<double> m_componentOpacities;
  std::vector<std::size_t> m_labelTableIndices;
  std::vector<InterpolationMode> m_interpolationModes;
};

/**
 * @brief Serialized data for a segmentation image
 */
struct Segmentation
{
  std::filesystem::path m_segFileName; //!< Segmentation image file

  /**
   * Optional segmentation settings
   */
  std::optional<serialize::SegSettings> m_settings = std::nullopt;
};

/**
 * @brief Serialized data for a group of image landmarks
 */
struct LandmarkGroup
{
  std::string m_csvFileName; //!< CSV file holding the landmarks

  /**
   * Flag indicating whether landmarks are defined in image voxel space (true)
   * or in physical/subject space (false)
   */
  bool m_inVoxelSpace = false;
};

/**
 * @brief Serialized DICOM-series source metadata for an image.
 *
 * `m_imageFileName` remains in `serialize::Image` as a fallback path. This structure records
 * the full source series identity so project reloads do not depend on treating one slice as a
 * normal standalone image.
 */
struct DicomSource
{
  std::filesystem::path m_rootPath;           //!< Root folder used to discover the series.
  std::string m_studyInstanceUid;             //!< Study Instance UID used to disambiguate the series.
  std::string m_seriesInstanceUid;            //!< Series Instance UID to reload.
  std::vector<std::filesystem::path> m_files; //!< Slice files in series order.
};

/**
 * @brief Serialized data for an image in Entropy
 */
struct Image
{
  std::filesystem::path m_imageFileName; //!< Image file name

  /**
   * Optional DICOM source. When present, this describes a full series instead of a single image file.
   */
  std::optional<serialize::DicomSource> m_dicomSource = std::nullopt;

  /**
   * Optional 4x4 affine transformation text file name
   */
  std::optional<std::filesystem::path> m_affineTxFileName = std::nullopt;

  /**
   * Optional inverse warp image file name.
   *
   * The inverse warp is used for moving-image sampling in reference/fixed space.
   */
  std::optional<std::filesystem::path> m_inverseWarpFileName = std::nullopt;

  /**
   * Optional forward warp image file name.
   *
   * The forward warp maps moving-image positions back to reference/fixed space.
   */
  std::optional<std::filesystem::path> m_forwardWarpFileName = std::nullopt;

  /**
   * Optional manual transformation matrix from affine-registered subject space to world space
   */
  std::optional<glm::mat4> m_worldDefTx = std::nullopt;

  /**
   * Optional annotations JSON file name
   */
  std::optional<std::filesystem::path> m_annotationsFileName = std::nullopt;

  /**
   * Segmentation image file names (each image can have multiple segmentations)
   */
  std::vector<serialize::Segmentation> m_segmentations;

  /**
   * Landmark groups (each image can have multiple landmark groups)
   */
  std::vector<serialize::LandmarkGroup> m_landmarkGroups;

  /**
   * Optional image settings
   */
  std::optional<serialize::ImageSettings> m_settings = std::nullopt;
};

/**
 * @brief Interface settings saved with a project.
 */
struct ProjectInterfaceSettings
{
  bool m_synchronizeTimeSeries = true; //!< Synchronize displayed time points across time-series images.
};

/**
 * @brief Project-wide view behavior and anatomical convention settings.
 */
struct ProjectViewSettings
{
  AnatomicalLabelType m_anatomicalLabelType = AnatomicalLabelType::Human; //!< Anatomical label convention.
  bool m_lockAnatomicalDirectionsToReferenceImage = false; //!< Lock anatomical axes to the reference image.
  CrosshairsSnapping m_crosshairsSnapping = CrosshairsSnapping::Disabled; //!< Crosshairs snapping behavior.
};

/**
 * @brief Serialized data for an Entropy project
 */
struct EntropyProject
{
  serialize::Image m_referenceImage;
  std::vector<serialize::Image> m_additionalImages;
  std::optional<std::filesystem::path> m_layoutsFileName = std::nullopt;
  std::vector<layout::LayoutSpec> m_layouts;
  std::optional<std::size_t> m_currentLayoutIndex = std::nullopt;
  ProjectInterfaceSettings m_interface;
  ProjectViewSettings m_view;
  ProjectComparisonSettings m_comparison;
  ProjectRaycastingSettings m_raycasting;
  ProjectIntensityProjectionSettings m_intensityProjection;
  ProjectSegmentationDisplaySettings m_segmentationDisplay;
  ProjectIsosurfaceDisplaySettings m_isosurfaces;
  ProjectAnnotationDisplaySettings m_annotationDisplay;
  std::vector<RegistrationResult> m_registrationResults;
};

/**
 * @brief Serialize project interface settings to JSON.
 * @param j Destination JSON object.
 * @param settings Settings to serialize.
 */
void to_json(nlohmann::json& j, const ProjectInterfaceSettings& settings);

/**
 * @brief Deserialize project interface settings from JSON.
 * @param j Source JSON object.
 * @param settings Settings to update.
 */
void from_json(const nlohmann::json& j, ProjectInterfaceSettings& settings);

/**
 * @brief Serialize project view settings to JSON.
 * @param j Destination JSON object.
 * @param settings Settings to serialize.
 */
void to_json(nlohmann::json& j, const ProjectViewSettings& settings);

/**
 * @brief Deserialize project view settings from JSON.
 * @param j Source JSON object.
 * @param settings Settings to update.
 */
void from_json(const nlohmann::json& j, ProjectViewSettings& settings);

/**
 * @brief Serialize project comparison settings to JSON.
 * @param j Destination JSON object.
 * @param settings Settings to serialize.
 */
void to_json(nlohmann::json& j, const ProjectComparisonSettings& settings);

/**
 * @brief Deserialize project comparison settings from JSON.
 * @param j Source JSON object.
 * @param settings Settings to update.
 */
void from_json(const nlohmann::json& j, ProjectComparisonSettings& settings);

/**
 * @brief Serialize project raycasting settings to JSON.
 * @param j Destination JSON object.
 * @param settings Settings to serialize.
 */
void to_json(nlohmann::json& j, const ProjectRaycastingSettings& settings);

/**
 * @brief Deserialize project raycasting settings from JSON.
 * @param j Source JSON object.
 * @param settings Settings to update.
 */
void from_json(const nlohmann::json& j, ProjectRaycastingSettings& settings);

/**
 * @brief Serialize project intensity projection defaults to JSON.
 * @param j Destination JSON object.
 * @param settings Settings to serialize.
 */
void to_json(nlohmann::json& j, const ProjectIntensityProjectionSettings& settings);

/**
 * @brief Deserialize project intensity projection defaults from JSON.
 * @param j Source JSON object.
 * @param settings Settings to update.
 */
void from_json(const nlohmann::json& j, ProjectIntensityProjectionSettings& settings);

/**
 * @brief Serialize project segmentation display settings to JSON.
 * @param j Destination JSON object.
 * @param settings Settings to serialize.
 */
void to_json(nlohmann::json& j, const ProjectSegmentationDisplaySettings& settings);

/**
 * @brief Deserialize project segmentation display settings from JSON.
 * @param j Source JSON object.
 * @param settings Settings to update.
 */
void from_json(const nlohmann::json& j, ProjectSegmentationDisplaySettings& settings);

/**
 * @brief Serialize project isosurface display settings to JSON.
 * @param j Destination JSON object.
 * @param settings Settings to serialize.
 */
void to_json(nlohmann::json& j, const ProjectIsosurfaceDisplaySettings& settings);

/**
 * @brief Deserialize project isosurface display settings from JSON.
 * @param j Source JSON object.
 * @param settings Settings to update.
 */
void from_json(const nlohmann::json& j, ProjectIsosurfaceDisplaySettings& settings);

/**
 * @brief Serialize project annotation display settings to JSON.
 * @param j Destination JSON object.
 * @param settings Settings to serialize.
 */
void to_json(nlohmann::json& j, const ProjectAnnotationDisplaySettings& settings);

/**
 * @brief Deserialize project annotation display settings from JSON.
 * @param j Source JSON object.
 * @param settings Settings to update.
 */
void from_json(const nlohmann::json& j, ProjectAnnotationDisplaySettings& settings);

/**
 * @brief Serialize a completed registration result to JSON.
 * @param j Destination JSON object.
 * @param result Result to serialize.
 */
void to_json(nlohmann::json& j, const RegistrationResult& result);

/**
 * @brief Deserialize a completed registration result from JSON.
 * @param j Source JSON object.
 * @param result Result to update.
 */
void from_json(const nlohmann::json& j, RegistrationResult& result);

/**
 * @brief Serialize an Entropy project to JSON.
 * @param j Destination JSON object.
 * @param project Project to serialize.
 */
void to_json(nlohmann::json& j, const EntropyProject& project);

/**
 * @brief Deserialize an Entropy project from JSON.
 * @param j Source JSON object.
 * @param project Project to update.
 */
void from_json(const nlohmann::json& j, EntropyProject& project);

/**
 * @todo Put these in a separate header file
 */

/**
 * @brief Create a project to be loaded from input parameters.
 * @param[in] params Command-line input parameters
 * @return Project
 */
serialize::EntropyProject createProjectFromInputParams(const InputParams& params);

/**
 * @brief Open a project from a file
 * @param[in/out] project
 * @param[in] fileName
 * @return True iff a valid project file was successfully opened and parsed
 */
bool open(EntropyProject& project, const std::filesystem::path& fileName);

/**
 * Save a project to a file
 */
bool save(const EntropyProject& project, const std::filesystem::path& fileName);

bool openAffineTxFile(glm::dmat4& matrix, const std::filesystem::path& fileName);
bool saveAffineTxFile(const glm::dmat4& matrix, const std::filesystem::path& fileName);

/**
 * @brief openLandmarkGroupCsvFile
 * @param landmarks Map of landmark ID to point record
 * @param csvFileName
 * @return
 */
bool openLandmarkGroupCsvFile(
  std::map<std::size_t, PointRecord<glm::vec3> >& landmarks,
  const std::filesystem::path& csvFileName);

/**
 * @brief saveLandmarkGroupCsvFile
 * @param landmarks Map of landmark ID to point record
 * @param csvFileName
 * @return
 */
bool saveLandmarkGroupCsvFile(
  const std::map<std::size_t, PointRecord<glm::vec3> >& landmarks,
  const std::filesystem::path& csvFileName);

/**
 * @brief Open annotations from a JSON file
 * @param[out] annots Vector of annotations in JSON file
 * @param jsonFileName Name of JSON file containing annotations
 * @return True iff annotations were loaded from JSON file
 */
bool openAnnotationsFromJsonFile(std::vector<Annotation>& annots, const std::filesystem::path& jsonFileName);

/**
 * @brief Append an annotation to a JSON structure
 * @param[in] annot Annotation to append
 * @param[out] j JSON structure holding annotation(s)
 */
void appendAnnotationToJson(const Annotation& annot, nlohmann::json& j);

/**
 * @brief Save a JSON object to disk
 * @param[in] j JSON object
 * @param[in] jsonFileName File name
 * @return True iff the JSON structure was saved to the file on disk
 */
bool saveToJsonFile(const nlohmann::json& j, const std::filesystem::path& jsonFileName);

} // namespace serialize
