#ifndef ACCUMULATED_RENDERING_DATA_H
#define ACCUMULATED_RENDERING_DATA_H

#include <glm/mat4x4.hpp>

/**
 * @brief Data for rendering that gets accumulated as the tree of drawables is traversed.
 * In other words, this struct is used for propagating data from a drawable to its children.
 */
struct AccumulatedRenderingData
{
  /// Drawable's model matrix that transforms it from its own "modeling" space to World space.
  /// The drawable's vertex coordinates (contravariant tensors) transform with this matrix.
  /// Note that normal vectors (covariant tensors) transform with the inverse-transpose
  /// of this matrix.
  glm::mat4 m_world_T_object = glm::mat4{1.0f};

  /// Master opacity multiplier: it multiplies down all individual layer opacities.
  float m_masterOpacityMultiplier = 1.0f;

  /// Flag for whether the drawable is pickable.
  bool m_pickable = true;
};

#endif // ACCUMULATED_RENDERING_DATA_H
