#include "rendering/Rendering.h"

#include "common/UuidUtility.h"
#include "logic/app/Data.h"
#include "rendering/PrivateMethods.h"
#include "rendering/mesh/MeshPrimitives.h"
#include "rendering/mesh/MeshScene.h"
#include "windowing/View.h"

#include <glm/gtc/matrix_transform.hpp>
#include <spdlog/spdlog.h>

#include <cstdlib>
#include <string>

namespace
{

bool syntheticMeshSceneEnabled()
{
#ifdef _WIN32
  char* value = nullptr;
  size_t valueLength = 0;
  if (_dupenv_s(&value, &valueLength, "ENTROPY_MESH_TEST_SCENE") != 0 || !value) {
    return false;
  }

  const std::string text{value};
  std::free(value);
  return text == "1" || text == "true" || text == "TRUE" || text == "on" || text == "ON";
#else
  const char* value = std::getenv("ENTROPY_MESH_TEST_SCENE");
  if (!value) {
    return false;
  }

  const std::string text{value};
  return text == "1" || text == "true" || text == "TRUE" || text == "on" || text == "ON";
#endif
}

const rendering::mesh::MeshHandle& syntheticMeshHandle()
{
  static const rendering::mesh::MeshHandle handle{.uid = generateRandomUuid(), .geometryVersion = 1};
  return handle;
}

} // namespace

void Rendering::renderSyntheticMeshSceneForView(const View& view)
{
  if (!syntheticMeshSceneEnabled() || ViewType::ThreeD != view.viewType()) {
    return;
  }

  const rendering::mesh::MeshHandle& handle = syntheticMeshHandle();
  if (!m_meshGpuStore.lookup(handle)) {
    rendering::mesh::MeshData cube = rendering::mesh::makeCubeMesh(25.0f);
    if (!m_meshGpuStore.uploadOrReplace(cube, handle)) {
      spdlog::error("Unable to upload synthetic mesh test scene");
      return;
    }
  }

  const glm::vec3 crosshairs = m_appData.state().worldCrosshairs().worldOrigin();

  rendering::mesh::MeshRenderable renderable;
  renderable.mesh = handle;
  renderable.world_T_mesh = glm::translate(glm::mat4{1.0f}, crosshairs);
  renderable.material.baseColor = glm::vec4{0.2f, 0.75f, 0.95f, 0.82f};
  renderable.compositingMode = rendering::mesh::MeshCompositingMode::Opaque;

  rendering::mesh::MeshScene scene;
  scene.setRenderables({renderable});
  const rendering::mesh::MeshRenderList list = rendering::mesh::buildRenderList(scene.renderables());

  drawMeshRenderListForView(view, list);
}
