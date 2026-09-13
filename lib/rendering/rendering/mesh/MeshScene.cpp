#include "rendering/mesh/MeshScene.h"

#include <utility>

namespace rendering::mesh
{

void MeshScene::setRenderables(std::vector<MeshRenderable> renderablesArg)
{
  m_renderables = std::move(renderablesArg);
}

void MeshScene::setImagePlaneRenderables(std::vector<MeshImagePlaneRenderable> imagePlanes)
{
  m_imagePlaneRenderables = std::move(imagePlanes);
}

const std::vector<MeshRenderable>& MeshScene::renderables() const noexcept
{
  return m_renderables;
}

const std::vector<MeshImagePlaneRenderable>& MeshScene::imagePlaneRenderables() const noexcept
{
  return m_imagePlaneRenderables;
}

void MeshScene::clear() noexcept
{
  m_renderables.clear();
  m_imagePlaneRenderables.clear();
}

void applySceneUpdate(MeshScene& scene, MeshSceneUpdate update)
{
  scene.setRenderables(std::move(update.renderables));
  scene.setImagePlaneRenderables(std::move(update.imagePlaneRenderables));
}

} // namespace rendering::mesh
