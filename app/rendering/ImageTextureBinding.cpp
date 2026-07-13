#include "rendering/Rendering.h"

#include "logic/app/Data.h"
#include "rendering/utility/gl/GLTexture.h"

#include <spdlog/spdlog.h>

#include <algorithm>
#include <functional>
#include <list>
#include <optional>

namespace
{
using namespace uuids;

const Uniforms::SamplerIndexType msk_imgTexSampler{0};
const Uniforms::SamplerIndexType msk_imgCmapTexSampler{1};
const Uniforms::SamplerIndexVectorType msk_imgRgbaTexSamplers{{0, 1, 2, 3}};
const Uniforms::SamplerIndexType msk_jumpTexSampler{1};
const Uniforms::SamplerIndexType msk_metricCmapTexSampler{2};
const Uniforms::SamplerIndexVectorType msk_metricImgTexSamplers{{0, 1}};

} // namespace

std::list<std::reference_wrapper<GLTexture>> Rendering::bindScalarImageTextures(const ImgSegPair& p)
{
  const auto& imgUid = p.first;
  const Image* image = (imgUid ? m_appData.image(*imgUid) : nullptr);

  std::list<std::reference_wrapper<GLTexture>> boundTextures;
  auto& R = m_appData.renderData();

  if (!image) {
    // No image, so bind the blank one:
    GLTexture& imgTex = R.m_blankImageBlackTransparentTexture;
    imgTex.bind(msk_imgTexSampler.index);
    boundTextures.push_back(imgTex);

    // Bind the first available colormap:
    auto it = std::begin(R.m_colormapTextures);
    GLTexture& cmapTex = it->second;
    cmapTex.bind(msk_imgCmapTexSampler.index);
    boundTextures.push_back(cmapTex);

    // The scalar image binder also supplies the distance-map slot used by volume rendering.
    GLTexture& distTex = R.m_blankDistMapTexture;
    distTex.bind(msk_jumpTexSampler.index);
    boundTextures.push_back(distTex);

    return boundTextures;
  }

  const ImageSettings& S = image->settings();

  // Uncomment this to render the image's distance map instead:
  // GLTexture& imgTex = R.m_distanceMapTextures.at(*imageUid).at(imgSettings.activeComponent());

  // Bind the active component of the image
  auto imageTextureIt = R.m_imageTextures.find(*imgUid);
  if (std::end(R.m_imageTextures) == imageTextureIt || imageTextureIt->second.empty()) {
    spdlog::warn("Image {} has no loaded texture; using a blank texture instead", *imgUid);
  }

  GLTexture& imgTex = (std::end(R.m_imageTextures) == imageTextureIt || imageTextureIt->second.empty())
                        ? R.m_blankImageBlackTransparentTexture
                        : imageTextureIt->second
                            [Image::MultiComponentBufferType::InterleavedImage == image->bufferType() &&
                                 imageTextureIt->second.size() == 1u
                               ? 0u
                               : std::min<std::size_t>(S.activeComponent(), imageTextureIt->second.size() - 1u)];
  imgTex.bind(msk_imgTexSampler.index);
  boundTextures.push_back(imgTex);

  // Bind the color map
  const auto cmapUid = image ? m_appData.imageColorMapUid(image->settings().colorMapIndex()) : std::nullopt;

  if (cmapUid) {
    GLTexture& cmapTex = R.m_colormapTextures.at(*cmapUid);
    cmapTex.bind(msk_imgCmapTexSampler.index);
    boundTextures.push_back(cmapTex);
  }
  else {
    // No colormap, so bind the first available one:
    auto it = std::begin(R.m_colormapTextures);
    GLTexture& cmapTex = it->second;
    cmapTex.bind(msk_imgCmapTexSampler.index);
    boundTextures.push_back(cmapTex);
  }

  const auto distTextureIt = R.m_distanceMapTextures.find(*imgUid);
  const bool useDistMap = S.useDistanceMapForRaycasting() && std::end(R.m_distanceMapTextures) != distTextureIt;
  bool foundMap = false;

  if (useDistMap) {
    const auto& distMaps = m_appData.distanceMaps(*imgUid, S.activeComponent());

    if (distMaps.empty()) {
      static bool alreadyShowedWarning = false;
      if (!alreadyShowedWarning) {
        spdlog::warn("No distance map for component {} of image {}", S.activeComponent(), *imgUid);
        alreadyShowedWarning = true;

        // Disable use of distance map for this image:
        if (Image* imageNonConst = (imgUid ? m_appData.image(*imgUid) : nullptr)) {
          imageNonConst->settings().setUseDistanceMapForRaycasting(false);
        }
      }
    }

    auto it2 = distTextureIt->second.find(S.activeComponent());
    if (std::end(distTextureIt->second) != it2) {
      foundMap = true;
      GLTexture& distTex = it2->second;
      distTex.bind(msk_jumpTexSampler.index);
      boundTextures.push_back(distTex);
    }
  }

  if (!useDistMap || !foundMap) {
    // Bind blank (zero) distance map:
    GLTexture& distTex = R.m_blankDistMapTexture;
    distTex.bind(msk_jumpTexSampler.index);
    boundTextures.push_back(distTex);
  }

  return boundTextures;
}

