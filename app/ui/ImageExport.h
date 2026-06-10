#pragma once

#include "logic/serialization/ProjectSerialization.h"

#include <uuid.h>

class AppData;

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
 * @brief Test whether an image was loaded from an explicit DICOM-series source.
 *
 * @param[in] appData Application data containing the project snapshot.
 * @param[in] imageUid Image UID to query.
 *
 * @return True when the image has DICOM-series source metadata.
 */
bool imageHasDicomSource(const AppData& appData, const uuids::uuid& imageUid);

/**
 * @brief Export a loaded DICOM-series image through ITK's extension-selected image writer.
 *
 * @param[in,out] appData Application data containing the image.
 * @param[in] imageUid Image UID to export.
 *
 * @return True when a file was selected and written successfully.
 */
bool exportDicomImage(AppData& appData, const uuids::uuid& imageUid);
} // namespace image_export
