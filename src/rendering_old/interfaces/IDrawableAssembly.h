#ifndef I_DRAWABLE_ASSEMBLY_H
#define I_DRAWABLE_ASSEMBLY_H

#include "rendering/drawables/DrawableBase.h"
#include "rendering_old/common/SceneType.h"

#include <memory>

/**
 * @brief Interface for class that creates and owns an "assembly" (tree) of drawable objects
 * that can be rendered in views.
 */
class IDrawableAssembly
{
public:
  virtual ~IDrawableAssembly() = default;

  /**
     * @brief Initialize and create the assembly. This function must be executed prior to requesting
     * the root drawable of the assembly. It must be assumed that executing function requires an active
     * OpenGL context.
     */
  virtual void initialize() = 0;

  /**
     * @brief Get a weak pointer to the root drawable of the assembly for a given scene type.
     */
  virtual std::weak_ptr<DrawableBase> getRoot(const SceneType& sceneType) = 0;
};

#endif // I_DRAWABLE_ASSEMBLY_H
