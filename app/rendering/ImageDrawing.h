#pragma once

#include "common/Viewport.h"
#include "logic/camera/CameraTypes.h"
#include "rendering/RenderData.h"

#include <glm/fwd.hpp>

#include <uuid.h>

#include <functional>
#include <optional>
#include <utility>
#include <vector>

class GLShaderProgram;
class Image;
class View;

/**
 * @brief Draw the primary image plane for a 2D view.
 *
 * This binds the supplied image-plane shader program, uploads the uniforms needed by the selected render mode, and
 * draws the shared quad geometry. The image list is interpreted as one or more fixed/moving image pairs, depending on
 * the render mode. Single-image modes use the first image in the first pair; comparison and metric modes may use
 * additional entries.
 *
 * @param program Shader program selected for the current image render path.
 * @param renderMode Image render mode to draw.
 * @param quad Shared quad geometry used for image-plane rendering.
 * @param view View whose camera, clip depth, and view transform define the rendered plane.
 * @param windowViewport Viewport in window pixel coordinates.
 * @param worldCrosshairs Current crosshairs position in world/LPS coordinates.
 * @param flashlightRadius Flashlight overlay radius in normalized view coordinates.
 * @param flashlightOverlays When true, the flashlight shows the moving image over the fixed image.
 * @param mipSlabThickness_mm Slab thickness, in millimeters, for intensity projection modes.
 * @param doMaxExtentMip When true, sample the maximum image extent for intensity projection.
 * @param xrayIntensityWindow Window width used by the X-ray projection shader.
 * @param xrayIntensityLevel Window level used by the X-ray projection shader.
 * @param imagePairs Fixed/moving image UID pairs used by the selected render mode.
 * @param getImage Lookup callback that resolves an optional image UID to an image pointer.
 * @param showEdges When true, render pixel edge overlays instead of intensity projection sampling.
 */
void drawImageQuad(
  GLShaderProgram& program,
  const ViewRenderMode& renderMode,
  RenderData::Quad& quad,
  const View& view,
  const Viewport& windowViewport,
  const glm::vec3& worldCrosshairs,
  float flashlightRadius,
  bool flashlightOverlays,
  float mipSlabThickness_mm,
  bool doMaxExtentMip,
  float xrayIntensityWindow,
  float xrayIntensityLevel,
  const std::vector<std::pair<std::optional<uuids::uuid>, std::optional<uuids::uuid> > >& imagePairs,
  const std::function<const Image*(const std::optional<uuids::uuid>& imageUid)> getImage,
  bool showEdges);

/**
 * @brief Draw a segmentation overlay on the current 2D image plane.
 *
 * The segmentation image is sampled in the deformed world space used by the view. The function configures outline,
 * smooth-fill, flashlight, and crosshairs uniforms before drawing the shared quad geometry.
 *
 * @param program Shader program selected for segmentation rendering.
 * @param quad Shared quad geometry used for image-plane rendering.
 * @param seg Segmentation image to render.
 * @param view View whose camera, clip depth, and view transform define the rendered plane.
 * @param windowViewport Viewport in window pixel coordinates.
 * @param worldCrosshairs Current crosshairs position in world/LPS coordinates.
 * @param flashlightRadius Flashlight overlay radius in normalized view coordinates.
 * @param flashlightOverlays When true, apply flashlight clipping to segmentation overlays.
 * @param segOutlineStyle Segmentation outline style.
 * @param segInteriorOpacity Interior opacity used when outlines are enabled.
 * @param segInterpCutoff Threshold used when rendering interpolated segmentation values.
 */
void drawSegQuad(
  GLShaderProgram& program,
  const RenderData::Quad& quad,
  const Image& seg,
  const Image& geometryImage,
  const View& view,
  const Viewport& windowViewport,
  const glm::vec3& worldCrosshairs,
  float flashlightRadius,
  bool flashlightOverlays,
  const SegmentationOutlineStyle& segOutlineStyle,
  float segInteriorOpacity,
  float segInterpCutoff);

/**
 * @brief Draw the transient segmentation preview overlay.
 *
 * This path is used for interactive segmentation previews before the preview has been committed to the segmentation
 * image. The caller supplies the preview texture transforms directly instead of passing an Image object.
 *
 * @param program Shader program selected for segmentation preview rendering.
 * @param quad Shared quad geometry used for image-plane rendering.
 * @param texture_T_world Transform from world/LPS coordinates to preview texture coordinates.
 * @param voxel_T_world Transform from world/LPS coordinates to preview voxel coordinates.
 * @param textureSize Preview texture size in texels.
 * @param view View whose camera, clip depth, and view transform define the rendered plane.
 * @param windowViewport Viewport in window pixel coordinates.
 * @param worldCrosshairs Current crosshairs position in world/LPS coordinates.
 * @param flashlightRadius Flashlight overlay radius in normalized view coordinates.
 * @param flashlightOverlays When true, apply flashlight clipping to the preview overlay.
 * @param segOutlineStyle Segmentation outline style.
 * @param segInteriorOpacity Interior opacity used when outlines are enabled.
 */
void drawSegPreviewQuad(
  GLShaderProgram& program,
  const RenderData::Quad& quad,
  const glm::mat4& texture_T_world,
  const glm::mat4& voxel_T_world,
  const glm::uvec3& textureSize,
  const Image* geometryImage,
  const View& view,
  const Viewport& windowViewport,
  const glm::vec3& worldCrosshairs,
  float flashlightRadius,
  bool flashlightOverlays,
  const SegmentationOutlineStyle& segOutlineStyle,
  float segInteriorOpacity);

/**
 * @brief Draw a 3D raycast pass for the visible image in a 3D view.
 *
 * The raycast shader draws the viewport quad on the near clip plane, then performs ray-volume intersection in the
 * fragment shader. Only the first image in @p imagePairs is used by the current raycast path.
 *
 * @param program Shader program selected for raycast isosurface rendering.
 * @param quad Shared viewport quad used for the screen-space raycast pass.
 * @param view 3D view whose camera defines the eye rays and projection.
 * @param texture_T_world Transform from world/LPS coordinates to image texture coordinates.
 * @param imagePairs Visible image UID pairs; the first fixed image is raycast.
 * @param getImage Lookup callback that resolves an optional image UID to an image pointer.
 */
void drawRaycastQuad(
  GLShaderProgram& program,
  RenderData::Quad& quad,
  const View& view,
  const glm::mat4& texture_T_world,
  const std::vector<std::pair<std::optional<uuids::uuid>, std::optional<uuids::uuid> > >& imagePairs,
  const std::function<const Image*(const std::optional<uuids::uuid>& imageUid)> getImage);
