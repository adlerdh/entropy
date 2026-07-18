#include "rendering/Rendering.h"

#include "common/Types.h"
#include "image/Image.h"
#include "image/ImageColorMap.h"
#include "image/ImageSettings.h"
#include "image/ImageTypes.h"
#include "logic/app/Data.h"
#include "logic/app/ParcellationLabelTable.h"
#include "rendering/RenderData.h"
#include "rendering/utility/gl/GLBufferTexture.h"
#include "rendering/utility/gl/GLTexture.h"
#include "rendering/utility/gl/GLTextureTypes.h"

#include <spdlog/fmt/ostr.h>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <optional>
#include <unordered_map>
#include <vector>

namespace
{
using namespace uuids;

void syncScalarProjectionLayerSettings(const ImageSettings& source, ImageSettings& projection)
{
  constexpr uint32_t k_projectionComponent = 0;

  projection.setBorderColor(source.borderColor());
  projection.setGlobalVisibility(source.globalVisibility());
  projection.setGlobalOpacity(source.globalOpacity());
  projection.setVisibility(k_projectionComponent, source.visibility());
  projection.setOpacity(k_projectionComponent, source.opacity());
}

} // namespace

void Rendering::updateImageInterpolation(const uuid& imageUid)
{
  const uuid effectiveImageUid = m_appData.effectiveImageUidForRendering(imageUid);
  const auto* image = m_appData.image(effectiveImageUid);
  const auto* sourceImage = m_appData.image(imageUid);
  if (!image) {
    spdlog::warn("Image {} is invalid", imageUid);
    return;
  }

  if (effectiveImageUid != imageUid && sourceImage) {
    Image* mutableEffectiveImage = m_appData.image(effectiveImageUid);
    if (mutableEffectiveImage) {
      syncScalarProjectionLayerSettings(sourceImage->settings(), mutableEffectiveImage->settings());
    }
  }

  if (m_appData.renderData().m_imageTextures.find(effectiveImageUid) == m_appData.renderData().m_imageTextures.end()) {
    spdlog::debug("Image {} has no texture for interpolation update", effectiveImageUid);
    return;
  }

  const bool renderAllComponents =
    image->settings().displayImageAsColor() ||
    ComponentRenderMode::VectorDirectionColor == image->settings().componentRenderMode() ||
    ComponentRenderMode::VectorSignedNormalProjection == image->settings().componentRenderMode() ||
    ComponentRenderMode::VectorPlanarProjectionColor == image->settings().componentRenderMode();
  if (!renderAllComponents) {
    // Modify the active component
    const uint32_t activeComp = image->settings().activeComponent();
    auto& textures = m_appData.renderData().m_imageTextures.at(effectiveImageUid);
    if (textures.empty()) {
      spdlog::warn("Image {} has no component textures for interpolation update", effectiveImageUid);
      return;
    }

    const bool packedInterleavedTexture =
      Image::MultiComponentBufferType::InterleavedImage == image->bufferType() && textures.size() == 1u;
    const std::size_t textureIndex =
      packedInterleavedTexture ? 0u : std::min<std::size_t>(activeComp, textures.size() - 1u);
    if (textureIndex >= textures.size()) {
      spdlog::warn("Image {} component {} has no texture for interpolation update", effectiveImageUid, activeComp);
      return;
    }

    GLTexture& texture = textures[textureIndex];

    tex::MinificationFilter minFilter = tex::MinificationFilter::Linear;
    tex::MagnificationFilter maxFilter = tex::MagnificationFilter::Linear;

    switch (image->settings().interpolationMode(activeComp)) {
      case InterpolationMode::NearestNeighbor: {
        minFilter = tex::MinificationFilter::Nearest;
        maxFilter = tex::MagnificationFilter::Nearest;
        break;
      }
      case InterpolationMode::Linear:
      case InterpolationMode::CubicBsplineConvolution: {
        minFilter = tex::MinificationFilter::Linear;
        maxFilter = tex::MagnificationFilter::Linear;
        break;
      }
    }

    texture.setMinificationFilter(minFilter);
    texture.setMagnificationFilter(maxFilter);
    spdlog::debug("Set image interpolation mode for image {}", effectiveImageUid);
  }
  else {
    // Modify all components for color images
    auto& textures = m_appData.renderData().m_imageTextures.at(effectiveImageUid);
    for (GLTexture& texture : textures) {
      tex::MinificationFilter minFilter = tex::MinificationFilter::Linear;
      tex::MagnificationFilter maxFilter = tex::MagnificationFilter::Linear;

      switch (image->settings().colorInterpolationMode()) {
        case InterpolationMode::NearestNeighbor: {
          minFilter = tex::MinificationFilter::Nearest;
          maxFilter = tex::MagnificationFilter::Nearest;
          break;
        }
        case InterpolationMode::Linear:
        case InterpolationMode::CubicBsplineConvolution: {
          minFilter = tex::MinificationFilter::Linear;
          maxFilter = tex::MagnificationFilter::Linear;
          break;
        }
      }

      texture.setMinificationFilter(minFilter);
      texture.setMagnificationFilter(maxFilter);
      spdlog::debug("Set image interpolation mode for color image {}", imageUid);
    }
  }
}

