#ifndef DATA_LOADING_H
#define DATA_LOADING_H

#include "common/UID.h"

#include <memory>
#include <optional>
#include <set>
#include <string>
#include <vector>

class DataManager;

namespace data
{

/**
 * @brief Attempt to load an image from disk into the DataManager instance. Return its assigned
 * UID if successful. Make this the active image and assign it to be the the last image in the
 * ordered list of images.
 *
 * @param[in] dataManager DataManager instance
 * @param[in] filename Image file name
 * @param[in] dicomSeriesUid Optional DICOM series UID
 *
 * @return If generation successful, return the image UID. Otherwise, return std::nullopt.
 */
std::optional<UID> loadImage(
  DataManager& dataManager,
  const std::string& filename,
  const std::optional<std::string>& dicomSeriesUid
);

/**
 * @brief Attempt to load a parcellation from disk into the DataManager instance.
 * Return its assigned UID if successful. Make this the active parcellation.
 * Create a label table for the parcellation and associate the table with the parcellation
 * in DataManager instance.
 *
 * @param[in] dataManager DataManager instance
 * @param[in] filename Parcellation image file name
 * @param[in] dicomSeriesUid Optional DICOM series UID
 *
 * @return If generation/loading successful, return the parcellation UID.
 * Otherwise, return std::nullopt.
 */
std::optional<UID> loadParcellation(
  DataManager& dataManager,
  const std::string& filename,
  const std::optional<std::string>& dicomSeriesUid
);

/**
 * @brief Load a slide image from disk and return its assigned UID if successful.
 *
 * @param dataManager DataManager reference
 * @param filename Slide file name
 * @param translateToTopOfStack If true, the slide will be translated along the stack's Z axis
 * such that it is on top of the stack
 *
 * @return If generation successful, return the slide UID.
 * Otherwise, return std::nullopt.
 */
std::optional<UID> loadSlide(
  DataManager& dataManager, const std::string& filename, bool translateToTopOfStack
);

/**
 * @brief Get the UID of the active parcellation. If there is no active parcellation,
 * then create a new blank parcellation with size matching the provided image.
 * A default label table is created for the blank parcellation and associated with it
 * in the DataManager instance. Load the parcellation into DataManager instance and return its UID.
 *
 * @param[in] dataManager DataManager instance
 * @param[in] imageUid UID of the image to match in size
 *
 * @return If generation successful, return the parcellation UID. Otherwise, return std::nullopt.
 */
std::optional<UID> getActiveParcellation(DataManager& dataManager, const UID& imageUid);

/**
 * @brief Generate a surface mesh at a given iso-value in an image.
 * Load the generated mesh into DataManager instance and return the UID of the mesh.
 *
 * @param[in] dataManager DataManager instance
 * @param[in] imageUid UID of the input image
 * @param[in] isoValue Iso-value at which to generate the surface
 *
 * @return If generation successful, return the mesh UID. Otherwise, return std::nullopt.
 */
std::optional<UID> generateIsoSurfaceMesh(
  DataManager& dataManager, const UID& imageUid, double isoValue
);

/**
 * @brief Generate surface meshes from a set of given label indices in a parcellation.
 * Does not re-generate meshes for label indices if they have already been generated.
 * Load the generated meshes into DataManager instance.
 *
 * @param[in] dataManager DataManager instance
 * @param[in] parcelUid UID of the input parcellation
 * @param[in] labelsIndices Set of label indices for which to generate meshes
 *
 * @return Vector of UIDs of generated meshes
 */
std::vector<UID> generateLabelMeshes(
  DataManager& dataManager, const UID& parcelUid, const std::set<uint32_t>& labelsIndices
);

/**
 * @brief Generate surface meshes from all label indices in a parcellation.
 * Load the meshes into DataManager instance.
 *
 * @param[in] dataManager DataManager instance
 * @param[in] parcelUid UID of the input parcellation
 *
 * @return Vector of UIDs of meshes successfully generated and loaded into dataManager
 */
std::vector<UID> generateAllLabelMeshes(DataManager& dataManager, const UID& parcelUid);

/**
 * @brief Load multiple image color maps from a directory on disk into DataManager instance
 *
 * @param[in] dataManager DataManager instance
 * @param[in] path Path to directory containing image color maps
 *
 * @return Vector of UIDs of loaded image color maps
 */
std::vector<UID> loadImageColorMaps(DataManager& dataManager, const std::string& directoryPath);

/**
 * @brief Load a single image color map from disk into DataManager instance
 *
 * @param[in] dataManager DataManager instance
 * @param[in] filePath Path to the color map file
 *
 * @return UID of the loaded color map record; std::nullopt if loading was not successful
 */
std::optional<UID> loadImageColorMap(DataManager& dataManager, const std::string& filePath);

/**
 * @brief Load the default (greyscale) color map into DataManager instance
 *
 * @param[in] dataManager DataManager instance
 *
 * @return UID of the greyscale color map (if it exists)
 */
std::optional<UID> loadDefaultGreyscaleColorMap(DataManager& dataManager);

} // namespace data

#endif // DATA_LOADING_H
