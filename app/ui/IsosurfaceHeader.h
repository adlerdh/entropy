#pragma once

#include "common/AsyncTasks.h"

#include <uuid.h>

#include <functional>
#include <future>

class AppData;

void renderIsosurfacesHeader(
  AppData& appData,
  const uuids::uuid& imageUid,
  size_t imageIndex,
  bool isActiveImage,
  std::function<void(const uuids::uuid& taskUid, std::future<AsyncTaskDetails> future)> storeFuture,
  std::function<void(const uuids::uuid& taskUid)> addTaskToIsosurfaceGpuMeshGenerationQueue);
