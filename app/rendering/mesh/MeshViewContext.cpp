#include "rendering/mesh/MeshViewContext.h"

#include "logic/camera/CameraHelpers.h"
#include "rendering/mesh/MeshGpuStore.h"
#include "windowing/View.h"

#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

namespace rendering::mesh
{

MeshDrawContext meshDrawContextForView(const MeshGpuStore& gpuStore, const View& view, const glm::vec4& lighting)
{
  const glm::vec3 cameraFrontWorld = helper::worldDirection(view.camera(), Directions::View::Front);

  return MeshDrawContext{
    .clip_T_world = helper::clip_T_world(view.camera()),
    .clip_T_camera = view.camera().clip_T_camera(),
    .camera_T_clip = view.camera().camera_T_clip(),
    .camera_T_world = view.camera().camera_T_world(),
    .cameraWorldPosition = helper::worldOrigin(view.camera()),
    .cameraFrontWorld = cameraFrontWorld,
    // Shadow and future advanced-lighting passes use a directional approximation of the shader's headlight.
    .lightDirectionWorld = -cameraFrontWorld,
    .lighting = lighting,
    .fallbackColor = glm::vec4{0.8f, 0.8f, 0.8f, 1.0f},
    .meshLookup = [&gpuStore](const MeshHandle& handle) -> const MeshGpuData* {
      return gpuStore.lookup(handle);
    }};
}

} // namespace rendering::mesh