void Rendering::updateImageColorMapInterpolation(std::size_t colorMapIndex)
{
  const auto cmapUid = m_appData.imageColorMapUid(colorMapIndex);
  if (!cmapUid) {
    spdlog::warn("Image color map index {} is invalid", colorMapIndex);
    return;
  }

  const auto* map = m_appData.imageColorMap(*cmapUid);
  if (!map) {
    spdlog::warn("Image color map {} is invalid", *cmapUid);
    return;
  }

  ImageColorMap* cmap = m_appData.imageColorMap(*cmapUid);
  if (!cmap) {
    spdlog::warn("Image color map {} is null", *cmapUid);
    return;
  }

  GLTexture& texture = m_appData.renderData().m_colormapTextures.at(*cmapUid);
  tex::MinificationFilter minFilter = tex::MinificationFilter::Linear;
  tex::MagnificationFilter maxFilter = tex::MagnificationFilter::Linear;

  switch (cmap->interpolationMode()) {
    case ImageColorMap::InterpolationMode::Nearest: {
      minFilter = tex::MinificationFilter::Nearest;
      maxFilter = tex::MagnificationFilter::Nearest;
      break;
    }
    case ImageColorMap::InterpolationMode::Linear: {
      minFilter = tex::MinificationFilter::Linear;
      maxFilter = tex::MagnificationFilter::Linear;
      break;
    }
  }

  texture.setMinificationFilter(minFilter);
  texture.setMagnificationFilter(maxFilter);

  spdlog::debug("Set interpolation mode for image color map {}", *cmapUid);
}

void Rendering::updateLabelColorTableTexture(std::size_t tableIndex)
{
  SPDLOG_TRACE("Begin updating texture for 1D label color map at index {}", tableIndex);

  if (tableIndex >= m_appData.numLabelTables()) {
    spdlog::error("Label color table at index {} does not exist", tableIndex);
    return;
  }

  const auto tableUid = m_appData.labelTableUid(tableIndex);
  if (!tableUid) {
    spdlog::error("Label table index {} is invalid", tableIndex);
    return;
  }

  const auto* table = m_appData.labelTable(*tableUid);
  if (!table) {
    spdlog::error("Label table {} is invalid", *tableUid);
    return;
  }

  auto it = m_appData.renderData().m_labelBufferTextures.find(*tableUid);
  if (std::end(m_appData.renderData().m_labelBufferTextures) == it) {
    spdlog::error("Buffer texture for label color table {} is invalid", *tableUid);
    return;
  }

  it->second.write(0, table->numColorBytes_RGBA_U8(), table->colorData_RGBA_nonpremult_U8());
  SPDLOG_TRACE("Done updating buffer texture for label color table {}", *tableUid);
}
