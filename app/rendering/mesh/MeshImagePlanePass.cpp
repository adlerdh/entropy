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
#include "rendering/gl/GLShaderProgram.h"
#include "rendering/gl/GLBufferTexture.h"
#include "rendering/gl/GLTexture.h"
#include "rendering/gl/OpenGLStateGuard.h"
#include "rendering/gl/Uniforms.h"
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
const Uniforms::SamplerIndexVectorType sk_imgRgbaTexSamplers{{0, 1, 2, 3}};
constexpr Uniforms::SamplerIndexType sk_imgCmapTexSampler{4};
constexpr Uniforms::SamplerIndexType sk_segTexSampler{5};
constexpr Uniforms::SamplerIndexType sk_segLabelTableTexSampler{6};
constexpr Uniforms::SamplerIndexType sk_previousDepthBoundsSampler{7};
constexpr Uniforms::SamplerIndexType sk_previousFrontColorSampler{8};
constexpr Uniforms::SamplerIndexType sk_compositeColorSampler{9};
constexpr Uniforms::SamplerIndexType sk_compositeDepthSampler{10};

constexpr std::array<rendering::mesh::MeshImagePlaneOrientation, 3> sk_imagePlaneOrientations{
  rendering::mesh::MeshImagePlaneOrientation::Axial,
  rendering::mesh::MeshImagePlaneOrientation::Coronal,
  rendering::mesh::MeshImagePlaneOrientation::Sagittal};

GLShaderProgram& shaderProgramForImagePlaneTextureDimension(
  // cppcheck-suppress constParameterReference -- returns the selected program as a mutable reference
  GLShaderProgram& texture3dProgram,
  // cppcheck-suppress constParameterReference -- returns the selected program as a mutable reference
  GLShaderProgram& texture2dProgram,
  const rendering::TextureDimension textureDimension)
{
  return rendering::TextureDimension::Texture2D == textureDimension ? texture2dProgram : texture3dProgram;
}

struct BoundImagePlaneTexture
{
  std::reference_wrapper<GLTexture> texture;
  uint32_t unit;
};

std::list<BoundImagePlaneTexture> bindDdpImagePlaneTextures(
  AppData& appData,
  const uuids::uuid& sourceImageUid,
  const uuids::uuid& textureImageUid,
  const uint32_t component,
  const bool bindMultipleComponents,
  const rendering::PlanarTextureLayout& textureLayout)
{
  auto& renderSettings = appData.renderResources();
  const Image* sourceImage = appData.image(sourceImageUid);
  const Image* textureImage = appData.image(textureImageUid);
  std::list<BoundImagePlaneTexture> boundTextures;
  GLTexture& blankTexture = rendering::TextureDimension::Texture2D == textureLayout.dimension
                              ? renderSettings.m_blankImageBlackTransparentTexture2D
                              : renderSettings.m_blankImageBlackTransparentTexture;
  const auto textureIt = renderSettings.m_imageTextures.find(textureImageUid);

  for (std::size_t slot = 0; slot < sk_imgRgbaTexSamplers.indices.size(); ++slot) {
    GLTexture* texture = &blankTexture;
    if (textureImage && textureIt != renderSettings.m_imageTextures.end() && !textureIt->second.empty()) {
      const std::size_t requestedComponent = bindMultipleComponents ? slot : component;
      const bool componentExists =
        !bindMultipleComponents || requestedComponent < textureImage->header().numComponentsPerPixel();
      if (componentExists) {
        const std::size_t textureIndex =
          Image::MultiComponentBufferType::InterleavedImage == textureImage->bufferType() &&
              textureIt->second.size() == 1u
            ? 0u
            : std::min(requestedComponent, textureIt->second.size() - 1u);
        texture = &textureIt->second.at(textureIndex);
      }
    }
    texture->bind(sk_imgRgbaTexSamplers.indices[slot]);
    boundTextures.push_back({*texture, static_cast<uint32_t>(sk_imgRgbaTexSamplers.indices[slot])});
  }

  const std::optional<uuids::uuid> cmapUid =
    sourceImage ? appData.imageColorMapUid(sourceImage->settings().colorMapIndex()) : std::nullopt;
  GLTexture& colorMapTexture =
    cmapUid ? renderSettings.m_colormapTextures.at(*cmapUid) : std::begin(renderSettings.m_colormapTextures)->second;
  colorMapTexture.bind(sk_imgCmapTexSampler.index);
  boundTextures.emplace_back(colorMapTexture, sk_imgCmapTexSampler.index);
  return boundTextures;
}

