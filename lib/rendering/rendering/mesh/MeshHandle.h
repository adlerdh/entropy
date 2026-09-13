#pragma once

#include <uuid.h>

#include <cstdint>

namespace rendering::mesh
{

/**
 * @brief Stable identity for a CPU/GPU mesh geometry version
 *
 * The UUID identifies the logical mesh. The geometry version changes when vertex, normal, color, or index buffers
 * need to be replaced.
 */
struct MeshHandle
{
  uuids::uuid uid = {};
  uint64_t geometryVersion = 0; //!< Geometry version for stale upload/renderable detection

  bool operator==(const MeshHandle&) const = default;
};

} // namespace rendering::mesh
