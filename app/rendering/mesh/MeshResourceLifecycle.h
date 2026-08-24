#pragma once

#include "rendering/mesh/MeshCache.h"
#include "rendering/mesh/MeshHandle.h"
#include "rendering/mesh/MeshKeys.h"

#include <functional>
#include <unordered_map>
#include <unordered_set>
#include <uuid.h>

namespace rendering::mesh
{

using MeshHandleMap = std::unordered_map<MeshGeometryKey, MeshHandle, MeshGeometryKeyHash>;
using MeshGeometryKeySet = std::unordered_set<MeshGeometryKey, MeshGeometryKeyHash>;

/**
 * Remove extracted CPU meshes and logical handles that are no longer represented by application state.
 * The callback releases the corresponding context-owned GPU upload without coupling this policy to OpenGL.
 */
std::size_t reconcileExtractedMeshResources(
  const MeshGeometryKeySet& liveKeys,
  MeshCache& cache,
  MeshHandleMap& handles,
  const std::function<void(const uuids::uuid&)>& releaseGpuUpload);

} // namespace rendering::mesh
