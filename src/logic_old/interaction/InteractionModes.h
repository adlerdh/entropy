#ifndef INTERACTION_MODES_H
#define INTERACTION_MODES_H

enum class CameraInteractionMode
{
  Translate,
  Rotate,
  Zoom
};

/**
 * @brief Defines the point picking mode for the CrosshairsInteractionHandler
 */
enum class CrosshairsPointPickingMode
{
  PlanarPicking, //!< Pick points on 2D cross-sections of images
  DepthPicking   //!< Pick points on 3D objects using depth
};

enum class CrosshairsInteractionMode
{
  Move
};

enum class RefImageInteractionMode
{
  Translate,
  Rotate
};

enum class SlideInteractionMode
{
  Rotate,
  Stretch, // includes scale and shear
  Translate
};

enum class StackInteractionMode
{
  Translate,
  Rotate
};

enum class WindowLevelInteractionMode
{
  Default
};

#endif // INTERACTION_MODES_H
