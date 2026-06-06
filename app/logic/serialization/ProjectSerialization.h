#pragma once

#include "common/InputParams.h"
#include "windowing/LayoutSpec.h"
#include <filesystem>
#include "logic/annotation/Annotation.h"
#include "logic/annotation/PointRecord.h"

#include <nlohmann/json.hpp>

#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

#include <map>
#include <optional>
#include <string>
#include <vector>

namespace serialize
{

/// @todo Create enum for all image color maps
/// @brief Serialized data for image settings
struct ImageSettings
{
  std::string m_displayName;

  double m_level = 0.0f;  //! Window center value in image units
  double m_window = 1.0f; //! Window width in image units

  double m_thresholdLow = 0.0f;  //!< Values below threshold not displayed
  double m_thresholdHigh = 0.0f; //!< Values above threshold not displayed

  double m_opacity = 1.0f; //!< Opacity [0, 1]

  /// @todo Add isosurfaces
};

/// @brief Serialized data for image segmentation settings
struct SegSettings
{
  double m_opacity = 1.0f;

  /// @todo Add rest of the segmentation options, e.g.
  /// visibility, color label table, etc.
};

/// @brief Serialized data for a segmentation image
struct Segmentation
{
  std::filesystem::path m_segFileName; //!< Segmentation image file

  /// Optional segmentation settings
  std::optional<serialize::SegSettings> m_settings = std::nullopt;
};

/// @brief Serialized data for a group of image landmarks
struct LandmarkGroup
{
  std::string m_csvFileName; //!< CSV file holding the landmarks

  /// Flag indicating whether landmarks are defined in image voxel space (true)
  /// or in physical/subject space (false)
  bool m_inVoxelSpace = false;
};

/// @brief Serialized data for an image in Entropy
struct Image
{
  std::filesystem::path m_imageFileName; //!< Image file name

  /// Optional 4x4 affine transformation text file name
  std::optional<std::filesystem::path> m_affineTxFileName = std::nullopt;

  /// Optional deformable transformation image file name
  std::optional<std::filesystem::path> m_deformationFileName = std::nullopt;

  /// Optional manual transformation matrix from affine-registered subject space to world space
  std::optional<glm::mat4> m_worldDefTx = std::nullopt;

  /// Optional annotations JSON file name
  std::optional<std::filesystem::path> m_annotationsFileName = std::nullopt;

  /// Segmentation image file names (each image can have multiple segmentations)
  std::vector<serialize::Segmentation> m_segmentations;

  /// Landmark groups (each image can have multiple landmark groups)
  std::vector<serialize::LandmarkGroup> m_landmarkGroups;

  /// Optional image settings
  std::optional<serialize::ImageSettings> m_settings = std::nullopt;
};

/// @brief Serialized data for an Entropy project
struct EntropyProject
{
  serialize::Image m_referenceImage;
  std::vector<serialize::Image> m_additionalImages;
  std::vector<layout::LayoutSpec> m_layouts;
  std::optional<std::size_t> m_currentLayoutIndex = std::nullopt;
};

/// @todo Put these in a separate header file

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

/// Save a project to a file
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
