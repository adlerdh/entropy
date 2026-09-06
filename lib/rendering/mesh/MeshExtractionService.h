#pragma once

#include "rendering/mesh/MeshCache.h"
#include "rendering/mesh/MeshExtractionQueue.h"
#include "rendering/mesh/MeshKeys.h"

#include <cstddef>
#include <string>
#include <vector>

namespace rendering::mesh
{

/**
 * @brief CPU-only coordinator for asynchronous mesh extraction and caching
 *
 * This service owns the background scheduler and the extracted CPU meshes. It deliberately has no OpenGL dependency.
 * Callers translate application objects into immutable extraction jobs, submit those jobs here, and consume completion
 * summaries on their owning thread.
 */
class MeshExtractionService
{
public:
  explicit MeshExtractionService(std::size_t maximumActiveJobs = 64);

  MeshExtractionService(const MeshExtractionService&) = delete;
  MeshExtractionService& operator=(const MeshExtractionService&) = delete;

  /** Return a ready mesh, or null when extraction has not completed successfully. */
  const MeshData* readyMesh(const MeshGeometryKey& key) const noexcept;

  /** Return the current cache entry, including failure and retry state. */
  const MeshCacheEntry* entry(const MeshGeometryKey& key) const noexcept;

  /** Return whether this key is absent or eligible for a bounded retry. */
  bool needsExtraction(const MeshGeometryKey& key, uint32_t maximumFailureCount = 2) const noexcept;

  /** Return whether a missing or failed key can be submitted without constructing an expensive job snapshot. */
  bool canSubmit(const MeshGeometryKey& key, uint32_t maximumFailureCount = 2) const;

  /**
   * Submit a CPU-only job and atomically mark its cache entry pending when accepted.
   * The service preserves the previous failure count so bounded retries remain correct.
   */
  bool submit(MeshGeometryKey key, std::string description, MeshExtractionJob job);

  /** Apply all completed jobs to the cache and return their status summaries. */
  std::vector<MeshExtractionRunResult> consumeCompleted();

  /** Evict cache entries and cancel queued jobs that are no longer represented by the scene. */
  std::size_t retainOnly(const MeshGeometryKeySet& liveKeys);

  std::size_t activeCount() const;
  std::vector<std::string> activeDescriptions() const;

private:
  MeshCache m_cache;
  MeshExtractionQueue m_queue;
};

} // namespace rendering::mesh
