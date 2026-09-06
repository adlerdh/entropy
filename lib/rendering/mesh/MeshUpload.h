#pragma once

#include "rendering/mesh/MeshData.h"
#include "rendering/mesh/MeshGpuData.h"
#include "rendering/mesh/MeshHandle.h"
#include "rendering/utility/gl/GLBufferTypes.h"

#include <optional>

namespace rendering::mesh
{

/**
 * @brief Upload CPU mesh arrays into OpenGL buffers
 *
 * This must be called only when the target OpenGL context is current. Invalid meshes return `std::nullopt` before
 * creating GL resources.
 *
 * @param mesh CPU mesh data to upload
 * @param handle Mesh handle represented by the upload
 * @param usagePattern OpenGL buffer usage hint
 * @return Uploaded GPU mesh data, or empty when validation fails
 * @throw Propagates OpenGL wrapper errors and allocation failures
 */
std::optional<MeshGpuData> uploadMeshData(
  const MeshData& mesh,
  const MeshHandle& handle,
  BufferUsagePattern usagePattern = BufferUsagePattern::StaticDraw);

} // namespace rendering::mesh
