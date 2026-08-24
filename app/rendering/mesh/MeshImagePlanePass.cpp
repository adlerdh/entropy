#include "rendering/Rendering.h"

#include "image/Image.h"
#include "logic/app/Data.h"
#include "logic/SurfaceUtility.h"
#include "rendering/PrivateMethods.h"
#include "rendering/helpers/PipelineHelpers.h"
#include "rendering/mesh/MeshGpuData.h"
#include "rendering/mesh/MeshImagePlaneRenderList.h"
#include "rendering/mesh/MeshViewContext.h"
#include "rendering/mesh/MeshViewViewport.h"
#include "rendering/utility/gl/GLShaderProgram.h"
#include "rendering/utility/gl/GLBufferTexture.h"
#include "rendering/utility/gl/GLTexture.h"
#include "rendering/utility/containers/Uniforms.h"
#include "viewer/ViewModes.h"
#include "windowing/View.h"

#include <glad/glad.h>
#include <spdlog/spdlog.h>

#include <glm/geometric.hpp>
#include <glm/gtc/matrix_inverse.hpp>
#include <glm/mat3x3.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include <algorithm>
#include <functional>
#include <list>
#include <optional>
#include <vector>

namespace
{

constexpr Uniforms::SamplerIndexType sk_imgTexSampler{0};
constexpr Uniforms::SamplerIndexType sk_imgCmapTexSampler{1};
constexpr Uniforms::SamplerIndexType sk_segTexSampler{2};
constexpr Uniforms::SamplerIndexType sk_segLabelTableTexSampler{3};
constexpr Uniforms::SamplerIndexType sk_previousDepthBoundsSampler{4};
constexpr Uniforms::SamplerIndexType sk_previousFrontColorSampler{5};

class ScopedImagePlaneBlendState
{
public:
  ScopedImagePlaneBlendState()
  {
    m_blendEnabled = glIsEnabled(GL_BLEND);
    glGetIntegerv(GL_BLEND_SRC_RGB, &m_srcRgb);
    glGetIntegerv(GL_BLEND_DST_RGB, &m_dstRgb);
    glGetIntegerv(GL_BLEND_SRC_ALPHA, &m_srcAlpha);
    glGetIntegerv(GL_BLEND_DST_ALPHA, &m_dstAlpha);
    glGetIntegerv(GL_BLEND_EQUATION_RGB, &m_equationRgb);
    glGetIntegerv(GL_BLEND_EQUATION_ALPHA, &m_equationAlpha);
  }

  ScopedImagePlaneBlendState(const ScopedImagePlaneBlendState&) = delete;
  ScopedImagePlaneBlendState& operator=(const ScopedImagePlaneBlendState&) = delete;

  ~ScopedImagePlaneBlendState()
  {
    glBlendEquationSeparate(static_cast<GLenum>(m_equationRgb), static_cast<GLenum>(m_equationAlpha));
    glBlendFuncSeparate(
      static_cast<GLenum>(m_srcRgb),
      static_cast<GLenum>(m_dstRgb),
      static_cast<GLenum>(m_srcAlpha),
      static_cast<GLenum>(m_dstAlpha));
    m_blendEnabled ? glEnable(GL_BLEND) : glDisable(GL_BLEND);
  }

private:
  GLboolean m_blendEnabled = GL_FALSE;
  GLint m_srcRgb = GL_ONE;
  GLint m_dstRgb = GL_ZERO;
  GLint m_srcAlpha = GL_ONE;
  GLint m_dstAlpha = GL_ZERO;
  GLint m_equationRgb = GL_FUNC_ADD;
  GLint m_equationAlpha = GL_FUNC_ADD;
};

class ScopedImagePlaneDepthState
{
public:
  ScopedImagePlaneDepthState()
  {
    m_depthTestEnabled = glIsEnabled(GL_DEPTH_TEST);
    m_stencilTestEnabled = glIsEnabled(GL_STENCIL_TEST);
    glGetBooleanv(GL_DEPTH_WRITEMASK, &m_depthWriteEnabled);
    glGetIntegerv(GL_DEPTH_FUNC, &m_depthFunc);
  }

