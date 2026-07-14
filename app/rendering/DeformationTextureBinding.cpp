#include "rendering/Rendering.h"

#include "logic/app/Data.h"
#include "rendering/TextureSetup.h"
#include "rendering/utility/gl/GLTexture.h"

#include <algorithm>
#include <functional>
#include <list>
#include <optional>
#include <vector>

namespace
{
using namespace uuids;

const Uniforms::SamplerIndexVectorType msk_defTexSamplers{{4, 5, 6}};

uuid deformationSettingsOwnerImageUid(const AppData& appData, const uuid& imageUid)
{
  return appData.componentProjectionSourceImageUid(imageUid).value_or(imageUid);
}

} // namespace

std::list<std::reference_wrapper<GLTexture>> Rendering::bindDeformationTextures(const uuid& defUid)
{
  return bindDeformationTextures(defUid, msk_defTexSamplers);
}

std::list<std::reference_wrapper<GLTexture>> Rendering::bindDeformationTextures(
  const uuid& defUid,
  const Uniforms::SamplerIndexVectorType& samplers)
{
  std::list<std::reference_wrapper<GLTexture>> boundTextures;
  if (!ensureDeformationTexture(defUid)) {
    return boundTextures;
  }

  const Image* def = m_appData.warpField(defUid);
  if (!def) {
    return boundTextures;
  }

  auto& renderData = m_appData.renderData();
  auto textureIt = renderData.m_imageTextures.find(defUid);
  if (textureIt == std::end(renderData.m_imageTextures) || textureIt->second.empty()) {
    return boundTextures;
  }

  if (textureIt->second.size() == 1u) {
    GLTexture& texture = textureIt->second.front();
    for (const auto samplerUnit : samplers.indices) {
      texture.bind(samplerUnit);
    }
    boundTextures.push_back(texture);
    return boundTextures;
  }

  for (std::size_t component = 0; component < 3; ++component) {
    GLTexture& texture = textureIt->second.at(component);
    texture.bind(samplers.indices[component]);
    boundTextures.push_back(texture);
  }

  return boundTextures;
}

bool Rendering::ensureDeformationTexture(const uuid& defUid)
{
  const Image* def = m_appData.warpField(defUid);
  if (!def) {
    return false;
  }

  const auto textureLayoutIsUsable = [def](const std::vector<GLTexture>& textures) {
    return def->header().numComponentsPerPixel() >= 3u && (1u == textures.size() || textures.size() >= 3u);
  };

  auto& renderData = m_appData.renderData();
  auto textureIt = renderData.m_imageTextures.find(defUid);
  if (textureIt != std::end(renderData.m_imageTextures) && textureLayoutIsUsable(textureIt->second)) {
    return true;
  }

  if (textureIt != std::end(renderData.m_imageTextures)) {
    renderData.m_imageTextures.erase(textureIt);
    renderData.m_imageTextureLayouts.erase(defUid);
  }

  const std::vector<uuid> createdTextureUids = createImageTextures(m_appData, uuid_range_t{defUid});
  if (std::find(std::begin(createdTextureUids), std::end(createdTextureUids), defUid) == std::end(createdTextureUids)) {
    return false;
  }

  textureIt = renderData.m_imageTextures.find(defUid);
  return textureIt != std::end(renderData.m_imageTextures) && textureLayoutIsUsable(textureIt->second);
}

std::optional<uuid> Rendering::activeRenderableDeformationUid(const uuid& imageUid)
{
  const uuid ownerUid = deformationSettingsOwnerImageUid(m_appData, imageUid);
  const Image* image = m_appData.image(ownerUid);
  if (!image || !image->settings().warpEnabled() || image->settings().warpStrength() <= 0.0f) {
    return std::nullopt;
  }

  const auto defUid = m_appData.imageToActiveInverseWarpUid(ownerUid);
  const auto referenceUid = m_appData.imageToActiveInverseWarpReferenceImageUid(ownerUid);
  if (!defUid || !referenceUid || !m_appData.warpField(*defUid) || !m_appData.image(*referenceUid)) {
    return std::nullopt;
  }

  if (!ensureDeformationTexture(*defUid)) {
    return std::nullopt;
  }

  return defUid;
}

std::optional<uuid> Rendering::activeRenderableDeformationReferenceImageUid(const uuid& imageUid)
{
  const uuid ownerUid = deformationSettingsOwnerImageUid(m_appData, imageUid);
  if (!activeRenderableDeformationUid(imageUid)) {
    return std::nullopt;
  }
  return m_appData.imageToActiveInverseWarpReferenceImageUid(ownerUid);
}

void Rendering::setDeformationUniforms(
  GLShaderProgram& program,
  const uuid& imageUid,
  const uuid& defUid,
  const glm::mat4& sampleTex_T_world) const
{
  const uuid ownerUid = deformationSettingsOwnerImageUid(m_appData, imageUid);
  const Image* image = m_appData.image(ownerUid);
  const Image* def = m_appData.warpField(defUid);
  if (!image || !def) {
    return;
  }

  program.setSamplerUniform("u_defTex", msk_defTexSamplers);
  program.setUniform("u_defTex_T_world", def->transformations().texture_T_worldDef());
  program.setUniform("u_sampleTex_T_world", sampleTex_T_world);
  program.setUniform("u_defSlope_native_T_texture", def->settings().slope_native_T_texture());
  program.setUniform("u_deformationStrength", image->settings().warpStrength());
  const auto textureIt = m_appData.renderData().m_imageTextures.find(defUid);
  const bool packedDeformationTexture =
    textureIt != std::end(m_appData.renderData().m_imageTextures) && textureIt->second.size() == 1u;
  program.setUniform("u_defInterleaved", packedDeformationTexture);
}

void Rendering::setMetricDeformationUniforms(
  GLShaderProgram& program,
  std::size_t slot,
  const uuid& imageUid,
  const uuid& defUid,
  const glm::mat4& sampleTex_T_world) const
{
  const uuid ownerUid = deformationSettingsOwnerImageUid(m_appData, imageUid);
  const Image* image = m_appData.image(ownerUid);
  const Image* def = m_appData.warpField(defUid);
  if (!image || !def || slot >= 2u) {
    return;
  }

  const std::string index = "[" + std::to_string(slot) + "]";
  program.setUniform("u_defTex_T_world" + index, def->transformations().texture_T_worldDef());
  program.setUniform("u_sampleTex_T_world" + index, sampleTex_T_world);
  program.setUniform("u_defSlope_native_T_texture" + index, def->settings().slope_native_T_texture());
  program.setUniform("u_deformationStrength" + index, image->settings().warpStrength());
  program.setUniform("u_warpEnabled" + index, true);

  const auto textureIt = m_appData.renderData().m_imageTextures.find(defUid);
  const bool packedDeformationTexture =
    textureIt != std::end(m_appData.renderData().m_imageTextures) && textureIt->second.size() == 1u;
  program.setUniform("u_defInterleaved" + index, packedDeformationTexture);
}
