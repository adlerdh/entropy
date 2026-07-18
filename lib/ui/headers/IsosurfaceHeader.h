#pragma once

#include "common/AsyncTasks.h"

#include <uuid.h>

#include <functional>
#include <future>

class AppData;

/**
 * @brief Render the isosurface controls for one image in the Isosurfaces window.
 *
 * @param appData Application data containing image, rendering, and isosurface state.
 * @param imageUid UID of the image whose isosurfaces are displayed.
 * @param imageIndex Zero-based image index used for the collapsing header label.
 * @param isActiveImage True when this image is the active image.
 * @param hasFollowingHeader True when another image header follows this one in the panel.
 * @param storeFuture Callback that stores asynchronous mesh-generation task futures.
 * @param addTaskToIsosurfaceGpuMeshGenerationQueue Callback that queues completed CPU meshes for GPU upload.
 */
void renderIsosurfacesHeader(
  AppData& appData,
  const uuids::uuid& imageUid,
  size_t imageIndex,
  bool isActiveImage,
  bool hasFollowingHeader,
  const std::function<void(const uuids::uuid& taskUid, std::future<AsyncTaskDetails> future)>& storeFuture,
  const std::function<void(const uuids::uuid& taskUid)>& addTaskToIsosurfaceGpuMeshGenerationQueue);
