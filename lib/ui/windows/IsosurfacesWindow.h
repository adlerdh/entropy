#pragma once

#include "common/AsyncTasks.h"

#include <uuid.h>

#include <functional>
#include <future>

class AppData;

/**
 * @brief Render the isosurface management window.
 * @param appData Application data containing image order, isosurfaces, and window visibility.
 * @param storeFuture Callback that stores a background isosurface task.
 * @param addTaskToIsosurfaceGpuMeshGenerationQueue Callback that queues GPU mesh generation for a task.
 */
void renderIsosurfacesWindow(
  AppData& appData,
  std::function<void(const uuids::uuid& taskUid, std::future<AsyncTaskDetails> future)> storeFuture,
  std::function<void(const uuids::uuid& taskUid)> addTaskToIsosurfaceGpuMeshGenerationQueue);