  ScopedImagePlaneDepthState(const ScopedImagePlaneDepthState&) = delete;
  ScopedImagePlaneDepthState& operator=(const ScopedImagePlaneDepthState&) = delete;

  ~ScopedImagePlaneDepthState()
  {
    glDepthFunc(static_cast<GLenum>(m_depthFunc));
    glDepthMask(m_depthWriteEnabled);
    m_depthTestEnabled ? glEnable(GL_DEPTH_TEST) : glDisable(GL_DEPTH_TEST);
    m_stencilTestEnabled ? glEnable(GL_STENCIL_TEST) : glDisable(GL_STENCIL_TEST);
  }

private:
  GLboolean m_depthTestEnabled = GL_FALSE;
  GLboolean m_stencilTestEnabled = GL_FALSE;
  GLboolean m_depthWriteEnabled = GL_TRUE;
  GLint m_depthFunc = GL_LESS;
};

GLShaderProgram& shaderProgramForImagePlaneTextureDimension(
  GLShaderProgram& texture3dProgram,
  GLShaderProgram& texture2dProgram,
  const rendering::TextureDimension textureDimension)
{
  return rendering::TextureDimension::Texture2D == textureDimension ? texture2dProgram : texture3dProgram;
}

std::list<std::reference_wrapper<GLTexture>> bindImagePlaneTextures(
  AppData& appData,
  const uuids::uuid& imageUid,
  const uint32_t component,
  const RenderData::PlanarTextureLayout& textureLayout)
{
  auto& renderData = appData.renderData();
  const Image* image = appData.image(imageUid);
  std::list<std::reference_wrapper<GLTexture>> boundTextures;

  GLTexture& blankImageTexture = rendering::TextureDimension::Texture2D == textureLayout.dimension
                                   ? renderData.m_blankImageBlackTransparentTexture2D
                                   : renderData.m_blankImageBlackTransparentTexture;
  GLTexture* imageTexture = &blankImageTexture;

  const auto textureIt = renderData.m_imageTextures.find(imageUid);
  if (image && std::end(renderData.m_imageTextures) != textureIt && !textureIt->second.empty()) {
    const std::size_t componentTextureIndex =
      Image::MultiComponentBufferType::InterleavedImage == image->bufferType() && textureIt->second.size() == 1u
        ? 0u
        : std::min<std::size_t>(component, textureIt->second.size() - 1u);
    imageTexture = &textureIt->second.at(componentTextureIndex);
  }

  imageTexture->bind(sk_imgTexSampler.index);
  boundTextures.emplace_back(*imageTexture);

  const std::optional<uuids::uuid> cmapUid =
    image ? appData.imageColorMapUid(image->settings().colorMapIndex()) : std::nullopt;
  GLTexture& colorMapTexture =
    cmapUid ? renderData.m_colormapTextures.at(*cmapUid) : std::begin(renderData.m_colormapTextures)->second;
  colorMapTexture.bind(sk_imgCmapTexSampler.index);
  boundTextures.emplace_back(colorMapTexture);

  return boundTextures;
}

struct BoundImagePlaneSegmentationTexture
{
  std::reference_wrapper<GLTexture> texture;
  bool hasSegmentation;
};

BoundImagePlaneSegmentationTexture bindImagePlaneSegmentationTexture(
  AppData& appData,
  const std::optional<uuids::uuid>& segmentationUid,
  RenderData::TextureDimension textureDimension)
{
  auto& renderData = appData.renderData();
  GLTexture* texture = textureDimension == RenderData::TextureDimension::Texture2D ? &renderData.m_blankSegTexture2D
                                                                                   : &renderData.m_blankSegTexture;
  bool hasSegmentation = false;

  if (segmentationUid) {
    const auto textureIt = renderData.m_segTextures.find(*segmentationUid);
    if (std::end(renderData.m_segTextures) != textureIt) {
      texture = &textureIt->second;
      hasSegmentation = true;
    }
  }

  texture->bind(sk_segTexSampler.index);
  return {*texture, hasSegmentation};
}

std::list<std::reference_wrapper<GLBufferTexture>> bindImagePlaneSegmentationLabelTableTextures(
  AppData& appData,
  const std::optional<uuids::uuid>& segmentationUid)
{
  std::list<std::reference_wrapper<GLBufferTexture>> boundTextures;
  if (appData.renderData().m_labelBufferTextures.empty()) {
    return boundTextures;
  }

  const Image* segmentation = segmentationUid ? appData.seg(*segmentationUid) : nullptr;
  const std::optional<uuids::uuid> tableUid =
    segmentation ? appData.labelTableUid(segmentation->settings().labelTableIndex()) : std::nullopt;
  auto tableIt = tableUid ? appData.renderData().m_labelBufferTextures.find(*tableUid)
                          : appData.renderData().m_labelBufferTextures.end();
  if (std::end(appData.renderData().m_labelBufferTextures) == tableIt) {
    tableIt = std::begin(appData.renderData().m_labelBufferTextures);
  }

  tableIt->second.attachBufferToTexture(sk_segLabelTableTexSampler.index);
  boundTextures.emplace_back(tableIt->second);
  return boundTextures;
}

void unbindImagePlaneSegmentationLabelTableTextures(const std::list<std::reference_wrapper<GLBufferTexture>>& textures)
{
  for (const std::reference_wrapper<GLBufferTexture> texture : textures) {
    texture.get().unbind();
  }
}

std::array<glm::vec3, 2> imagePlaneWorldAxes(const rendering::mesh::MeshImagePlaneOrientation orientation) noexcept
{
  switch (orientation) {
    case rendering::mesh::MeshImagePlaneOrientation::Axial:
      return {glm::vec3{1.0f, 0.0f, 0.0f}, glm::vec3{0.0f, 1.0f, 0.0f}};
    case rendering::mesh::MeshImagePlaneOrientation::Coronal:
      return {glm::vec3{1.0f, 0.0f, 0.0f}, glm::vec3{0.0f, 0.0f, 1.0f}};
    case rendering::mesh::MeshImagePlaneOrientation::Sagittal:
      return {glm::vec3{0.0f, 1.0f, 0.0f}, glm::vec3{0.0f, 0.0f, 1.0f}};
  }

  return {glm::vec3{1.0f, 0.0f, 0.0f}, glm::vec3{0.0f, 1.0f, 0.0f}};
}

glm::vec3 textureSamplingDirectionForImageVoxelOffset(const Image& image, const glm::vec3& worldAxis)
{
  glm::vec3 pixelDirection = glm::mat3{image.transformations().pixel_T_worldDef()} * worldAxis;
  const float directionLength = glm::length(pixelDirection);
  if (!std::isfinite(directionLength) || directionLength <= 1.0e-6f) {
    return glm::vec3{0.0f};
  }

  pixelDirection /= directionLength;
  return glm::dot(glm::abs(pixelDirection), image.transformations().invPixelDimensions()) * pixelDirection;
}

std::vector<glm::vec3> computeMeshImagePlaneSegmentationVoxelSamplingDirs(
  const Image& geometryImage,
  const rendering::mesh::MeshImagePlaneOrientation orientation)
{
  std::vector<glm::vec3> samplingDirs{glm::vec3{0.0f}, glm::vec3{0.0f}};
  const std::array<glm::vec3, 2> worldAxes = imagePlaneWorldAxes(orientation);
  for (int i = 0; i < 2; ++i) {
    samplingDirs[i] = textureSamplingDirectionForImageVoxelOffset(geometryImage, worldAxes[i]);
  }

  return samplingDirs;
}

void setMeshImagePlaneSegmentationUniforms(
  GLShaderProgram& program,
  AppData& appData,
  const rendering::mesh::MeshImagePlaneRenderable& renderable,
  const RenderData::ImageUniforms& uniforms,
  const bool segmentationVisible)
{
  const Image* image = appData.image(renderable.texture.imageUid);
  const Image* segmentation =
    renderable.texture.segmentationUid ? appData.seg(*renderable.texture.segmentationUid) : nullptr;
  const RenderData& renderData = appData.renderData();
  const bool drawSegmentation = segmentationVisible && image && segmentation && uniforms.segOpacity > 0.0f;

  program.setUniform("u_segVisible", drawSegmentation);
  program.setSamplerUniform("u_segTex", sk_segTexSampler.index);
  program.setSamplerUniform("u_segLabelCmapTex", sk_segLabelTableTexSampler.index);
  program.setUniform(
    "u_segOpacity",
    drawSegmentation
      ? uniforms.segOpacity * (renderData.m_modulateSegOpacityWithImageOpacity ? uniforms.imgOpacity : 1.0f)
      : 0.0f);
  program.setUniform(
    "u_segFillOpacity",
    (SegmentationOutlineStyle::Disabled == renderData.m_segOutlineStyle) ? 1.0f : renderData.m_segInteriorOpacity);
  program.setUniform("u_segInterpCutoff", renderData.m_segInterpCutoff);
  program.setUniform(
    "u_segLinearInterpolation",
    drawSegmentation && InterpolationMode::NearestNeighbor != segmentation->settings().interpolationMode());
  program.setUniform(
    "u_segOutlineUsesScreenPixels",
    drawSegmentation && SegmentationOutlineStyle::ViewPixel == renderData.m_segOutlineStyle);

  const std::vector<glm::vec3> voxelSamplingDirs =
    image ? computeMeshImagePlaneSegmentationVoxelSamplingDirs(*image, renderable.orientation)
          : std::vector<glm::vec3>{glm::vec3{0.0f}, glm::vec3{0.0f}};
  const bool useImageVoxelOutline = SegmentationOutlineStyle::ImageVoxel == renderData.m_segOutlineStyle;
  const std::vector<glm::vec3> outlineSamplingDirs =
    useImageVoxelOutline ? voxelSamplingDirs : std::vector<glm::vec3>{glm::vec3{0.0f}, glm::vec3{0.0f}};
  program.setUniform("u_texSamplingDirsForSegOutline", outlineSamplingDirs);
  program.setUniform("u_texSamplingDirsForSmoothSeg", voxelSamplingDirs);
}

void setMeshImagePlaneUniforms(
  GLShaderProgram& program,
  const View& view,
  const rendering::mesh::MeshImagePlaneRenderable& renderable,
  const RenderData& renderData,
  const RenderData::ImageUniforms& uniforms,
  const RenderData::PlanarTextureLayout& textureLayout,
  const rendering::mesh::MeshDrawContext& context,
  const bool hasVertexNormals,
  const int checkerboardSquares)
{
  program.setSamplerUniform("u_imgTex", sk_imgTexSampler.index);
  program.setSamplerUniform("u_cmapTex", sk_imgCmapTexSampler.index);
  rendering::setTexture2DAxesUniforms(program, textureLayout);

  program.setUniform("u_clip_T_world", context.clip_T_world);
  program.setUniform("u_world_T_mesh", renderable.world_T_mesh);
  program.setUniform("u_world_T_meshNormal", glm::inverseTranspose(glm::mat3{renderable.world_T_mesh}));
  program.setUniform("u_hasVertexNormals", hasVertexNormals);
  program.setUniform("u_imagePlaneShadingEnabled", renderable.shadingEnabled);
  program.setUniform("u_cameraWorldPosition", context.cameraWorldPosition);
  program.setUniform("u_lightingAmbient", renderData.m_imagePlaneLightingAmbient);
  program.setUniform("u_lightingDiffuse", renderData.m_imagePlaneLightingDiffuse);
  program.setUniform("u_lightingSpecular", renderData.m_imagePlaneLightingSpecular);
  program.setUniform("u_lightingSpecularPower", renderData.m_imagePlaneLightingSpecularPower);
  program.setUniform("u_aspectRatio", view.camera().aspectRatio());
  program.setUniform("u_numCheckers", checkerboardSquares);

  program.setUniform("u_imgSlopeIntercept", uniforms.slopeIntercept_normalized_T_texture);
  program.setUniform("u_applyHsvMod", false);
  program.setUniform("u_cmapHsvModFactors", uniforms.hsvModFactors);
  program.setUniform("u_cmapSlopeIntercept", uniforms.cmapSlopeIntercept);
  program.setUniform("u_cmapQuantLevels", uniforms.cmapQuantLevels);
  program.setUniform("u_imgThresholds", uniforms.thresholds);
  program.setUniform("u_imgMinMax", uniforms.minMax);
  program.setUniform("u_imgOpacity", uniforms.imgOpacity * renderable.opacityMultiplier);
  program.setUniform("u_imagePlaneBorderColor", renderable.borderColor);
  program.setUniform("u_imagePlaneBorderWidthPixels", renderable.borderWidthPixels);
  program.setUniform("u_ddpDepthBias", renderable.ddpDepthBias);
  program.setUniform("u_boundaryVertexCount", static_cast<int>(renderable.boundaryVertexCount));
  program.setUniform(
    "u_boundaryWorldPositions",
    std::vector<glm::vec3>{
      renderable.boundaryWorld.begin(),
      renderable.boundaryWorld.begin() + renderable.boundaryVertexCount});
  std::array<GLint, 4> viewport{};
  glGetIntegerv(GL_VIEWPORT, viewport.data());
  program.setUniform("u_viewportSize", glm::vec2{viewport[2], viewport[3]});

  // 3D image planes use the image shader's ordinary layer path. Comparison modes, flashlight masking, and intensity
  // projection are 2D-view concepts and remain disabled for this mesh pass.
  program.setUniform("u_renderMode", 0);
  program.setUniform("u_clipCrosshairs", glm::vec2{0.0f});
  program.setUniform("u_quadrants", glm::ivec2{0, 0});
  program.setUniform("u_showFix", true);
  program.setUniform("u_flashlightRadius", 0.0f);
  program.setUniform("u_flashlightMovingOnFixed", false);
  program.setUniform("u_mipMode", 0);
  program.setUniform("u_halfNumMipSamples", 0);
  program.setUniform("u_texSamplingDirZ", glm::vec3{0.0f});
  program.setUniform("u_worldSamplingDirZ", glm::vec3{0.0f});
}

void setMeshImagePlaneIsoContourUniforms(
  GLShaderProgram& program,
  const View& view,
  const rendering::mesh::MeshImagePlaneRenderable& renderable,
  const RenderData::ImageUniforms& uniforms,
  const RenderData::PlanarTextureLayout& textureLayout,
  const rendering::mesh::MeshDrawContext& context,
  const int checkerboardSquares,
  const ImageSettings& imageSettings,
  const Isosurface& surface,
  const glm::vec3& color,
  const float imagePlaneOpacityMultiplier)
{
  const float isosurfaceOpacity = imageSettings.isosurfaceOpacityModulator() * imagePlaneOpacityMultiplier;

  program.setSamplerUniform("u_imgTex", sk_imgTexSampler.index);
  rendering::setTexture2DAxesUniforms(program, textureLayout);

  program.setUniform("u_clip_T_world", context.clip_T_world);
  program.setUniform("u_world_T_mesh", renderable.world_T_mesh);
  program.setUniform("u_aspectRatio", view.camera().aspectRatio());
  program.setUniform("u_numCheckers", checkerboardSquares);

  program.setUniform("u_isoValue", static_cast<float>(imageSettings.mapNativeIntensityToTexture(surface.value)));
  program.setUniform("u_fillOpacity", static_cast<float>(isosurfaceOpacity * surface.fillOpacity));
  program.setUniform("u_lineOpacity", static_cast<float>(isosurfaceOpacity * surface.opacity));
  program.setUniform("u_contourWidth", static_cast<float>(imageSettings.isoContourLineWidthIn2D()));
  program.setUniform("u_color", color);
  program.setUniform("u_imgMinMax", uniforms.minMax);
  program.setUniform("u_imgThresholds", uniforms.thresholds);

  // Mesh image planes are ordinary 3D slice overlays. The comparison and intensity-projection controls are specific
  // to 2D image views, so they stay disabled here.
  program.setUniform("u_renderMode", 0);
  program.setUniform("u_clipCrosshairs", glm::vec2{0.0f});
  program.setUniform("u_quadrants", glm::ivec2{0, 0});
  program.setUniform("u_showFix", true);
  program.setUniform("u_flashlightRadius", 0.0f);
  program.setUniform("u_flashlightMovingOnFixed", false);
  program.setUniform("u_mipMode", 0);
  program.setUniform("u_halfNumMipSamples", 0);
  program.setUniform("u_texSamplingDirZ", glm::vec3{0.0f});
  program.setUniform("u_worldSamplingDirZ", glm::vec3{0.0f});
}

void drawUploadedImagePlane(const rendering::mesh::MeshGpuData& gpuData)
{
  gpuData.vao().bind();
  gpuData.vao().drawElements(gpuData.drawParams());
  gpuData.vao().release();
}

void drawImagePlaneRenderablesWithProgram(
  AppData& appData,
  const View& view,
  const rendering::mesh::MeshImagePlaneRenderList& list,
  const rendering::mesh::MeshDrawContext& context,
  GLShaderProgram& texture3dProgram,
  GLShaderProgram& texture2dProgram,
  GLShaderProgram* const previousTexturesProgram = nullptr,
  GLTexture* const previousDepthBounds = nullptr,
  GLTexture* const previousFrontColor = nullptr)
{
  if (!context.meshLookup) {
    return;
  }

  if (previousTexturesProgram && previousDepthBounds && previousFrontColor) {
    previousDepthBounds->bind(sk_previousDepthBoundsSampler.index);
    previousFrontColor->bind(sk_previousFrontColorSampler.index);
  }

  for (const std::reference_wrapper<const rendering::mesh::MeshImagePlaneRenderable> imagePlaneRef : list.imagePlanes) {
    const rendering::mesh::MeshImagePlaneRenderable& imagePlane = imagePlaneRef.get();
    const rendering::mesh::MeshGpuData* gpuData = context.meshLookup(imagePlane.mesh);
    if (!gpuData || !gpuData->hasTextureCoords()) {
      continue;
    }

    const Image* image = appData.image(imagePlane.texture.imageUid);
    if (!image) {
      continue;
    }

    const auto uniformsIt = appData.renderData().m_uniforms.find(imagePlane.texture.imageUid);
    if (uniformsIt == std::end(appData.renderData().m_uniforms)) {
      continue;
    }

    const RenderData::PlanarTextureLayout textureLayout =
      rendering::textureLayoutOrDefault(appData.renderData().m_imageTextureLayouts, imagePlane.texture.imageUid);
    GLShaderProgram& program =
      shaderProgramForImagePlaneTextureDimension(texture3dProgram, texture2dProgram, textureLayout.dimension);
    const auto boundTextures =
      bindImagePlaneTextures(appData, imagePlane.texture.imageUid, imagePlane.texture.component, textureLayout);
    const auto boundSegTexture =
      bindImagePlaneSegmentationTexture(appData, imagePlane.texture.segmentationUid, textureLayout.dimension);
    const auto boundSegBufferTextures =
      bindImagePlaneSegmentationLabelTableTextures(appData, imagePlane.texture.segmentationUid);

    program.use();
    setMeshImagePlaneUniforms(
      program,
      view,
      imagePlane,
      appData.renderData(),
      uniformsIt->second,
      textureLayout,
      context,
      gpuData->hasNormals(),
      appData.renderData().m_numCheckerboardSquares);
    setMeshImagePlaneSegmentationUniforms(
      program,
      appData,
      imagePlane,
      uniformsIt->second,
      boundSegTexture.hasSegmentation && !boundSegBufferTextures.empty());
    if (previousTexturesProgram) {
      program.setSamplerUniform("u_previousDepthBoundsTex", sk_previousDepthBoundsSampler.index);
      program.setSamplerUniform("u_previousFrontColorTex", sk_previousFrontColorSampler.index);
    }
    drawUploadedImagePlane(*gpuData);
    program.stopUse();

    for (std::reference_wrapper<GLTexture> texture : boundTextures) {
      texture.get().unbind();
    }
    boundSegTexture.texture.get().unbind();
    unbindImagePlaneSegmentationLabelTableTextures(boundSegBufferTextures);
  }

  if (previousTexturesProgram && previousDepthBounds && previousFrontColor) {
    previousFrontColor->unbind(sk_previousFrontColorSampler.index);
    previousDepthBounds->unbind(sk_previousDepthBoundsSampler.index);
  }
}

std::vector<std::reference_wrapper<const rendering::mesh::MeshImagePlaneRenderable>> sortedImagePlanesBackToFront(
  const rendering::mesh::MeshImagePlaneRenderList& list,
  const rendering::mesh::MeshDrawContext& context)
{
  std::vector<std::reference_wrapper<const rendering::mesh::MeshImagePlaneRenderable>> imagePlanes = list.imagePlanes;
  std::ranges::stable_sort(imagePlanes, [&context](const auto& lhsRef, const auto& rhsRef) {
    const auto& lhs = lhsRef.get();
    const auto& rhs = rhsRef.get();
    const float lhsDepth = glm::dot(lhs.centerWorld - context.cameraWorldPosition, context.cameraFrontWorld);
    const float rhsDepth = glm::dot(rhs.centerWorld - context.cameraWorldPosition, context.cameraFrontWorld);
    return lhsDepth > rhsDepth;
  });
  return imagePlanes;
}

} // namespace

