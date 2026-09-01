#include "rendering/Rendering.h"

#include "image/Image.h"
#include "image/ImageSettings.h"
#include "image/Isosurface.h"
#include "logic/app/Data.h"
#include "rendering/PrivateMethods.h"
#include "rendering/RenderData.h"
#include "rendering/helpers/PipelineHelpers.h"
#include "windowing/View.h"

#include <uuid.h>

#include <iterator>
#include <list>
#include <optional>
#include <utility>

std::optional<Rendering::ImgSegPair> Rendering::raycastImageForView(const View& view)
{
  const CurrentImages imageSegPairs = raycastImagesForView(view);
  if (imageSegPairs.empty()) {
    return std::nullopt;
  }

  return imageSegPairs.front();
}

Rendering::CurrentImages Rendering::raycastImagesForView(const View& view)
{
  const RenderData& R = m_appData.renderData();
  CurrentImages imageSegPairs;
  for (ImgSegPair& pair : meshSceneImagesForView(view)) {
    if (!pair.first) {
      continue;
    }
    const uuids::uuid textureUid = m_appData.effectiveImageUidForRendering(*pair.first);
    if (!rendering::imageHasRaycastableTextureLayout(R.m_imageTextureLayouts, textureUid)) {
      continue;
    }

    const Image* image = m_appData.image(*pair.first);
    if (
      image && ((image->settings().vectorArrowOverlayVisible() && !image->settings().vectorArrowOverlayOnImage()) ||
                (image->settings().vectorWarpedGridVisible() && !image->settings().vectorWarpedGridOverlayOnImage())))
    {
      continue;
    }
    imageSegPairs.push_back(std::move(pair));
  }
  return imageSegPairs;
}

std::optional<Rendering::ImgSegPair> Rendering::meshSceneImageForView(const View& view)
{
  CurrentImages imageSegPairs = meshSceneImagesForView(view);
  if (imageSegPairs.empty()) {
    return std::nullopt;
  }
  return imageSegPairs.front();
}

Rendering::CurrentImages Rendering::meshSceneImagesForView(const View& view)
{
  const RenderData& R = m_appData.renderData();
  CurrentImages imageSegPairs;

  for (const uuids::uuid& imageUid : view.visibleImages()) {
    const Image* image = m_appData.image(imageUid);
    if (!image) {
      continue;
    }

    const uuids::uuid renderImageUid = m_appData.effectiveImageUidForRendering(imageUid);
    if (
      std::end(R.m_imageTextures) == R.m_imageTextures.find(renderImageUid) ||
      !rendering::imageHasMeshSceneTextureLayout(R.m_imageTextureLayouts, renderImageUid))
    {
      continue;
    }

    ImgSegPair imgSegPair;
    imgSegPair.first = imageUid;

    if (const auto segUid = m_appData.imageToActiveSegUid(imageUid)) {
      if (std::end(R.m_segTextures) != R.m_segTextures.find(*segUid)) {
        imgSegPair.second = *segUid;
      }
    }

    imageSegPairs.push_back(std::move(imgSegPair));
  }

  return imageSegPairs;
}

std::optional<Rendering::ActiveIsosurfaceEdit> Rendering::activeIsosurfaceEdit(const CurrentImages& imageSegPairs) const
{
  for (const ImgSegPair& imgSegPair : imageSegPairs) {
    if (!imgSegPair.first) {
      continue;
    }

    const uuids::uuid& imageUid = *imgSegPair.first;
    const Image* image = m_appData.image(imageUid);
    if (!image) {
      continue;
    }

    const ImageSettings& settings = image->settings();
    const uint32_t activeComponent = settings.activeComponent();
    for (const uuids::uuid& surfaceUid : m_appData.isosurfaceUids(imageUid, activeComponent)) {
      const Isosurface* surface = m_appData.isosurface(imageUid, activeComponent, surfaceUid);
      if (surface && surface->visible && surface->valueEditInProgress) {
        return ActiveIsosurfaceEdit{.imageSegPair = imgSegPair, .isosurfaceUid = surfaceUid};
      }
    }
  }

  return std::nullopt;
}
