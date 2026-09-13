#pragma once

#include "logic/camera/CameraTypes.h"
#include "viewer/ViewTypes.h"

namespace windowing
{

/**
 * @brief Return the 2D slice view type used to initialize a view's dedicated slice camera
 * @param viewType Visible view type requested for the view
 * @return `ViewType::Axial` for 3D views; otherwise `viewType`
 */
ViewType initialSliceViewType(ViewType viewType) noexcept;

/**
 * @brief Return the default projection type for a view's dedicated 2D slice camera
 * @param viewType Visible view type requested for the view
 * @return Orthographic projection for all 2D slice-camera initialization
 */
ProjectionType initialSliceProjectionType(ViewType viewType) noexcept;

/**
 * @brief Return whether a visible view's slice camera tracks the live crosshairs frame
 * @param viewType Visible view type
 * @return True for orthogonal 2D views; false for oblique and 3D views
 */
bool sliceCameraTracksCrosshairs(ViewType viewType) noexcept;

/**
 * @brief Return whether default 2D framing should reserve space for overlaid view controls
 * @param isLightbox True when the controls belong to an entire lightbox layout
 * @return False for lightboxes, whose shared controls cover only a small fraction of the grid
 */
bool defaultFramingAvoidsControls(bool isLightbox) noexcept;

} // namespace windowing