void Rendering::drawMeshImagePlaneRenderListForView(
  const View& view,
  const rendering::mesh::MeshImagePlaneRenderList& list)
{
  if (list.imagePlanes.empty()) {
    return;
  }

  const rendering::mesh::ScopedMeshViewViewport scopedViewport{view, m_appData.windowData()};
  const rendering::mesh::MeshDrawContext context = rendering::mesh::meshDrawContextForView(m_meshGpuStore, view);
  if (!context.meshLookup) {
    return;
  }

  const ScopedImagePlaneDepthState scopedDepthState;
  const ScopedImagePlaneBlendState scopedBlendState;
  glEnable(GL_DEPTH_TEST);
  glDepthFunc(GL_LESS);
  glDepthMask(GL_TRUE);
  glDisable(GL_STENCIL_TEST);
  glEnable(GL_BLEND);
  glBlendEquation(GL_FUNC_ADD);
  glBlendFuncSeparate(GL_ONE, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

  const auto sortedImagePlanes = sortedImagePlanesBackToFront(list, context);
  for (const std::reference_wrapper<const rendering::mesh::MeshImagePlaneRenderable> imagePlaneRef : sortedImagePlanes)
  {
    const rendering::mesh::MeshImagePlaneRenderable& imagePlane = imagePlaneRef.get();
    const rendering::mesh::MeshGpuData* gpuData = context.meshLookup(imagePlane.mesh);
    if (!gpuData || !gpuData->hasTextureCoords()) {
      continue;
    }

    const Image* image = m_appData.image(imagePlane.texture.imageUid);
    if (!image) {
      continue;
    }

    const auto uniformsIt = m_appData.renderData().m_uniforms.find(imagePlane.texture.imageUid);
    if (uniformsIt == std::end(m_appData.renderData().m_uniforms)) {
      continue;
    }

    const RenderData::PlanarTextureLayout textureLayout =
      rendering::textureLayoutOrDefault(m_appData.renderData().m_imageTextureLayouts, imagePlane.texture.imageUid);
    GLShaderProgram& program = shaderProgramForImagePlaneTextureDimension(
      m_meshImagePlaneGrayLinearProgram,
      m_meshImagePlaneGrayLinearTexture2DProgram,
      textureLayout.dimension);

    const auto boundTextures =
      bindImagePlaneTextures(m_appData, imagePlane.texture.imageUid, imagePlane.texture.component, textureLayout);

    program.use();
    {
      setMeshImagePlaneUniforms(
        program,
        view,
        imagePlane,
        m_appData.renderData(),
        uniformsIt->second,
        textureLayout,
        context,
        gpuData->hasNormals(),
        m_appData.renderData().m_numCheckerboardSquares);
      drawUploadedImagePlane(*gpuData);
    }
    program.stopUse();

    const ImageSettings& imageSettings = image->settings();
    if (imageSettings.isosurfacesVisible() && imageSettings.showIsocontoursIn2D()) {
      GLShaderProgram& isoProgram = shaderProgramForImagePlaneTextureDimension(
        m_meshImagePlaneIsoContourProgram,
        m_meshImagePlaneIsoContourTexture2DProgram,
        textureLayout.dimension);

      isoProgram.use();
      for (const auto& surfaceUid : m_appData.isosurfaceUids(imagePlane.texture.imageUid, imagePlane.texture.component))
      {
        const Isosurface* surface =
          m_appData.isosurface(imagePlane.texture.imageUid, imagePlane.texture.component, surfaceUid);
        if (!surface) {
          spdlog::warn("Null isosurface {} for image {}", surfaceUid, imagePlane.texture.imageUid);
          continue;
        }
        if (!surface->visible || !surface->showIn2d) {
          continue;
        }

        static constexpr bool premultipliedAlpha = false;
        const glm::vec3 color = glm::vec3{
          getIsosurfaceColor(m_appData, *surface, imageSettings, imagePlane.texture.component, premultipliedAlpha)};
        setMeshImagePlaneIsoContourUniforms(
          isoProgram,
          view,
          imagePlane,
          uniformsIt->second,
          textureLayout,
          context,
          m_appData.renderData().m_numCheckerboardSquares,
          imageSettings,
          *surface,
          color,
          imagePlane.opacityMultiplier);
        drawUploadedImagePlane(*gpuData);
      }
      isoProgram.stopUse();
    }

    unbindTextures(boundTextures);
  }

  setupOpenGLState();
}

void Rendering::drawMeshImagePlaneDdpDepthBoundsForView(
  const View& view,
  const rendering::mesh::MeshImagePlaneRenderList& list,
  const rendering::mesh::MeshDrawContext& context)
{
  drawImagePlaneRenderablesWithProgram(
    m_appData,
    view,
    list,
    context,
    m_meshImagePlaneDdpInitProgram,
    m_meshImagePlaneDdpInitTexture2DProgram);
}

void Rendering::drawMeshImagePlaneDdpPeelLayersForView(
  const View& view,
  const rendering::mesh::MeshImagePlaneRenderList& list,
  const rendering::mesh::MeshDrawContext& context,
  GLTexture& previousDepthBounds,
  GLTexture& previousFrontColor)
{
  drawImagePlaneRenderablesWithProgram(
    m_appData,
    view,
    list,
    context,
    m_meshImagePlaneDdpPeelProgram,
    m_meshImagePlaneDdpPeelTexture2DProgram,
    &m_meshImagePlaneDdpPeelProgram,
    &previousDepthBounds,
    &previousFrontColor);
}
