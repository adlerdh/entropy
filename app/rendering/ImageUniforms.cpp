#include "rendering/Rendering.h"

#include "image/ImageColorMap.h"
#include "logic/app/Data.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_inverse.hpp>

#include <spdlog/fmt/ostr.h>
#include <spdlog/spdlog.h>

#include <utility>

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

void updateSegmentationUniformsForImage(
  const AppData& appData,
  const uuid& segmentationOwnerImageUid,
  const Image& image,
  RenderData::ImageUniforms& uniforms)
{
  const ImageSettings& imageSettings = image.settings();
  const auto& segUid = appData.imageToActiveSegUid(segmentationOwnerImageUid);
  if (!segUid) {
    uniforms.segOpacity = 0.0f;
    return;
  }

  const Image* seg = appData.seg(*segUid);
  if (!seg) {
    spdlog::error("Segmentation {} is null on updating uniforms for image {}", *segUid, segmentationOwnerImageUid);
    uniforms.segOpacity = 0.0f;
    return;
  }

  uniforms.segTexture_T_world =
    seg->transformations().texture_T_subject() * image.transformations().subject_T_worldDef();

  if (imageSettings.numComponents() > 1) {
    uniforms.segOpacity = static_cast<float>(
      ((seg->settings().visibility() && imageSettings.globalVisibility()) ? 1.0 : 0.0) * seg->settings().opacity());
  }
  else {
    uniforms.segOpacity = static_cast<float>(
      ((seg->settings().visibility() && imageSettings.visibility(0) && imageSettings.globalVisibility()) ? 1.0 : 0.0) *
      seg->settings().opacity());
  }
}

} // namespace

void Rendering::updateImageUniforms(uuid_range_t imageUids)
{
  for (const auto& imageUid : imageUids) {
    updateImageUniforms(imageUid);
  }
}

