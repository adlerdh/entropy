#pragma once

#include <uuid.h>

class AppData;
namespace serialize
{
struct DicomSource;
}

namespace image_export
{
/**
 * @brief Get the serialized DICOM-series source for an image.
 *
 * @param[in] appData Application data containing the project snapshot.
 * @param[in] imageUid Image UID to query.
 *
 * @return Pointer to the DICOM source metadata, or nullptr when the image is not a DICOM series.
 */
const serialize::DicomSource* dicomSourceForImage(const AppData& appData, const uuids::uuid& imageUid);

/**
 * @brief Export a loaded image through the extension-selected image writer.
 *
 * @param[in,out] appData Application data containing the image.
 * @param[in] imageUid Image UID to export.
 *
 * @return True when a file was selected and written successfully.
 */
bool exportImage(AppData& appData, const uuids::uuid& imageUid);

/**
 * @brief Export a loaded segmentation without changing its project source identity.
 * @param[in,out] appData Application data containing the segmentation.
 * @param[in] segmentationUid Segmentation UID to export.
 * @return True when a destination was selected and written successfully.
 */
bool exportSegmentation(AppData& appData, const uuids::uuid& segmentationUid);
} // namespace image_export
