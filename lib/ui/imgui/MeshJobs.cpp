#include "ui/ImGuiWrapper.h"

#include "logic/app/Data.h"

#include <spdlog/fmt/std.h>
#include <spdlog/spdlog.h>

#include <chrono>
#include <functional>
#include <utility>

void ImGuiWrapper::storeFuture(const uuids::uuid& taskUid, std::future<AsyncTaskDetails> future)
{
  std::lock_guard<std::mutex> lock(m_futuresMutex);

  if (!future.valid()) {
    spdlog::warn("Future for task {} is not valid", taskUid);
    return;
  }

  m_futures.emplace(taskUid, std::move(future));

  spdlog::debug("Storing future for UI task {}. Total number of UI task futures: {}", taskUid, m_futures.size());
}

void ImGuiWrapper::addTaskToIsosurfaceGpuMeshGenerationQueue(const uuids::uuid& taskUid)
{
  std::lock_guard<std::mutex> lock(m_isosurfaceTaskQueueMutex);

  m_isosurfaceTaskQueueForGpuMeshGeneration.push(taskUid);

  // Post an empty event to notify render thread
  if (m_postEmptyGlfwEvent) {
    m_postEmptyGlfwEvent();
  }
}

void ImGuiWrapper::generateIsosurfaceMeshGpuRecords()
{
  std::lock_guard<std::mutex> lock(m_isosurfaceTaskQueueMutex);

  while (!m_isosurfaceTaskQueueForGpuMeshGeneration.empty()) {
    const uuids::uuid taskUid = m_isosurfaceTaskQueueForGpuMeshGeneration.front();
    m_isosurfaceTaskQueueForGpuMeshGeneration.pop();

    auto it = m_futures.find(taskUid);
    if (std::end(m_futures) == it) {
      spdlog::error("Cannot generate an isosurface GPU mesh because task {} has no stored CPU result", taskUid);
      continue;
    }

    auto& future = it->second;

    // In case the CPU mesh generation task is not done, then wait for it to finish
    // and get the result. (Note: it should be done, since tasks only get on this queue when
    // CPU mesh generation is done.)
    const AsyncTaskDetails value = future.get();

    // Remove the future
    m_futures.erase(it);

    if (
      AsyncTasks::IsosurfaceMeshGeneration != value.task || !value.success || !value.imageUid ||
      !value.imageComponent || !value.objectUid)
    {
      spdlog::error(
        "Isosurface mesh task {} returned an unsuccessful or incomplete CPU result; no GPU mesh will be created",
        taskUid);
      continue;
    }

    spdlog::debug("Task {}: starting GPU mesh generation for isosurface {}", taskUid, *value.objectUid);

    // Get the isosurface associated with this task
    const Isosurface* surface = m_appData.isosurface(*value.imageUid, *value.imageComponent, *value.objectUid);
    if (!surface) {
      spdlog::error("Null isosurface for isosurface {} of image {}", *value.objectUid, *value.imageUid);
      continue;
    }
  }
}