struct BoundImagePlaneSegmentationTexture
{
  std::reference_wrapper<GLTexture> texture;
  uint32_t unit;
  bool hasSegmentation = false;
};

BoundImagePlaneSegmentationTexture bindImagePlaneSegmentationTexture(
  AppData& appData,
  const std::optional<uuids::uuid>& segmentationUid,
  rendering::TextureDimension textureDimension)
{
  auto& renderSettings = appData.renderResources();
  GLTexture* texture = textureDimension == rendering::TextureDimension::Texture2D ? &renderSettings.m_blankSegTexture2D
                                                                                  : &renderSettings.m_blankSegTexture;
  bool hasSegmentation = false;

  if (segmentationUid) {
    const auto textureIt = renderSettings.m_segTextures.find(*segmentationUid);
    if (std::end(renderSettings.m_segTextures) != textureIt) {
      texture = &textureIt->second;
      hasSegmentation = true;
    }
  }

  texture->bind(sk_segTexSampler.index);
  return {*texture, sk_segTexSampler.index, hasSegmentation};
}

struct BoundImagePlaneBufferTexture
{
  std::reference_wrapper<GLBufferTexture> texture;
  uint32_t unit;
};

std::list<BoundImagePlaneBufferTexture> bindImagePlaneSegmentationLabelTableTextures(
  AppData& appData,
  const std::optional<uuids::uuid>& segmentationUid)
{
  std::list<BoundImagePlaneBufferTexture> boundTextures;
  if (appData.renderResources().m_labelBufferTextures.empty()) {
    return boundTextures;
  }

  const Image* segmentation = segmentationUid ? appData.seg(*segmentationUid) : nullptr;
  const std::optional<uuids::uuid> tableUid =
    segmentation ? appData.labelTableUid(segmentation->settings().labelTableIndex()) : std::nullopt;
  auto tableIt = tableUid ? appData.renderResources().m_labelBufferTextures.find(*tableUid)
                          : appData.renderResources().m_labelBufferTextures.end();
  if (std::end(appData.renderResources().m_labelBufferTextures) == tableIt) {
    tableIt = std::begin(appData.renderResources().m_labelBufferTextures);
  }

  tableIt->second.bind(sk_segLabelTableTexSampler.index);
  boundTextures.push_back({tableIt->second, static_cast<uint32_t>(sk_segLabelTableTexSampler.index)});
  return boundTextures;
}

