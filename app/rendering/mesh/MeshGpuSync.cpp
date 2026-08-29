#include "rendering/mesh/MeshGpuSync.h"

namespace rendering::mesh
{

MeshGpuSyncStatus syncReadyMeshToGpu(
  const MeshGeometryKey& key,
  const MeshHandle& handle,
  const MeshCache& cache,
  MeshGpuStore& gpuStore,
  const BufferUsagePattern usagePattern)
{
  if (gpuStore.lookup(handle)) {
    return MeshGpuSyncStatus::AlreadyCurrent;
  }

  const MeshData* mesh = cache.readyMesh(key);
  if (!mesh) {
    return MeshGpuSyncStatus::NotReady;
  }

  return gpuStore.uploadOrReplace(*mesh, handle, usagePattern) ? MeshGpuSyncStatus::Uploaded
                                                               : MeshGpuSyncStatus::UploadFailed;
}

} // namespace rendering::mesh
