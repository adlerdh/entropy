#pragma once

#include "rendering/mesh/MeshData.h"
#include "rendering/mesh/MeshGpuData.h"
#include "rendering/mesh/MeshHandle.h"
#include "rendering/utility/gl/GLBufferTypes.h"

#include <cstddef>
#include <optional>
#include <unordered_map>
#include <uuid.h>

namespace rendering::mesh
{

/**
 * @brief Owns uploaded mesh buffers for one OpenGL context
 *
 * The store is the renderer-side boundary for replacing or releasing GPU mesh resources. It does not own CPU mesh data
 * and does not know how meshes were extracted.
 */
class MeshGpuStore
{
public:
  /**
   * @brief Upload a mesh or replace the previous upload for the same logical mesh
   * @param mesh CPU mesh data to upload
   * @param handle Logical mesh identity and geometry version
   * @param usagePattern OpenGL buffer usage hint
   * @return True when upload succeeds
   * @throw Propagates OpenGL wrapper errors and allocation failures
   */
  bool uploadOrReplace(
    const MeshData& mesh,
    const MeshHandle& handle,
    BufferUsagePattern usagePattern = BufferUsagePattern::StaticDraw);

  /**
   * @brief Find uploaded buffers for an exact mesh handle
   * @param handle Mesh handle to resolve
   * @return Uploaded buffers, or null when no matching geometry version exists
   */
  const MeshGpuData* lookup(const MeshHandle& handle) const noexcept;

  /**
   * @brief Remove one logical mesh and release its GPU buffers
   * @param uid Logical mesh UID
   * @return Whether an upload was removed
   */
  bool remove(const uuids::uuid& uid) noexcept;

  /**
   * @brief Release all uploaded mesh buffers
   */
  void clear() noexcept;

  /**
   * @brief Return the number of logical meshes currently uploaded
   * @return Upload count
   */
  std::size_t size() const noexcept;

private:
  std::unordered_map<uuids::uuid, MeshGpuData> m_uploads;
};

} // namespace rendering::mesh
