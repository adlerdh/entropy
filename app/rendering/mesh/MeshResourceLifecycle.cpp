#include "rendering/mesh/MeshResourceLifecycle.h"

namespace rendering::mesh
{

std::size_t reconcileExtractedMeshResources(
  const MeshGeometryKeySet& liveKeys,
  MeshCache& cache,
  MeshHandleMap& handles,
  const std::function<void(const uuids::uuid&)>& releaseGpuUpload)
{
  std::size_t removed = 0;
  for (auto it = handles.begin(); it != handles.end();) {
    if (liveKeys.contains(it->first)) {
      ++it;
      continue;
    }

    cache.erase(it->first);
    if (releaseGpuUpload) {
      releaseGpuUpload(it->second.uid);
    }
    it = handles.erase(it);
    ++removed;
  }
  return removed;
}

} // namespace rendering::mesh
