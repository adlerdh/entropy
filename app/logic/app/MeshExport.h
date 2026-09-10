#pragma once

#include <uuid.h>

#include <cstddef>
#include <cstdint>

class AppData;

namespace mesh_export
{

/// Export one generated isosurface after prompting for coordinate space and destination.
void exportIsosurface(AppData& appData, const uuids::uuid& imageUid, uint32_t component, const uuids::uuid& surfaceUid);

/// Export one generated segmentation-label surface.
void exportSegmentationLabel(
  AppData& appData,
  const uuids::uuid& imageUid,
  const uuids::uuid& segmentationUid,
  std::size_t labelIndex);

/// Export every non-empty segmentation label to a separate suffixed file.
void exportAllSegmentationLabels(AppData& appData, const uuids::uuid& imageUid, const uuids::uuid& segmentationUid);

} // namespace mesh_export
