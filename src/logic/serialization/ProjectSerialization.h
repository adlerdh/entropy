#ifndef PARSE_ARGUMENTS_H
#define PARSE_ARGUMENTS_H

#include "common/InputParams.h"
#include "common/filesystem.h"
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
/// @todo Serialize rest of app data, like custom layouts

/// Serialized data for image settings
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

/// Serialized data for image segmentation settings
struct SegSettings
{
  double m_opacity = 1.0f;

  /// @todo Add rest of the segmentation options, e.g.
  /// visibility, color label table, etc.
};

/// @brief Serialized data for a segmentation image
struct Segmentation
{
  /// Segmentation image file
  fs::path m_segFileName;

  /// Segmentation settings
  serialize::SegSettings m_settings;
};

/// @brief Serialized data for a group of image landmarks
struct LandmarkGroup
{
  /// CSV file holding the landmarks
  std::string m_csvFileName;

  /// Flag indicating whether landmarks are defined in image voxel space (true)
  /// or in physical/subject space (false)
  bool m_inVoxelSpace = false;
};

/// @brief Serialized data for an image in Entropy
struct Image
{
  /// Image file name
  fs::path m_imageFileName;

  /// Optional 4x4 affine transformation text file name
  std::optional<fs::path> m_affineTxFileName = std::nullopt;

  /// Optional deformable transformation image file name
  std::optional<fs::path> m_deformationFileName = std::nullopt;

  /// Optional annotations JSON file name
  std::optional<fs::path> m_annotationsFileName = std::nullopt;

  /// Segmentation image file names (each image can have multiple segmentations)
  std::vector<serialize::Segmentation> m_segmentations;

  /// Landmark groups (each image can have multiple landmark groups)
  std::vector<serialize::LandmarkGroup> m_landmarkGroups;

  /// Image settings
  serialize::ImageSettings m_settings;
};

/// @brief Serialized data for an Entropy project
struct EntropyProject
{
  serialize::Image m_referenceImage;
  std::vector<serialize::Image> m_additionalImages;
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
bool open(EntropyProject& project, const fs::path& fileName);

/// Save a project to a file
bool save(const EntropyProject& project, const fs::path& fileName);

bool openAffineTxFile(glm::dmat4& matrix, const fs::path& fileName);
bool saveAffineTxFile(const glm::dmat4& matrix, const fs::path& fileName);

/**
 * @brief openLandmarkGroupCsvFile
 * @param landmarks Map of landmark ID to point record
 * @param csvFileName
 * @return
 */
bool openLandmarkGroupCsvFile(
  std::map<std::size_t, PointRecord<glm::vec3> >& landmarks, const fs::path& csvFileName
);

/**
 * @brief saveLandmarkGroupCsvFile
 * @param landmarks Map of landmark ID to point record
 * @param csvFileName
 * @return
 */
bool saveLandmarkGroupCsvFile(
  const std::map<std::size_t, PointRecord<glm::vec3> >& landmarks, const fs::path& csvFileName
);

/**
 * @brief Open annotations from a JSON file
 * @param[out] annots Vector of annotations in JSON file
 * @param jsonFileName Name of JSON file containing annotations
 * @return True iff annotations were loaded from JSON file
 */
bool openAnnotationsFromJsonFile(std::vector<Annotation>& annots, const fs::path& jsonFileName);

/**
 * @brief Append an annotation to a JSON structure
 * @param[in] annot Annotation to append
 * @param[out] j JSON structure holding annotation(s)
 */
void appendAnnotationToJson(const Annotation& annot, nlohmann::json& j);

/**
 * @brief Save a JSON structure to disk
 * @param[in] j JSON structure
 * @param[in] jsonFileName File name
 * @return True iff the JSON structure was saved to the file on disk
 */
bool saveToJsonFile(const nlohmann::json& j, const fs::path& jsonFileName);

} // namespace serialize

#endif // PARSE_ARGUMENTS_H
