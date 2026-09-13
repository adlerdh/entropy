#include "rendering/mesh/MeshGpuStore.h"

#include "rendering/mesh/MeshUpload.h"

#include <utility>

namespace rendering::mesh
{

bool MeshGpuStore::uploadOrReplace(
  const MeshData& mesh,
  const MeshHandle& handle,
  const BufferUsagePattern usagePattern)
{
  std::optional<MeshGpuData> upload = uploadMeshData(mesh, handle, usagePattern);
  if (!upload) {
    return false;
  }

  m_uploads.insert_or_assign(handle.uid, std::move(*upload));
  return true;
}

const MeshGpuData* MeshGpuStore::lookup(const MeshHandle& handle) const noexcept
{
  const auto it = m_uploads.find(handle.uid);
  if (it == m_uploads.end() || it->second.handle() != handle) {
    return nullptr;
  }

  return &it->second;
}

bool MeshGpuStore::remove(const uuids::uuid& uid) noexcept
{
  return m_uploads.erase(uid) > 0;
}

void MeshGpuStore::clear() noexcept
{
  m_uploads.clear();
}

std::size_t MeshGpuStore::size() const noexcept
{
  return m_uploads.size();
}

} // namespace rendering::mesh
