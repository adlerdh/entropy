#ifndef DATA_LOADING_DETAILS_H
#define DATA_LOADING_DETAILS_H

#include "common/UID.h"

#include "imageio/HZeeTypes.hpp"
#include "imageio/ImageCpuRecord.h"
#include "imageio/ParcellationCpuRecord.h"

#include "logic/records/ImageColorMapRecord.h"
#include "logic/records/LabelTableRecord.h"

#include <memory>
#include <optional>
#include <string>

class DataManager;
class MeshCpuRecord;

namespace slideio
{
class SlideCpuRecord;
}

namespace data
{

namespace details
{

std::unique_ptr<imageio::ImageCpuRecord> generateImageCpuRecord(
  const std::string& filename,
  const std::optional<std::string>& dicomSeriesUid,
  const imageio::ComponentNormalizationPolicy& normPolicy
);

std::unique_ptr<ImageColorMap> loadImageColorMapWithQt(const std::string& path);

#if 0
std::unique_ptr<ImageColorMap> loadImageColorMapWithStdLib( const std::string& path );
#endif

std::vector<std::unique_ptr<ImageColorMap> > loadImageColorMapsFromDirectory(const std::string& path
);

/**
 * @brief Create the greyscale image color map record. This is the default map for images.
 * @return Shared pointer to color map record; nullptr on failure
 */
std::shared_ptr<ImageColorMapRecord> createDefaultGreyscaleImageColorMapRecord();

std::shared_ptr<LabelTableRecord> createLabelTableRecord(const size_t numLabels);

std::unique_ptr<MeshCpuRecord> generateIsoSurfaceMeshCpuRecord(
  DataManager& dataManager, const UID& imageUid, const double isoValue
);

std::unique_ptr<MeshCpuRecord> generateLabelMeshCpuRecord(
  DataManager& dataManager, const UID& parcelUid, const uint32_t labelIndex
);

std::optional<UID> generateLabelMeshRecord(
  DataManager& dataManager, const UID& parcelUid, const uint32_t labelIndex
);

std::unique_ptr<imageio::ParcellationCpuRecord> generateDefaultParcellationCpuRecord(
  DataManager& dataManager, const UID& imageUid
);

/**
 * @brief Create a parcellation of 0 (background) values with image size matching a given image.
 * Load the parcellation into DataManager instance and return UID.
 *
 * @param[in] dataManager DataManager instance
 * @param[in] imageUid Image UID to match in size
 *
 * @return UID of blank parcellation; std::nullopt if creation/loading not successful.
 */
std::optional<UID> createBlankParcellation(DataManager& dataManager, const UID& imageUid);

std::unique_ptr<slideio::SlideCpuRecord> generateSlideCpuRecord(const std::string& filename);

} // namespace details

} // namespace data

#endif // DATA_LOADING_DETAILS_H
