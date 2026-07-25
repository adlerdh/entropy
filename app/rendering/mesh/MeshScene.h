#pragma once

#include "rendering/mesh/MeshImagePlaneRenderable.h"
#include "rendering/mesh/MeshRenderable.h"

#include <span>
#include <vector>

namespace rendering::mesh
{

/**
 * @brief Renderer-facing mesh content for one view
 *
 * `MeshScene` stores view-specific renderables after application state has already been translated into mesh rendering
 * state. It intentionally does not know about images, segmentations, landmarks, or UI widgets.
 */
class MeshScene
{
public:
  /**
   * @brief Replace all renderables in the scene
   * @param renderables View-specific renderables
   * @throw Propagates allocation failures
   */
  void setRenderables(std::vector<MeshRenderable> renderables);

  /**
   * @brief Replace all textured image-plane renderables in the scene
   * @param imagePlanes View-specific textured image planes
   * @throw Propagates allocation failures
   */
  void setImagePlaneRenderables(std::vector<MeshImagePlaneRenderable> imagePlanes);

  /**
   * @brief Return all renderables in insertion order
   * @return Renderables
   */
  const std::vector<MeshRenderable>& renderables() const noexcept;

  /**
   * @brief Return all textured image-plane renderables in insertion order
   * @return Image-plane renderables
   */
  const std::vector<MeshImagePlaneRenderable>& imagePlaneRenderables() const noexcept;

  /**
   * @brief Remove all renderables from the scene
   */
  void clear() noexcept;

private:
  std::vector<MeshRenderable> m_renderables;
  std::vector<MeshImagePlaneRenderable> m_imagePlaneRenderables;
};

/**
 * @brief Explicit scene update for replacing view renderables
 */
struct MeshSceneUpdate
{
  std::vector<MeshRenderable> renderables;                     //!< Replacement material-shaded renderables for one view
  std::vector<MeshImagePlaneRenderable> imagePlaneRenderables; //!< Replacement textured image-plane renderables
};

/**
 * @brief Apply a full scene update
 * @param scene Scene to update
 * @param update Replacement scene content
 * @throw Propagates allocation failures
 */
void applySceneUpdate(MeshScene& scene, MeshSceneUpdate update);

} // namespace rendering::mesh
