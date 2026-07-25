#pragma once

#include "rendering/mesh/MeshExtraction.h"
#include "rendering/mesh/MeshKeys.h"

#include <cstddef>
#include <optional>
#include <string>
#include <unordered_map>
#include <uuid.h>
#include <vector>

namespace rendering::mesh
{

/**
 * @brief Cached CPU mesh extraction entry
 */
struct MeshCacheEntry
{
  MeshCacheState state = MeshCacheState::Pending; //!< Current cache state
  std::optional<MeshData> mesh;                   //!< Extracted CPU mesh when ready
  std::vector<std::string> diagnostics;           //!< Non-fatal diagnostics or failure reasons
};

/**
 * @brief CPU-side cache for extracted meshes keyed by geometry-producing inputs
 *
 * The cache owns CPU meshes only. GPU upload and eviction are handled separately by `MeshGpuStore`, which keeps the
 * OpenGL lifetime boundary independent from extraction and cache policy.
 */
class MeshCache
{
public:
  /**
   * @brief Mark a geometry key as pending extraction
   * @param key Geometry key
   * @throw Propagates allocation failures
   */
  void markPending(MeshGeometryKey key);

  /**
   * @brief Store a successful extraction result
   * @param result Extracted mesh result
   * @throw Propagates allocation failures
   */
  void storeReady(MeshExtractionResult result);

  /**
   * @brief Store a successful result only if the matching entry is still pending
   * @param result Extracted mesh result
   * @return True when the result was accepted
   * @throw Propagates allocation failures
   */
  bool storeReadyIfPending(MeshExtractionResult result);

  /**
   * @brief Store a failed extraction state
   * @param key Geometry key
   * @param diagnostics Failure diagnostics
   * @throw Propagates allocation failures
   */
  void storeFailed(MeshGeometryKey key, std::vector<std::string> diagnostics);

  /**
   * @brief Store failure diagnostics only if the matching entry is still pending
   * @param key Geometry key
   * @param diagnostics Failure diagnostics
   * @return True when the failure was accepted
   * @throw Propagates allocation failures
   */
  bool storeFailedIfPending(MeshGeometryKey key, std::vector<std::string> diagnostics);

  /**
   * @brief Mark matching entries stale without removing their old mesh data
   * @param sourceUid Source image, segmentation, annotation, or imported mesh UID
   * @return Number of entries marked stale
   */
  std::size_t markSourceStale(const uuids::uuid& sourceUid);

  /**
   * @brief Evict matching entries and release their CPU mesh data
   * @param sourceUid Source image, segmentation, annotation, or imported mesh UID
   * @return Number of entries marked evicted
   */
  std::size_t evictSource(const uuids::uuid& sourceUid);

  /**
   * @brief Find an entry by exact geometry key
   * @param key Geometry key
   * @return Cache entry, or null when absent
   */
  const MeshCacheEntry* find(const MeshGeometryKey& key) const noexcept;

  /**
   * @brief Return a ready mesh by exact geometry key
   * @param key Geometry key
   * @return Ready mesh, or null when absent or not ready
   */
  const MeshData* readyMesh(const MeshGeometryKey& key) const noexcept;

  /**
   * @brief Return whether the cache contains an entry for a key
   * @param key Geometry key
   * @return Whether an entry exists
   */
  bool contains(const MeshGeometryKey& key) const noexcept;

  /**
   * @brief Return the number of entries
   * @return Entry count
   */
  std::size_t size() const noexcept;

  /**
   * @brief Remove every cache entry
   */
  void clear() noexcept;

private:
  std::unordered_map<MeshGeometryKey, MeshCacheEntry, MeshGeometryKeyHash> m_entries;
};

} // namespace rendering::mesh
