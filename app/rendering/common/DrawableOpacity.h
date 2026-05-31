#ifndef RENDERING_DRAWABLE_OPACITY_H
#define RENDERING_DRAWABLE_OPACITY_H

/**
 * @brief Describes the opacity of a rendered Drawable object. Opacity refers to the alpha value
 * of the object's rasterized fragments, where alpha == 1.0 means fully opaque and
 * 0.0 <= alpha < 1.0 means transparent.
 */
enum class OpacityFlag
{
  /// None of the object's rasterized fragments will be transparent.
  Opaque,

  /// At least one of the object's rasterized fragments is transparent.
  Transparent,

  /// The opacity of the object's rasterized is unknown, not determined, or not applicable
  /// (e.g. for Transformations or collections of other objects).
  Unknown
};

/**
 * @brief Holds two opacity flags for a Drawable object. One flag refers to the parent Drawable
 * object itself; the second flag refers collectively to all children and descendants of the
 * parent Drawable.
 */
struct DrawableOpacity
{
  /// Opacity of parent Drawable object (the object returning this struct).
  OpacityFlag m_parentFlag = OpacityFlag::Unknown;

  /// Collective opacity of the all descendants Drawables of the parent Drawable object.
  OpacityFlag m_descendantFlag = OpacityFlag::Unknown;
};

#endif // RENDERING_DRAWABLE_OPACITY_H
