#pragma once

/**
 * @brief Camera property synchronized between views.
 */
enum class CameraSyncMode
{
  Rotation,    //!< View rotation/orientation.
  Translation, //!< Crosshair translation.
  Zoom         //!< Zoom level.
};

/**
 * @brief Runtime layout kind.
 *
 * Values are serialized; update the JSON mapping and tests when this list changes.
 */
enum class LayoutKind
{
  Custom = 0,          //!< User-edited layout.
  FourUp = 1,          //!< Axial/coronal/sagittal/3D layout.
  ThreeUp = 2,         //!< Three orthogonal slice views.
  OneUp = 3,           //!< Single view.
  MultiImageGrid = 4,  //!< Multi-image grid.
  AxCorSagByImage = 5, //!< Axial/coronal/sagittal views grouped by image.
  Lightbox = 6,        //!< Slice lightbox.
  NumElements          //!< Sentinel count.
};
