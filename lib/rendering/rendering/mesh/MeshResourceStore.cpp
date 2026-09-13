#include "rendering/mesh/MeshResourceStore.h"

#include "common/UuidUtility.h"

#include <utility>

namespace rendering::mesh
{

MeshResourceStore::MeshResourceStore(MeshHandleFactory handleFactory)
  : m_handleFactory{handleFactory ? std::move(handleFactory) : MeshHandleFactory{generateRandomUuid}}
{
}

MeshHandle MeshResourceStore::handleFor(const MeshGeometryKey& key)
{
  if (const auto existing = m_handles.find(key); existing != m_handles.end()) {
    return existing->second;
  }

  MeshHandle handle{.uid = m_handleFactory(), .geometryVersion = MeshGeometryKeyHash{}(key)};
  m_handles.emplace(key, handle);
  return handle;
}

const MeshHandle* MeshResourceStore::findHandle(const MeshGeometryKey& key) const noexcept
{
  const auto it = m_handles.find(key);
  return it == m_handles.end() ? nullptr : &it->second;
}

const MeshGeometryKey* MeshResourceStore::findKey(const MeshHandle& handle) const noexcept
{
  for (const auto& [key, candidate] : m_handles) {
    if (candidate == handle) {
      return &key;
    }
  }
  return nullptr;
}

MeshGpuSyncStatus
MeshResourceStore::synchronize(const MeshHandle& handle, const MeshData& mesh, const BufferUsagePattern usagePattern)
{
  if (m_gpuStore.lookup(handle)) {
    return MeshGpuSyncStatus::AlreadyCurrent;
  }
  return m_gpuStore.uploadOrReplace(mesh, handle, usagePattern) ? MeshGpuSyncStatus::Uploaded
                                                                : MeshGpuSyncStatus::UploadFailed;
}

const MeshGpuData* MeshResourceStore::lookup(const MeshHandle& handle) const noexcept
{
  return m_gpuStore.lookup(handle);
}

bool MeshResourceStore::uploadOrReplace(
  const MeshData& mesh,
  const MeshHandle& handle,
  const BufferUsagePattern usagePattern)
{
  return m_gpuStore.uploadOrReplace(mesh, handle, usagePattern);
}

std::size_t MeshResourceStore::retainOnly(const MeshGeometryKeySet& liveKeys)
{
  std::size_t removed = 0;
  for (auto it = m_handles.begin(); it != m_handles.end();) {
    if (liveKeys.contains(it->first)) {
      ++it;
      continue;
    }
    m_gpuStore.remove(it->second.uid);
    it = m_handles.erase(it);
    ++removed;
  }
  return removed;
}

MeshGpuStore& MeshResourceStore::gpuStore() noexcept
{
  return m_gpuStore;
}

const MeshGpuStore& MeshResourceStore::gpuStore() const noexcept
{
  return m_gpuStore;
}

} // namespace rendering::mesh
