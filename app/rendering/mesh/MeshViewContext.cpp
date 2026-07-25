#include "rendering/mesh/MeshViewContext.h"

#include "logic/camera/CameraHelpers.h"
#include "rendering/mesh/MeshGpuStore.h"
#include "windowing/View.h"

#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

namespace rendering::mesh
{

MeshDrawContext meshDrawContextForView(const MeshGpuStore& gpuStore, const View& view)
{
  return MeshDrawContext{
    .clip_T_world = helper::clip_T_world(view.camera()),
    .cameraWorldPosition = helper::worldOrigin(view.camera()),
    .lightDirectionWorld = glm::vec3{0.4f, 0.6f, 0.7f},
    .fallbackColor = glm::vec4{0.8f, 0.8f, 0.8f, 1.0f},
    .meshLookup = [&gpuStore](const MeshHandle& handle) -> const MeshGpuData* {
      return gpuStore.lookup(handle);
    }};
}

} // namespace rendering::mesh
