#ifndef I_DRAWABLE_H
#define I_DRAWABLE_H

#include "common/CoordinateFrame.h"
#include "common/Viewport.h"

#include "logic/camera/Camera.h"

#include "rendering/common/AccumulatedRenderingData.h"
#include "rendering/common/ShaderStageTypes.h"

#include <glm/fwd.hpp>

/**
 * @brief Interface for a drawable object in a 3D scene. Typical use is to arrange
 * drawable objects in a tree structure, where each object is parented and
 * itself contains a collection of child drawables.
 */
class IDrawable
{
public:
  virtual ~IDrawable() = default;

  /**
     * @brief Render the drawable. The drawable's state is not updated here.
     *
     * @param[in] renderStage Current rendering stage that can be used to modify how the object is rendered.
     * @param[in] objectsToRender Types of objects to render in this call.
     */
  virtual void render(const RenderStage& renderStage, const ObjectsToRender& objectsToRender) = 0;

  /**
     * @brief Update the drawable's internal state according to a new set of rendering parameters.
     *
     * @param[in] time Value might be set in order to update a running animation
     * @param[in] viewport Viewport of the view in which the object is rendered
     * @param[in] camera Camera of the view
     * @param[in] crosshairs Crosshairs of the view
     * @param[in] parentData Rendering data propagated from parent to child
     */
  virtual void update(
    double time,
    const Viewport& viewport,
    const camera::Camera& camera,
    const CoordinateFrame& crosshairs,
    const AccumulatedRenderingData& parentData
  ) = 0;

  /**
     * @brief printTree
     * @param depth
     */
  virtual void printTree(int depth) const = 0;
};

#endif // I_DRAWABLE_H
