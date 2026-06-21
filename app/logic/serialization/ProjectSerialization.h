#pragma once

#include "common/InputParams.h"
#include "layout/LayoutSpec.h"
#include <filesystem>
#include "logic/annotation/Annotation.h"
#include "logic/annotation/PointRecord.h"

#include <nlohmann/json.hpp>

#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

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
  Magnitude
};

/**
 * @todo Create enum for all image color maps
 * @brief Serialized data for image settings
 */
struct ImageSettings
{
  std::string m_displayName;

  double m_level = 0.0f;  //! Window center value in image units
  double m_window = 1.0f; //! Window width in image units

  double m_thresholdLow = 0.0f;  //!< Values below threshold not displayed
  double m_thresholdHigh = 0.0f; //!< Values above threshold not displayed

  double m_opacity = 1.0f; //!< Opacity [0, 1]

  uint32_t m_activeComponent = 0; //!< Active image component.
  ProjectComponentRenderMode m_componentRenderMode =
    ProjectComponentRenderMode::SingleComponent; //!< Multi-component render mode.
  bool m_ignoreAlpha = false;                    //!< Ignore alpha when rendering four-component images as RGBA.
  std::vector<bool> m_componentVisibility;       //!< Per-component visibility, when present.
  std::vector<double> m_componentOpacities;      //!< Per-component opacity, when present.

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

  /**
   * @todo Add isosurfaces
   */
};

/**
 * @brief Serialized data for image segmentation settings
 */
struct SegSettings
{
  double m_opacity = 1.0f;

  /**
   * @todo Add rest of the segmentation options, e.g.
   * visibility, color label table, etc.
   */
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
   * Optional deformable transformation image file name
   */
  std::optional<std::filesystem::path> m_deformationFileName = std::nullopt;

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
 * @brief Serialized layout tab bar edge.
 */
enum class ProjectLayoutTabPlacement : std::uint8_t
{
  Top,
  Bottom
};

/**
 * @brief Interface settings saved with a project.
 */
struct ProjectInterfaceSettings
{
  bool m_showLayoutTabs = true;                                                    //!< Show the layout tab bar.
  ProjectLayoutTabPlacement m_layoutTabPlacement = ProjectLayoutTabPlacement::Top; //!< Layout tab bar edge.
  std::uint32_t m_imageValuePrecision = 3; //!< Decimal places for displayed image values.
  std::uint32_t m_coordsPrecision = 3;     //!< Decimal places for displayed coordinates.
  std::uint32_t m_txPrecision = 3;         //!< Decimal places for displayed transform values.
  std::uint32_t m_percentilePrecision = 2; //!< Decimal places for displayed percentiles.
};

/**
 * @brief Serialized data for an Entropy project
 */
struct EntropyProject
{
  serialize::Image m_referenceImage;
  std::vector<serialize::Image> m_additionalImages;
  std::vector<layout::LayoutSpec> m_layouts;
  std::optional<std::size_t> m_currentLayoutIndex = std::nullopt;
  ProjectInterfaceSettings m_interface;
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
