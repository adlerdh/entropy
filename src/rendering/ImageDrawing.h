#ifndef IMAGE_DRAWING_H
#define IMAGE_DRAWING_H

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

void drawImageQuad(
  GLShaderProgram& program,
  const ViewRenderMode& renderMode,
  RenderData::Quad& quad,
  const View& view,
  const Viewport& windowViewport,
  const glm::vec3& worldOrigin,
  float flashlightRadius,
  bool flashlightOverlays,
  float mipSlabThickness_mm,
  bool doMaxExtentMip,
  float xrayIntensityWindow,
  float xrayIntensityLevel,
  const std::vector<std::pair<std::optional<uuids::uuid>, std::optional<uuids::uuid> > >& I,
  const std::function<const Image*(const std::optional<uuids::uuid>& imageUid)> getImage,
  bool showEdges);

void drawSegQuad(
  GLShaderProgram& program,
  const RenderData::Quad& quad,
  const Image& seg,
  const View& view,
  const Viewport& windowViewport,
  const glm::vec3& worldOrigin,
  float flashlightRadius,
  bool flashlightOverlays,
  const SegmentationOutlineStyle& segOutlineStyle,
  float segInteriorOpacity,
  float segInterpCutoff);

void drawRaycastQuad(
  GLShaderProgram& program,
  RenderData::Quad& quad,
  const View& view,
  const std::vector<std::pair<std::optional<uuids::uuid>, std::optional<uuids::uuid> > >& I,
  const std::function<const Image*(const std::optional<uuids::uuid>& imageUid)> getImage
);

#endif // IMAGE_DRAWING_H
