#include "rendering/Rendering.h"

#include "image/Image.h"
#include "image/ImageSettings.h"
#include "logic/app/Data.h"
#include "rendering/RenderData.h"
#include "rendering/utility/containers/Uniforms.h"
#include "rendering/utility/gl/GLBufferTexture.h"
#include "rendering/utility/gl/GLTexture.h"

#include <uuid.h>

#include <functional>
#include <iterator>
#include <list>
#include <optional>
#include <unordered_map>

namespace
{
using namespace uuids;

const Uniforms::SamplerIndexType msk_segTexSampler{0};
const Uniforms::SamplerIndexType msk_segLabelTableTexSampler{1};

} // namespace

std::list<std::reference_wrapper<GLTexture>> Rendering::bindSegTextures(const ImgSegPair& p)
{
  const auto& segUid = p.second;

  std::list<std::reference_wrapper<GLTexture>> boundTextures;
  auto& R = m_appData.renderData();

  if (segUid) {
    // Uncomment this to render the image's distance map instead:
    // GLTexture& segTex = R.m_distanceMapTextures.at( *imageUid ).at( 0 );
    GLTexture& segTex = R.m_segTextures.at(*segUid);
    segTex.bind(msk_segTexSampler.index);
    boundTextures.push_back(segTex);
  }
  else {
    // No segmentation, so bind the blank one:
    GLTexture& segTex = R.m_blankSegTexture;
    segTex.bind(msk_segTexSampler.index);
    boundTextures.push_back(segTex);
  }

  return boundTextures;
}

std::list<std::reference_wrapper<GLBufferTexture>> Rendering::bindSegBufferTextures(const ImgSegPair& p)
{
  std::list<std::reference_wrapper<GLBufferTexture>> boundBufferTextures;
  auto& R = m_appData.renderData();

  const auto& segUid = p.second;

  const Image* seg = segUid ? m_appData.seg(*p.second) : nullptr;
  const auto tableUid = seg ? m_appData.labelTableUid(seg->settings().labelTableIndex()) : std::nullopt;

  if (tableUid) {
    GLBufferTexture& tblTex = R.m_labelBufferTextures.at(*tableUid);
    tblTex.bind(msk_segLabelTableTexSampler.index);
    tblTex.attachBufferToTexture(msk_segLabelTableTexSampler.index);
    boundBufferTextures.push_back(tblTex);
  }
  else {
    // No label table, so bind the first available one:
    auto it = std::begin(R.m_labelBufferTextures);
    GLBufferTexture& tblTex = it->second;
    tblTex.bind(msk_segLabelTableTexSampler.index);
    tblTex.attachBufferToTexture(msk_segLabelTableTexSampler.index);
    boundBufferTextures.push_back(tblTex);
  }

  return boundBufferTextures;
}

void Rendering::unbindBufferTextures(const std::list<std::reference_wrapper<GLBufferTexture>>& textures)
{
  for (auto& T : textures) {
    T.get().unbind();
  }
}
