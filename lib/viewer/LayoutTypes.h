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
 * Values are serialized; keep existing ordinals stable.
 */
enum class LayoutKind
{
  Custom = 0,                  //!< User-edited layout.
  FourUp = 1,                  //!< Axial/coronal/sagittal/3D layout.
  Tri = 2,                     //!< Three orthogonal slice views.
  SingleAxial = 3,             //!< Single axial view.
  MultiImageAxialGrid = 4,     //!< Multi-image axial grid.
  AxCorSagByImage = 5,         //!< Axial/coronal/sagittal views grouped by image.
  AxialLightbox = 6,           //!< Axial slice lightbox.
  CoronalLightbox = 7,         //!< Coronal slice lightbox.
  SagittalLightbox = 8,        //!< Sagittal slice lightbox.
  SingleCoronal = 9,           //!< Single coronal view.
  SingleSagittal = 10,         //!< Single sagittal view.
  MultiImageCoronalGrid = 11,  //!< Multi-image coronal grid.
  MultiImageSagittalGrid = 12, //!< Multi-image sagittal grid.
  NumElements                  //!< Sentinel count.
};
