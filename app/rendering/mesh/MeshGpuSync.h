#pragma once

#include "rendering/mesh/MeshCache.h"
#include "rendering/mesh/MeshGpuStore.h"
#include "rendering/mesh/MeshHandle.h"
#include "rendering/utility/gl/GLBufferTypes.h"

namespace rendering::mesh
{

/**
 * @brief Result of synchronizing a ready CPU mesh cache entry to GPU buffers
 */
enum class MeshGpuSyncStatus
{
  Uploaded,
  AlreadyCurrent,
  NotReady,
  UploadFailed
};

/**
 * @brief Upload a ready CPU mesh to the GPU store if the exact handle is not already current
 *
 * The caller owns the OpenGL context requirement: this must be called only when the target context is current.
 *
 * @param key Geometry cache key to read
 * @param handle Mesh handle and geometry version that should represent the upload
 * @param cache CPU mesh cache
 * @param gpuStore GPU mesh store
 * @param usagePattern OpenGL buffer usage hint
 * @return Synchronization status
 * @throw Propagates OpenGL wrapper errors and allocation failures
 */
MeshGpuSyncStatus syncReadyMeshToGpu(
  const MeshGeometryKey& key,
  MeshHandle handle,
  const MeshCache& cache,
  MeshGpuStore& gpuStore,
  BufferUsagePattern usagePattern = BufferUsagePattern::StaticDraw);

} // namespace rendering::mesh