std::list<std::reference_wrapper<GLTexture>> Rendering::bindColorImageTextures(const ImgSegPair& p)
{
  const auto& imgUid = p.first;
  const Image* image = (imgUid ? m_appData.image(*imgUid) : nullptr);

  auto& R = m_appData.renderData();
  std::list<std::reference_wrapper<GLTexture>> boundTextures;

  if (!image) {
    // No image, so bind the blank one:
    GLTexture& imgTex = R.m_blankImageBlackTransparentTexture;
    imgTex.bind(msk_imgTexSampler.index);
    boundTextures.push_back(imgTex);
    return boundTextures;
  }

  // Bind the four (RGBA) components:
  auto& compTextures = R.m_imageTextures.at(*imgUid);

  for (std::size_t i = 0; i < 4; ++i) {
    const bool compExists =
      (i < static_cast<std::size_t>(image->settings().numComponents()) && i < compTextures.size());
    GLTexture& tex = compExists ? compTextures.at(i) : R.m_blankImageBlackTransparentTexture;
    tex.bind(msk_imgRgbaTexSamplers.indices[i]);
    boundTextures.push_back(tex);
  }

  return boundTextures;
}

std::list<std::reference_wrapper<GLTexture>> Rendering::bindMetricImageTextures(
  const CurrentImages& imageSegPairs,
  const ViewRenderMode& metricType)
{
  std::list<std::reference_wrapper<GLTexture>> textures;

  auto& R = m_appData.renderData();
  bool usesMetricColormap = false;
  std::size_t metricCmapIndex = 0;

  switch (metricType) {
    case ViewRenderMode::Difference: {
      usesMetricColormap = true;
      metricCmapIndex = R.m_squaredDifferenceParams.m_colorMapIndex;
      break;
    }
    case ViewRenderMode::LocalNcc: {
      usesMetricColormap = true;
      metricCmapIndex = R.m_localNccParams.m_colorMapIndex;
      break;
    }
    case ViewRenderMode::LocalLinearResidual: {
      usesMetricColormap = true;
      metricCmapIndex = R.m_localLinearResidualParams.m_colorMapIndex;
      break;
    }
    case ViewRenderMode::JointHistogram: {
      usesMetricColormap = true;
      metricCmapIndex = R.m_jointHistogramParams.m_colorMapIndex;
      break;
    }
    case ViewRenderMode::Overlay: {
      usesMetricColormap = false;
      break;
    }
    case ViewRenderMode::Disabled: {
      return textures;
    }
    default: {
      spdlog::error("Invalid metric shader type {}", typeString(metricType));
      return textures;
    }
  }

  if (usesMetricColormap) {
    const auto cmapUid = m_appData.imageColorMapUid(metricCmapIndex);
    if (cmapUid) {
      GLTexture& T = R.m_colormapTextures.at(*cmapUid);
      T.bind(msk_metricCmapTexSampler.index);
      textures.push_back(T);
    }
    else {
      auto it = std::begin(R.m_colormapTextures);
      GLTexture& T = it->second;
      T.bind(msk_metricCmapTexSampler.index);
      textures.push_back(T);
    }
  }

  std::size_t i = 0;

  for (const auto& [imgUid, segUid] : imageSegPairs) {
    const Image* image = (imgUid ? m_appData.image(*imgUid) : nullptr);
    if (image) {
      // Bind the active component
      const uint32_t activeComp = image->settings().activeComponent();
      auto textureIt = R.m_imageTextures.find(*imgUid);
      GLTexture& T = (std::end(R.m_imageTextures) == textureIt || textureIt->second.empty())
                       ? R.m_blankImageBlackTransparentTexture
                       : textureIt->second
                           [Image::MultiComponentBufferType::InterleavedImage == image->bufferType() &&
                                textureIt->second.size() == 1u
                              ? 0u
                              : std::min<std::size_t>(activeComp, textureIt->second.size() - 1u)];
      T.bind(msk_metricImgTexSamplers.indices[i]);
      textures.push_back(T);
    }
    else {
      GLTexture& T = R.m_blankImageBlackTransparentTexture;
      T.bind(msk_metricImgTexSamplers.indices[i]);
      textures.push_back(T);
    }
    ++i;
  }

  return textures;
}