void Rendering::updateImageUniforms(const uuid& imageUid)
{
  const uuid effectiveImageUid = m_appData.effectiveImageUidForRendering(imageUid);
  if (effectiveImageUid != imageUid) {
    Image* source = m_appData.image(imageUid);
    Image* effective = m_appData.image(effectiveImageUid);
    if (source && effective) {
      syncScalarProjectionLayerSettings(source->settings(), effective->settings());
      updateImageUniforms(effectiveImageUid);
      RenderData::ImageUniforms& effectiveUniforms = m_appData.renderData().m_uniforms[effectiveImageUid];
      updateSegmentationUniformsForImage(m_appData, imageUid, *effective, effectiveUniforms);
    }
  }

  auto it = m_appData.renderData().m_uniforms.find(imageUid);

  if (std::end(m_appData.renderData().m_uniforms) == it) {
    spdlog::debug("Adding rendering uniforms for image {}", imageUid);
    m_appData.renderData().m_uniforms.insert(std::make_pair(imageUid, RenderData::ImageUniforms()));
  }

  RenderData::ImageUniforms& uniforms = m_appData.renderData().m_uniforms[imageUid];
  Image* img = m_appData.image(imageUid);
  if (!img) {
    uniforms.imgOpacity = 0.0f;
    uniforms.segOpacity = 0.0f;
    uniforms.showEdges = false;
    spdlog::error("Image {} is null on updating its uniforms; setting default uniform values", imageUid);
    return;
  }

  const auto& imgSettings = img->settings();

  uniforms.cmapQuantLevels =
    imgSettings.colorMapContinuous() ? 0 : static_cast<int>(imgSettings.colorMapQuantizationLevels());
  uniforms.hsvModFactors = imgSettings.colorMapHsvModFactors();

  if (const auto cmapUid = m_appData.imageColorMapUid(imgSettings.colorMapIndex())) {
    if (const ImageColorMap* map = m_appData.imageColorMap(*cmapUid)) {
      uniforms.cmapSlopeIntercept = map->slopeIntercept(imgSettings.isColorMapInverted());

      // If the color map has nearest-neighbor interpolation, then do NOT quantize:
      if (ImageColorMap::InterpolationMode::Nearest == map->interpolationMode()) {
        uniforms.cmapQuantLevels = 0;
      }
    }
    else {
      spdlog::error("Null image color map {} on updating uniforms for image {}", *cmapUid, imageUid);
    }
  }
  else {
    spdlog::error(
      "Invalid image color map at index {} on updating uniforms for image {}",
      imgSettings.colorMapIndex(),
      imageUid);
  }

  glm::mat4 imgTexture_T_world(1.0f);

  /// @note This has been removed, since the reference image transformations are now always locked!
  /// In order for this commented-out code to work, the uniforms for all images must be updated
  /// when the reference image transformation is updated. Otherwise, the uniforms of the
  /// other images will be out of sync.

  /*
    if ( m_appData.refImageUid() &&
         imgSettings.isLockedToReference() &&
         imageUid != m_appData.refImageUid() )
    {
        if ( const Image* refImg = m_appData.refImage() )
        {
            /// @note \c img->transformations().subject_T_worldDef() is the contactenation of
            /// both the initial loaded affine tx and the manually applied affine tx for the given
    image.
            /// It used here, since when the given image is locked to the reference image, the tx
    from
            /// World space to image Subject space (\c img->transformations().subject_T_worldDef() )
            /// is repurposed as the tx from reference image Subject space to image Subject space.
            const glm::mat4& imgSubject_T_refSubject = img->transformations().subject_T_worldDef();

            imgTexture_T_world =
                    img->transformations().texture_T_subject() *
                    imgSubject_T_refSubject *
                    refImg->transformations().subject_T_worldDef();
        }
        else
        {
            spdlog::error( "Null reference image" );
        }
    }
    else
    */
  {
    imgTexture_T_world = img->transformations().texture_T_worldDef();
  }

  uniforms.imgTexture_T_world = imgTexture_T_world;
  uniforms.world_T_imgTexture = glm::inverse(imgTexture_T_world);

  if (imgSettings.displayImageAsColor() && (3 == imgSettings.numComponents() || 4 == imgSettings.numComponents())) {
    for (uint32_t i = 0; i < imgSettings.numComponents(); ++i) {
      uniforms.slopeInterceptRgba_normalized_T_texture[i] = imgSettings.slopeInterceptVec2_normalized_T_texture(i);

      uniforms.thresholdsRgba[i] = glm::vec2{
        static_cast<float>(imgSettings.mapNativeIntensityToTexture(imgSettings.thresholds(i).first)),
        static_cast<float>(imgSettings.mapNativeIntensityToTexture(imgSettings.thresholds(i).second))};

      uniforms.minMaxRgba[i] = glm::vec2{
        static_cast<float>(imgSettings.mapNativeIntensityToTexture(imgSettings.minMaxImageRange(i).first)),
        static_cast<float>(imgSettings.mapNativeIntensityToTexture(imgSettings.minMaxImageRange(i).second))};

      uniforms.imgOpacityRgba[i] = static_cast<float>(
        ((imgSettings.globalVisibility() && imgSettings.visibility(i)) ? 1.0 : 0.0) * imgSettings.globalOpacity() *
        imgSettings.opacity(i));
    }

    if (3 == imgSettings.numComponents()) {
      // These two will be ignored for RGB images:
      uniforms.slopeInterceptRgba_normalized_T_texture[3] = glm::vec2{1.0f, 0.0f};
      uniforms.thresholdsRgba[3] = glm::vec2{0.0f, 1.0f};
      uniforms.minMaxRgba[3] = glm::vec2{0.0f, 1.0f};

      uniforms.imgOpacityRgba[3] =
        static_cast<float>((imgSettings.globalVisibility() ? 1.0 : 0.0) * imgSettings.globalOpacity());
    }
  }
  else {
    uniforms.slopeIntercept_normalized_T_texture = imgSettings.slopeInterceptVec2_normalized_T_texture();
  }

  uniforms.slope_native_T_texture = imgSettings.slope_native_T_texture();
  uniforms.largestSlopeIntercept = imgSettings.largestSlopeInterceptTextureVec2();

  const glm::vec3 dims = glm::vec3{img->header().pixelDimensions()};

  uniforms.textureGradientStep = glm::mat3{
    glm::vec3{1.0f / dims[0], 0.0f, 0.0f},
    glm::vec3{0.0f, 1.0f / dims[1], 0.0f},
    glm::vec3{0.0f, 0.0f, 1.0f / dims[2]}};

  uniforms.voxelSpacing = img->header().spacing();

  // Map the native thresholds to OpenGL texture values:
  uniforms.thresholds = glm::vec2{
    static_cast<float>(imgSettings.mapNativeIntensityToTexture(imgSettings.thresholds().first)),
    static_cast<float>(imgSettings.mapNativeIntensityToTexture(imgSettings.thresholds().second))};

  // Map the native image values to OpenGL texture values:
  uniforms.minMax = glm::vec2{
    static_cast<float>(imgSettings.mapNativeIntensityToTexture(imgSettings.minMaxImageRange().first)),
    static_cast<float>(imgSettings.mapNativeIntensityToTexture(imgSettings.minMaxImageRange().second))};

  uniforms.imgOpacity = static_cast<float>(
    ((imgSettings.globalVisibility() && imgSettings.visibility()) ? 1.0 : 0.0) * imgSettings.opacity() *
    ((imgSettings.numComponents() > 0) ? imgSettings.globalOpacity() : 1.0));

  // Edges
  uniforms.showEdges = imgSettings.showEdges();
  uniforms.thresholdEdges = imgSettings.thresholdEdges();
  uniforms.edgeMagnitude = static_cast<float>(imgSettings.edgeMagnitude());
  uniforms.overlayEdges = imgSettings.overlayEdges();
  uniforms.colormapEdges = imgSettings.colormapEdges();
  uniforms.edgeColor = static_cast<float>(imgSettings.edgeOpacity()) * glm::vec4{imgSettings.edgeColor(), 1.0f};
  uniforms.showPixelEdges = imgSettings.showPixelEdges();
  uniforms.thresholdPixelEdges = imgSettings.thresholdPixelEdges();
  uniforms.thinPixelEdges = imgSettings.thinPixelEdges();
  uniforms.pixelEdgeScale = static_cast<float>(imgSettings.pixelEdgeScale());
  uniforms.pixelEdgeThreshold = static_cast<float>(imgSettings.pixelEdgeThreshold());
  uniforms.overlayPixelEdges = imgSettings.overlayPixelEdges();

  updateSegmentationUniformsForImage(m_appData, imageUid, *img, uniforms);
}
