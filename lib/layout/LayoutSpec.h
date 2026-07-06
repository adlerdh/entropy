#pragma once

#include <cstddef>
#include <optional>
#include <set>
#include <string>
#include <vector>

namespace layout
{

/**
 * @brief Image selection state for a layout or individual view.
 *
 * Indices are zero-based positions in the current image list.
 */
struct ImageSelectionSpec
{
  std::vector<std::size_t> m_renderedImageIndices;       //!< Images rendered in the view.
  std::vector<std::size_t> m_volumeRenderedImageIndices; //!< Single image rendered in 3D volume raycasting.
  std::vector<std::size_t> m_metricImageIndices;         //!< Images used by metric/comparison modes.
};

/**
 * @brief Serializable state for one viewport within a layout.
 *
 * View specs store normalized viewport bounds, view/render/projection modes,
 * offset settings, synchronization groups, and image selection.
 */
struct ViewSpec
{
  float m_left = -1.0f;   //!< Normalized left edge in layout coordinates.
  float m_bottom = -1.0f; //!< Normalized bottom edge in layout coordinates.
  float m_width = 2.0f;   //!< Normalized width in layout coordinates.
  float m_height = 2.0f;  //!< Normalized height in layout coordinates.

  int m_viewType = 0;                //!< Serialized `ViewType` value.
  int m_renderMode = 0;              //!< Serialized `ViewRenderMode` value.
  int m_intensityProjectionMode = 0; //!< Serialized `IntensityProjectionMode` value.

  int m_offsetMode = 3;                                         //!< Serialized view offset mode.
  float m_absoluteOffset = 0.0f;                                //!< Absolute offset distance.
  int m_relativeOffsetSteps = 0;                                //!< Relative offset step count.
  std::optional<std::size_t> m_offsetImageIndex = std::nullopt; //!< Reference image index for offsets.

  std::optional<std::size_t> m_rotationSyncGroup = std::nullopt;              //!< Rotation source sync group.
  std::optional<std::size_t> m_translationSyncGroup = std::nullopt;           //!< Translation source sync group.
  std::optional<std::size_t> m_zoomSyncGroup = std::nullopt;                  //!< Zoom source sync group.
  std::optional<std::size_t> m_rotationSyncMembershipGroup = std::nullopt;    //!< Rotation sync membership.
  std::optional<std::size_t> m_translationSyncMembershipGroup = std::nullopt; //!< Translation sync membership.
  std::optional<std::size_t> m_zoomSyncMembershipGroup = std::nullopt;        //!< Zoom sync membership.

  std::set<std::size_t> m_preferredDefaultRenderedImages; //!< Preferred default rendered image indices.
  bool m_defaultRenderAllImages = true;                   //!< Whether this view renders all images by default.
  ImageSelectionSpec m_imageSelection;                    //!< Explicit image selection for this view.

  int m_threeDProjectionType = 1;               //!< Serialized 3D projection type.
  int m_threeDOrbitTargetMode = 0;              //!< Serialized 3D orbit target mode.
  bool m_threeDCameraFollowsCrosshairs = false; //!< Whether 3D camera position follows crosshairs.
  float m_threeDPerspectiveZoom = 1.0f;         //!< Saved 3D perspective projection zoom.
  float m_threeDOrthographicZoom = 1.0f;        //!< Saved 3D orthographic projection zoom.
};

/**
 * @brief Serializable state for an entire layout.
 *
 * Layout specs are used for project snapshots and detailed layout JSON.
 */
struct LayoutSpec
{
  int m_kind = 0;                                         //!< Serialized `LayoutKind` value.
  std::string m_displayName;                              //!< User name for custom layouts.
  bool m_isLightbox = false;                              //!< Whether this layout behaves as a lightbox.
  int m_viewType = 0;                                     //!< Default serialized `ViewType` value.
  int m_renderMode = 0;                                   //!< Default serialized `ViewRenderMode` value.
  int m_intensityProjectionMode = 0;                      //!< Default serialized `IntensityProjectionMode` value.
  std::set<std::size_t> m_preferredDefaultRenderedImages; //!< Preferred default rendered image indices.
  bool m_defaultRenderAllImages = false;                  //!< Whether generated views render all images.
  ImageSelectionSpec m_imageSelection;                    //!< Layout-level explicit image selection.
  std::vector<ViewSpec> m_views;                          //!< View specs in display order.
};

} // namespace layout