void unbindImagePlaneSegmentationLabelTableTextures(const std::list<BoundImagePlaneBufferTexture>& textures)
{
  for (const BoundImagePlaneBufferTexture& binding : textures) {
    binding.texture.get().unbind(binding.unit);
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

bool imagePlaneUsesMultipleComponents(const ComponentRenderMode mode) noexcept
{
  return ComponentRenderMode::Color == mode || ComponentRenderMode::VectorDirectionColor == mode ||
         ComponentRenderMode::VectorSignedNormalProjection == mode ||
         ComponentRenderMode::VectorPlanarProjectionColor == mode;
}

int imagePlaneShaderDisplayMode(const ComponentRenderMode mode) noexcept
{
  switch (mode) {
    case ComponentRenderMode::Color:
      return 1;
    case ComponentRenderMode::VectorDirectionColor:
      return 2;
    case ComponentRenderMode::VectorSignedNormalProjection:
      return 3;
    case ComponentRenderMode::VectorPlanarProjectionColor:
      return 4;
    default:
      return 0;
  }
}

float maxAbsVectorComponentValue(const ImageSettings& settings)
{
  float maxAbs = 1.0f;
  for (uint32_t component = 0; component < std::min<uint32_t>(3u, settings.numComponents()); ++component) {
    const auto [minValue, maxValue] = settings.minMaxImageRange(component);
    maxAbs = std::max(maxAbs, static_cast<float>(std::max(std::abs(minValue), std::abs(maxValue))));
  }
  return maxAbs;
}

struct ImagePlaneSubjectDirections
{
  glm::vec3 normal;
  glm::vec3 right;
  glm::vec3 up;
};

ImagePlaneSubjectDirections imagePlaneSubjectDirections(
  const Image& image,
  const rendering::mesh::MeshImagePlaneOrientation orientation,
  const ViewConvention viewConvention)
{
  glm::vec3 normalWorld{0.0f, 0.0f, -1.0f};
  glm::vec3 rightWorld{1.0f, 0.0f, 0.0f};
  glm::vec3 upWorld{0.0f, -1.0f, 0.0f};
  const bool neurological = ViewConvention::Neurological == viewConvention;
  switch (orientation) {
    case rendering::mesh::MeshImagePlaneOrientation::Axial:
      normalWorld = neurological ? glm::vec3{0.0f, 0.0f, 1.0f} : glm::vec3{0.0f, 0.0f, -1.0f};
      rightWorld = neurological ? glm::vec3{-1.0f, 0.0f, 0.0f} : glm::vec3{1.0f, 0.0f, 0.0f};
      upWorld = {0.0f, -1.0f, 0.0f};
      break;
    case rendering::mesh::MeshImagePlaneOrientation::Coronal:
      normalWorld = neurological ? glm::vec3{0.0f, 1.0f, 0.0f} : glm::vec3{0.0f, -1.0f, 0.0f};
      rightWorld = neurological ? glm::vec3{-1.0f, 0.0f, 0.0f} : glm::vec3{1.0f, 0.0f, 0.0f};
      upWorld = {0.0f, 0.0f, 1.0f};
      break;
    case rendering::mesh::MeshImagePlaneOrientation::Sagittal:
      normalWorld = {1.0f, 0.0f, 0.0f};
      rightWorld = {0.0f, 1.0f, 0.0f};
      upWorld = {0.0f, 0.0f, 1.0f};
      break;
  }

  const glm::mat3 subject_T_world{image.transformations().subject_T_worldDef()};
  return {
    glm::normalize(subject_T_world * normalWorld),
    glm::normalize(subject_T_world * rightWorld),
    glm::normalize(subject_T_world * upWorld)};
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
  const rendering::RenderDerivedData::ImageUniforms& uniforms,
  const bool segmentationVisible)
{
  const Image* image = appData.image(renderable.texture.imageUid);
  const Image* segmentation =
    renderable.texture.segmentationUid ? appData.seg(*renderable.texture.segmentationUid) : nullptr;
  const rendering::RenderSettings& renderSettings = appData.renderSettings();
  const bool drawSegmentation = segmentationVisible && image && segmentation && uniforms.segOpacity > 0.0f;

  program.setUniform("u_segVisible", drawSegmentation);
  program.setSamplerUniform("u_segTex", sk_segTexSampler.index);
  program.setSamplerUniform("u_segLabelCmapTex", sk_segLabelTableTexSampler.index);
  program.setUniform(
    "u_segOpacity",
    drawSegmentation ? uniforms.segOpacity *
                         (renderSettings.m_modulateSegmentationOpacityWithImageOpacity2d ? uniforms.imgOpacity : 1.0f)
                     : 0.0f);
  program.setUniform(
    "u_segFillOpacity",
    (SegmentationOutlineStyle::Disabled == renderSettings.m_segOutlineStyle) ? 1.0f
                                                                             : renderSettings.m_segInteriorOpacity);
  program.setUniform("u_segInterpCutoff", renderSettings.m_segInterpCutoff);
  program.setUniform(
    "u_segLinearInterpolation",
    drawSegmentation && InterpolationMode::NearestNeighbor != segmentation->settings().interpolationMode());
  program.setUniform(
    "u_segOutlineUsesScreenPixels",
    drawSegmentation && SegmentationOutlineStyle::ViewPixel == renderSettings.m_segOutlineStyle);

  const std::vector<glm::vec3> voxelSamplingDirs =
    image ? computeMeshImagePlaneSegmentationVoxelSamplingDirs(*image, renderable.orientation)
          : std::vector<glm::vec3>{glm::vec3{0.0f}, glm::vec3{0.0f}};
  const bool useImageVoxelOutline = SegmentationOutlineStyle::ImageVoxel == renderSettings.m_segOutlineStyle;
  const std::vector<glm::vec3> outlineSamplingDirs =
    useImageVoxelOutline ? voxelSamplingDirs : std::vector<glm::vec3>{glm::vec3{0.0f}, glm::vec3{0.0f}};
  program.setUniform("u_texSamplingDirsForSegOutline", outlineSamplingDirs);
  program.setUniform("u_texSamplingDirsForSmoothSeg", voxelSamplingDirs);
}

void setMeshImagePlaneUniforms(
  GLShaderProgram& program,
  const View& view,
  const rendering::mesh::MeshImagePlaneRenderable& renderable,
  const rendering::RenderSettings& renderSettings,
  const rendering::RenderDerivedData::ImageUniforms& uniforms,
  const rendering::PlanarTextureLayout& textureLayout,
  const Image& sourceImage,
  const ViewConvention viewConvention,
  const bool matchComponentRenderMode,
  const rendering::mesh::MeshDrawContext& context,
  const bool hasVertexNormals,
  const int checkerboardSquares)
{
  if (matchComponentRenderMode) {
    program.setSamplerUniform("u_imgTex", sk_imgTexSampler.index);
    program.setSamplerUniform("u_imgRgbaTex", sk_imgRgbaTexSamplers);
  }
  else {
    program.setSamplerUniform("u_imgTex", sk_imgTexSampler.index);
  }
  program.setSamplerUniform("u_cmapTex", sk_imgCmapTexSampler.index);
  rendering::setTexture2DAxesUniforms(program, textureLayout);
  if (matchComponentRenderMode) {
    program.setUniform("u_tex2DAxes[2]", rendering::textureAxesForProgramSlot(textureLayout));
    program.setUniform("u_tex2DAxes[3]", rendering::textureAxesForProgramSlot(textureLayout));
  }

  program.setUniform("u_clip_T_world", context.clip_T_world);
  program.setUniform("u_world_T_mesh", renderable.world_T_mesh);
  program.setUniform("u_world_T_meshNormal", glm::inverseTranspose(glm::mat3{renderable.world_T_mesh}));
  program.setUniform("u_hasVertexNormals", hasVertexNormals);
  program.setUniform("u_imagePlaneShadingEnabled", renderable.shadingEnabled);
  program.setUniform("u_cameraWorldPosition", context.cameraWorldPosition);
  program.setUniform("u_lightingAmbient", renderSettings.m_imagePlaneLightingAmbient);
  program.setUniform("u_lightingDiffuse", renderSettings.m_imagePlaneLightingDiffuse);
  program.setUniform("u_lightingSpecular", renderSettings.m_imagePlaneLightingSpecular);
  program.setUniform("u_lightingSpecularPower", renderSettings.m_imagePlaneLightingSpecularPower);
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
  if (matchComponentRenderMode) {
    program.setUniform(
      "u_componentRenderMode",
      imagePlaneShaderDisplayMode(sourceImage.settings().componentRenderMode()));
    program.setUniform("u_imgSlopeInterceptRgba", uniforms.slopeInterceptRgba_normalized_T_texture);
    program.setUniform("u_imgThresholdsRgba", uniforms.thresholdsRgba);
    program.setUniform("u_imgMinMaxRgba", uniforms.minMaxRgba);
    std::vector<float> rgbaOpacity = uniforms.imgOpacityRgba;
    std::transform(rgbaOpacity.begin(), rgbaOpacity.end(), rgbaOpacity.begin(), [&renderable](const float opacity) {
      return opacity * renderable.opacityMultiplier;
    });
    program.setUniform("u_imgOpacityRgba", rgbaOpacity);
    program.setUniform(
      "u_alphaIsOne",
      sourceImage.settings().ignoreAlpha() || 3 == sourceImage.header().numComponentsPerPixel());
    program.setUniform("u_imgSlope_native_T_texture", uniforms.slope_native_T_texture);
    program.setUniform("u_projectionScale", maxAbsVectorComponentValue(sourceImage.settings()));
    const ImagePlaneSubjectDirections directions =
      imagePlaneSubjectDirections(sourceImage, renderable.orientation, viewConvention);
    program.setUniform("u_planeNormal_subject", directions.normal);
    program.setUniform("u_planeRight_subject", directions.right);
    program.setUniform("u_planeUp_subject", directions.up);
    program.setUniform("u_vectorSignedColors", sourceImage.settings().vectorPlanarProjectionSignedColors());
  }
  program.setUniform("u_imagePlaneBorderColor", renderable.borderColor);
  program.setUniform("u_imagePlaneBorderWidthPixels", renderable.borderWidthPixels);
  // Direct image draws and stack composition preserve physical depth. The later DDP contribution applies its small
  // tie-break only between the three pre-composited plane orientations.
  program.setUniform("u_ddpDepthOrder", 0u);
  program.setUniform("u_boundaryVertexCount", static_cast<int>(renderable.boundaryVertexCount));
  program.setUniform(
    "u_boundaryWorldPositions",
    std::vector<glm::vec3>{
      renderable.boundaryWorld.begin(),
      renderable.boundaryWorld.begin() + renderable.boundaryVertexCount});
  std::array<GLint, 4> viewport{};
  glGetIntegerv(GL_VIEWPORT, viewport.data());
  program.setUniform("u_viewportOrigin", glm::vec2{viewport[0], viewport[1]});
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
  const rendering::RenderDerivedData::ImageUniforms& uniforms,
  const rendering::PlanarTextureLayout& textureLayout,
  const rendering::mesh::MeshDrawContext& context,
  const int checkerboardSquares,
  const ImageSettings& imageSettings,
  const Isosurface& surface,
  const glm::vec3& color,
  const float imagePlaneOpacityMultiplier)
{
  const float imageOpacity = imageSettings.modulateIsosurfaceOpacityWithImageOpacity() ? uniforms.imgOpacity : 1.0f;
  const float isosurfaceOpacity =
    imageSettings.isosurfaceOpacityModulator() * imageOpacity * imagePlaneOpacityMultiplier;

  program.setSamplerUniform("u_imgTex", sk_imgTexSampler.index);
  rendering::setTexture2DAxesUniforms(program, textureLayout);

  program.setUniform("u_clip_T_world", context.clip_T_world);
  program.setUniform("u_world_T_mesh", renderable.world_T_mesh);
  program.setUniform("u_aspectRatio", view.camera().aspectRatio());
  program.setUniform("u_numCheckers", checkerboardSquares);

  program.setUniform("u_isoValue", static_cast<float>(imageSettings.mapNativeIntensityToTexture(surface.value)));
  program.setUniform("u_fillOpacity", static_cast<float>(isosurfaceOpacity * surface.fillOpacity));
  program.setUniform("u_fillAboveIsovalue", surface.fillAboveIsovalue);
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
  gpuData.vao().unbind();
}

void drawImagePlaneRenderablesWithProgram(
  AppData& appData,
  const View& view,
  const rendering::mesh::MeshImagePlaneRenderList& list,
  const rendering::mesh::MeshDrawContext& context,
  GLShaderProgram& texture3dProgram,
  GLShaderProgram& texture2dProgram,
  const bool usePreviousTextures = false,
  GLTexture* const previousDepthBounds = nullptr,
  GLTexture* const previousFrontColor = nullptr)
{
  if (!context.meshLookup) {
    return;
  }

  if (usePreviousTextures && previousDepthBounds && previousFrontColor) {
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

    const ComponentRenderMode componentRenderMode = image->settings().componentRenderMode();
    const bool multipleComponents = imagePlaneUsesMultipleComponents(componentRenderMode);
    const uuids::uuid renderImageUid = multipleComponents
                                         ? imagePlane.texture.imageUid
                                         : appData.effectiveImageUidForRendering(imagePlane.texture.imageUid);
    const auto uniformsIt = appData.renderDerivedData().imageUniforms.find(renderImageUid);
    if (uniformsIt == std::end(appData.renderDerivedData().imageUniforms)) {
      continue;
    }

    const rendering::PlanarTextureLayout textureLayout =
      rendering::textureLayoutOrDefault(appData.renderResources().m_imageTextureLayouts, renderImageUid);
    GLShaderProgram& program =
      shaderProgramForImagePlaneTextureDimension(texture3dProgram, texture2dProgram, textureLayout.dimension);
    const auto boundTextures = bindDdpImagePlaneTextures(
      appData,
      imagePlane.texture.imageUid,
      renderImageUid,
      imagePlane.texture.component,
      multipleComponents,
      textureLayout);
    const auto boundSegTexture =
      bindImagePlaneSegmentationTexture(appData, imagePlane.texture.segmentationUid, textureLayout.dimension);
    const auto boundSegBufferTextures =
      bindImagePlaneSegmentationLabelTableTextures(appData, imagePlane.texture.segmentationUid);

    program.use();
    setMeshImagePlaneUniforms(
      program,
      view,
      imagePlane,
      appData.renderSettings(),
      uniformsIt->second,
      textureLayout,
      *image,
      appData.windowData().getViewOrientationConvention(),
      true,
      context,
      gpuData->hasNormals(),
      appData.renderSettings().m_numCheckerboardSquares);
    setMeshImagePlaneSegmentationUniforms(
      program,
      appData,
      imagePlane,
      uniformsIt->second,
      boundSegTexture.hasSegmentation && !boundSegBufferTextures.empty());
    if (usePreviousTextures) {
      program.setSamplerUniform("u_previousDepthBoundsTex", sk_previousDepthBoundsSampler.index);
      program.setSamplerUniform("u_previousFrontColorTex", sk_previousFrontColorSampler.index);
    }
    drawUploadedImagePlane(*gpuData);
    program.stopUse();

    for (const BoundImagePlaneTexture& binding : boundTextures) {
      binding.texture.get().unbind(binding.unit);
    }
    boundSegTexture.texture.get().unbind(boundSegTexture.unit);
    unbindImagePlaneSegmentationLabelTableTextures(boundSegBufferTextures);
  }

  if (usePreviousTextures && previousDepthBounds && previousFrontColor) {
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
  const OpenGLStateGuard state{
    {0u, GL_TEXTURE_2D},
    {0u, GL_TEXTURE_3D},
    {1u, GL_TEXTURE_2D},
    {1u, GL_TEXTURE_3D},
    {2u, GL_TEXTURE_2D},
    {2u, GL_TEXTURE_3D},
    {3u, GL_TEXTURE_2D},
    {3u, GL_TEXTURE_3D},
    {4u, GL_TEXTURE_1D},
    {5u, GL_TEXTURE_2D},
    {5u, GL_TEXTURE_3D},
    {6u, GL_TEXTURE_BUFFER}};
  const rendering::mesh::MeshDrawContext context =
    rendering::mesh::meshDrawContextForView(m_meshResources.gpuStore(), view);
  if (!context.meshLookup) {
    return;
  }

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

    const ComponentRenderMode componentRenderMode = image->settings().componentRenderMode();
    const bool multipleComponents = imagePlaneUsesMultipleComponents(componentRenderMode);
    const uuids::uuid renderImageUid = multipleComponents
                                         ? imagePlane.texture.imageUid
                                         : m_appData.effectiveImageUidForRendering(imagePlane.texture.imageUid);
    const auto uniformsIt = m_appData.renderDerivedData().imageUniforms.find(renderImageUid);
    if (uniformsIt == std::end(m_appData.renderDerivedData().imageUniforms)) {
      continue;
    }

    const rendering::PlanarTextureLayout textureLayout =
      rendering::textureLayoutOrDefault(m_appData.renderResources().m_imageTextureLayouts, renderImageUid);
    GLShaderProgram& program = shaderProgramForImagePlaneTextureDimension(
      m_meshImagePlaneGrayLinearProgram,
      m_meshImagePlaneGrayLinearTexture2DProgram,
      textureLayout.dimension);

    const auto boundTextures = bindDdpImagePlaneTextures(
      m_appData,
      imagePlane.texture.imageUid,
      renderImageUid,
      imagePlane.texture.component,
      multipleComponents,
      textureLayout);

    program.use();
    {
      setMeshImagePlaneUniforms(
        program,
        view,
        imagePlane,
        m_appData.renderSettings(),
        uniformsIt->second,
        textureLayout,
        *image,
        m_appData.windowData().getViewOrientationConvention(),
        false,
        context,
        gpuData->hasNormals(),
        m_appData.renderSettings().m_numCheckerboardSquares);
      drawUploadedImagePlane(*gpuData);
    }
    program.stopUse();

    const ImageSettings& imageSettings = image->settings();
    GLShaderProgram& isoProgram = shaderProgramForImagePlaneTextureDimension(
      m_meshImagePlaneIsoContourProgram,
      m_meshImagePlaneIsoContourTexture2DProgram,
      textureLayout.dimension);

    isoProgram.use();
    for (const auto& surfaceUid : m_appData.isosurfaceUids(imagePlane.texture.imageUid, imagePlane.texture.component)) {
      const Isosurface* surface =
        m_appData.isosurface(imagePlane.texture.imageUid, imagePlane.texture.component, surfaceUid);
      if (!surface) {
        spdlog::warn("Null isosurface {} for image {}", surfaceUid, imagePlane.texture.imageUid);
        continue;
      }
      if (!surface->visibleIn2d) {
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
        m_appData.renderSettings().m_numCheckerboardSquares,
        imageSettings,
        *surface,
        color,
        imagePlane.opacityMultiplier);
      drawUploadedImagePlane(*gpuData);
    }
    isoProgram.stopUse();

    for (const BoundImagePlaneTexture& binding : boundTextures) {
      binding.texture.get().unbind(binding.unit);
    }
  }
}

void Rendering::drawMeshImagePlaneDdpDepthBoundsForView(
  const View&,
  const rendering::mesh::MeshImagePlaneRenderList&,
  const rendering::mesh::MeshDrawContext&)
{
  for (std::size_t index = 0; index < sk_imagePlaneOrientations.size(); ++index) {
    GLTexture& color = m_meshDdpResources.imagePlaneCompositeColorTexture(index);
    GLTexture& depth = m_meshDdpResources.imagePlaneCompositeDepthTexture(index);
    color.bind(sk_compositeColorSampler.index);
    depth.bind(sk_compositeDepthSampler.index);
    m_meshImagePlaneCompositeDdpInitProgram.use();
    m_meshImagePlaneCompositeDdpInitProgram.setSamplerUniform(
      "u_compositeColorTex",
      static_cast<GLint>(sk_compositeColorSampler.index));
    m_meshImagePlaneCompositeDdpInitProgram.setSamplerUniform(
      "u_compositeDepthTex",
      static_cast<GLint>(sk_compositeDepthSampler.index));
    m_meshImagePlaneCompositeDdpInitProgram.setUniform(
      "u_ddpDepthOrder",
      rendering::mesh::imagePlaneCompositeDdpDepthOrder(sk_imagePlaneOrientations[index]));
    m_meshDdpResources.fullScreenVao().bind();
    m_meshDdpResources.fullScreenVao().drawArrays(PrimitiveMode::Triangles, 0, 3);
    m_meshDdpResources.fullScreenVao().unbind();
    m_meshImagePlaneCompositeDdpInitProgram.stopUse();
    depth.unbind(sk_compositeDepthSampler.index);
    color.unbind(sk_compositeColorSampler.index);
  }
}

void Rendering::drawMeshImagePlaneDdpPeelLayersForView(
  const View&,
  const rendering::mesh::MeshImagePlaneRenderList&,
  const rendering::mesh::MeshDrawContext&,
  GLTexture& previousDepthBounds,
  GLTexture& previousFrontColor)
{
  previousDepthBounds.bind(sk_previousDepthBoundsSampler.index);
  previousFrontColor.bind(sk_previousFrontColorSampler.index);
  for (std::size_t index = 0; index < sk_imagePlaneOrientations.size(); ++index) {
    GLTexture& color = m_meshDdpResources.imagePlaneCompositeColorTexture(index);
    GLTexture& depth = m_meshDdpResources.imagePlaneCompositeDepthTexture(index);
    color.bind(sk_compositeColorSampler.index);
    depth.bind(sk_compositeDepthSampler.index);
    m_meshImagePlaneCompositeDdpPeelProgram.use();
    m_meshImagePlaneCompositeDdpPeelProgram.setSamplerUniform(
      "u_compositeColorTex",
      static_cast<GLint>(sk_compositeColorSampler.index));
    m_meshImagePlaneCompositeDdpPeelProgram.setSamplerUniform(
      "u_compositeDepthTex",
      static_cast<GLint>(sk_compositeDepthSampler.index));
    m_meshImagePlaneCompositeDdpPeelProgram.setSamplerUniform(
      "u_previousDepthBoundsTex",
      static_cast<GLint>(sk_previousDepthBoundsSampler.index));
    m_meshImagePlaneCompositeDdpPeelProgram.setSamplerUniform(
      "u_previousFrontColorTex",
      static_cast<GLint>(sk_previousFrontColorSampler.index));
    m_meshImagePlaneCompositeDdpPeelProgram.setUniform(
      "u_ddpDepthOrder",
      rendering::mesh::imagePlaneCompositeDdpDepthOrder(sk_imagePlaneOrientations[index]));
    m_meshDdpResources.fullScreenVao().bind();
    m_meshDdpResources.fullScreenVao().drawArrays(PrimitiveMode::Triangles, 0, 3);
    m_meshDdpResources.fullScreenVao().unbind();
    m_meshImagePlaneCompositeDdpPeelProgram.stopUse();
    depth.unbind(sk_compositeDepthSampler.index);
    color.unbind(sk_compositeColorSampler.index);
  }
  previousFrontColor.unbind(sk_previousFrontColorSampler.index);
  previousDepthBounds.unbind(sk_previousDepthBoundsSampler.index);
}

void Rendering::prepareMeshImagePlaneDdpCompositesForView(
  const View& view,
  const rendering::mesh::MeshImagePlaneRenderList& list,
  const rendering::mesh::MeshDrawContext& context)
{
  for (std::size_t index = 0; index < sk_imagePlaneOrientations.size(); ++index) {
    const rendering::mesh::MeshImagePlaneRenderList orientationList =
      rendering::mesh::imagePlaneRenderListForOrientation(list, sk_imagePlaneOrientations[index]);
    m_meshDdpResources.bindImagePlaneCompositeTarget(index);
    glDrawBuffer(GL_COLOR_ATTACHMENT0);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glClearDepth(1.0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // All images for an orientation represent one geometric slice. Compose them in the same bottom-to-top order as
    // 2D views before DDP so equal-depth image fragments never rely on an artificial depth offset.
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_ALWAYS);
    glDepthMask(GL_TRUE);
    glEnable(GL_BLEND);
    glBlendEquation(GL_FUNC_ADD);
    glBlendFuncSeparate(GL_ONE, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    drawImagePlaneRenderablesWithProgram(
      m_appData,
      view,
      orientationList,
      context,
      m_meshImagePlaneCompositeProgram,
      m_meshImagePlaneCompositeTexture2DProgram,
      false,
      nullptr,
      nullptr);
  }

  glDisable(GL_DEPTH_TEST);
  glDepthMask(GL_FALSE);
}
