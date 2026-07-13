#include "rendering/Rendering.h"

#include "logic/app/Data.h"
#include "rendering/helpers/PipelineHelpers.h"
#include "rendering/vector/VectorFieldOverlayDrawing.h"
#include "windowing/View.h"

#include <glm/glm.hpp>

#include <functional>
#include <list>
#include <optional>

namespace
{
using namespace uuids;

const Uniforms::SamplerIndexVectorType s_imageRgbaTexSamplers{{0, 1, 2, 3}};

} // namespace

void Rendering::renderVectorWarpedGridOverlaysForView(
  const View& view,
  const glm::vec3& worldOffsetXhairs,
  int displayModeUniform,
  const CurrentImages& sourceImages)
{
  if (ViewType::ThreeD == view.viewType()) {
    return;
  }

  const RenderData& R = m_appData.renderData();
  const Viewport& windowVP = m_appData.windowData().viewport();

  bool isFixedImage = true;
  for (const auto& imgSegPair : sourceImages) {
    if (!imgSegPair.first) {
      isFixedImage = false;
      continue;
    }

    const uuid& imageUid = *imgSegPair.first;
    const Image* image = m_appData.image(imageUid);
    if (
      !image || image->header().numComponentsPerPixel() != 3u || !image->settings().globalVisibility() ||
      !image->settings().vectorWarpedGridVisible() || activeRenderableDeformationUid(imageUid).has_value())
    {
      isFixedImage = false;
      continue;
    }
    if (R.m_imageTextures.find(imageUid) == R.m_imageTextures.end()) {
      isFixedImage = false;
      continue;
    }

    const ImageSettings& settings = image->settings();
    const RenderData::PlanarTextureLayout imageTextureLayout =
      rendering::textureLayoutOrDefault(R.m_imageTextureLayouts, ImgSegPair{imageUid, std::nullopt}.first);
    GLShaderProgram* program = nullptr;
    switch (settings.colorInterpolationMode()) {
      case InterpolationMode::NearestNeighbor:
      case InterpolationMode::Linear:
        program = &rendering::shaderProgramForTextureDimension(
          m_shaderPrograms,
          m_shaderPrograms2D,
          ShaderProgramType::VectorWarpedGridLinear,
          imageTextureLayout.dimension);
        break;
      case InterpolationMode::CubicBsplineConvolution:
        program = &rendering::shaderProgramForTextureDimension(
          m_shaderPrograms,
          m_shaderPrograms2D,
          ShaderProgramType::VectorWarpedGridCubic,
          imageTextureLayout.dimension);
        break;
    }

    const RenderData::ImageUniforms& U = R.m_uniforms.at(imageUid);
    const auto boundTextures = bindColorImageTextures(ImgSegPair{imageUid, std::nullopt});
    program->use();
    {
      program->setSamplerUniform("u_imgTex", s_imageRgbaTexSamplers);
      rendering::setTexture2DAxesUniforms(*program, imageTextureLayout);
      program->setUniform("u_numCheckers", static_cast<float>(R.m_numCheckerboardSquares));
      program->setUniform("u_tex_T_world", U.imgTexture_T_world);
      program->setUniform("u_imgSlope_native_T_texture", U.slope_native_T_texture);
      program->setUniform("u_subject_T_texture", image->transformations().subject_T_texture());
      program->setUniform(
        "u_viewRight_subject",
        rendering::vector_overlay::subjectViewDirection(*image, view, Directions::View::Right));
      program->setUniform(
        "u_viewUp_subject",
        rendering::vector_overlay::subjectViewDirection(*image, view, Directions::View::Up));
      program->setUniform(
        "u_gridSpacing_subject",
        rendering::vector_overlay::warpedGridSpacingMillimeters(settings, windowVP, view, *image, worldOffsetXhairs));
      program->setUniform("u_lineThickness_px", settings.vectorWarpedGridLineThickness());
      program->setUniform("u_warpScale", settings.vectorWarpedGridScaleFactor());
      program->setUniform(
        "u_convention",
        VectorWarpedGridConvention::ApparentDeformation == settings.vectorWarpedGridConvention() ? 1 : 0);
      program->setUniform("u_foregroundColor", settings.vectorWarpedGridForegroundColor());
      program->setUniform("u_backgroundColor", settings.vectorWarpedGridBackgroundColor());
      program->setUniform("u_imgOpacity", U.imgOpacity);
      program->setUniform("u_quadrants", R.m_quadrants);
      program->setUniform("u_showFix", isFixedImage);
      program->setUniform("u_renderMode", displayModeUniform);

      renderOneImage(view, worldOffsetXhairs, *program, CurrentImages{ImgSegPair{imageUid, std::nullopt}}, false);
    }
    program->stopUse();
    unbindTextures(boundTextures);

    isFixedImage = false;
  }
}
