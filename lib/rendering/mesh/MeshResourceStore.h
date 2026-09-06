#pragma once

#include "rendering/mesh/MeshGpuStore.h"
#include "rendering/mesh/MeshHandle.h"
#include "rendering/mesh/MeshKeys.h"
#include "rendering/utility/gl/GLBufferTypes.h"

#include <cstddef>
#include <functional>
#include <unordered_map>
#include <uuid.h>

namespace rendering::mesh
{

using MeshHandleFactory = std::function<uuids::uuid()>;
using MeshHandleMap = std::unordered_map<MeshGeometryKey, MeshHandle, MeshGeometryKeyHash>;

enum class MeshGpuSyncStatus
{
  Uploaded,
  AlreadyCurrent,
  UploadFailed
};

/**
 * @brief Context-bound owner of logical mesh handles and uploaded GPU buffers
 *
 * CPU extraction and application scene policy stay outside this class. Every mutating GPU operation requires the
 * associated OpenGL context to be current.
 */
class MeshResourceStore
{
public:
  explicit MeshResourceStore(MeshHandleFactory handleFactory = {});

  /** Return the stable logical handle for a geometry key, creating it on first use. */
  MeshHandle handleFor(const MeshGeometryKey& key);

  /** Find a previously created handle without creating one. */
  const MeshHandle* findHandle(const MeshGeometryKey& key) const noexcept;

  /** Find the geometry key represented by a logical handle. */
  const MeshGeometryKey* findKey(const MeshHandle& handle) const noexcept;

  /** Upload a ready CPU mesh unless the exact handle version is already current. */
  MeshGpuSyncStatus synchronize(
    const MeshHandle& handle,
    const MeshData& mesh,
    BufferUsagePattern usagePattern = BufferUsagePattern::StaticDraw);

  /** Access uploaded buffers when constructing a draw context. */
  const MeshGpuData* lookup(const MeshHandle& handle) const noexcept;

  /** Upload or replace dynamic geometry that has no extraction-cache entry. */
  bool uploadOrReplace(
    const MeshData& mesh,
    const MeshHandle& handle,
    BufferUsagePattern usagePattern = BufferUsagePattern::StaticDraw);

  /** Remove handles and GPU uploads not represented by the current scene. */
  std::size_t retainOnly(const MeshGeometryKeySet& liveKeys);

  MeshGpuStore& gpuStore() noexcept;
  const MeshGpuStore& gpuStore() const noexcept;

private:
  MeshHandleFactory m_handleFactory;
  MeshHandleMap m_handles;
  MeshGpuStore m_gpuStore;
};

} // namespace rendering::mesh
