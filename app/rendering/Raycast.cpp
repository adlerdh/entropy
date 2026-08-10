#include "rendering/Rendering.h"

#include "logic/app/Data.h"
#include "rendering/ImageDrawing.h"
#include "rendering/utility/gl/GLTexture.h"
#include "windowing/View.h"

#include <spdlog/spdlog.h>

#include <functional>
#include <list>
#include <optional>

namespace
{
using namespace uuids;
} // namespace

void Rendering::volumeRenderOneImage(
  const View& view,
  GLShaderProgram& program,
  const glm::mat4& texture_T_world,
  const CurrentImages& imageSegPairs)
{
  auto getImage = [this](const std::optional<uuid>& imageUid) -> const Image* {
    return (imageUid ? m_appData.image(*imageUid) : nullptr);
  };

  drawRaycastQuad(program, m_appData.renderData().m_quad, view, texture_T_world, imageSegPairs, getImage);

  setupOpenGLState();
}

void Rendering::renderVolumeImagesForView(const View& view)
{
  const CurrentImages imageSegPairs = raycastImagesForView(view);
  if (imageSegPairs.empty() || !imageSegPairs.front().first) {
    return;
  }

  const std::optional<ActiveIsosurfaceEdit> activeEdit = activeIsosurfaceEdit(imageSegPairs);

  if (
    !activeEdit && m_appData.renderData().m_isosurfaceMeshRenderingEnabled &&
    renderIsosurfaceMeshesForView(view, imageSegPairs))
  {
    renderMeshLandmarksForView(view);
    return;
  }

  // The raycast shader remains a single-volume fallback. During isovalue edits, render only the edited surface through
  // this live path so all committed meshes are hidden until the edit finishes.
  const ImgSegPair imgSegPair = activeEdit ? activeEdit->imageSegPair : imageSegPairs.front();
  const std::optional<uuid> onlyIsosurfaceUid =
    activeEdit ? std::optional<uuid>{activeEdit->isosurfaceUid} : std::nullopt;
  if (!imgSegPair.first) {
    return;
  }

  const Image* image = m_appData.image(*imgSegPair.first);
  if (!image) {
    spdlog::warn("Null image {} when raycasting", *imgSegPair.first);
    return;
  }

  const ImageSettings& settings = image->settings();
  if (!settings.isosurfacesVisible() || !settings.showIsosurfacesIn3D()) {
    return; // Hide all surfaces
  }

  // Render surfaces of the active image component
  const uint32_t activeComp = image->settings().activeComponent();

  const auto isosurfaceUids = m_appData.isosurfaceUids(*imgSegPair.first, activeComp);
  if (isosurfaceUids.empty()) {
    return;
  }

  const auto deformationUid = activeRenderableDeformationUid(*imgSegPair.first);
  const bool renderWarped = deformationUid.has_value();

  updateIsosurfaceDataFor3d(m_appData, *imgSegPair.first, onlyIsosurfaceUid);

  const std::optional<uuid> referenceImageUid =
    renderWarped ? activeRenderableDeformationReferenceImageUid(*imgSegPair.first) : std::nullopt;
  const Image* domainImage = referenceImageUid ? m_appData.image(*referenceImageUid) : image;

  const auto boundImageTextures = bindScalarImageTextures(imgSegPair);
  const auto boundDefTextures =
    renderWarped ? bindDeformationTextures(*deformationUid) : std::list<std::reference_wrapper<GLTexture>>{};
  const auto boundSegBufferTextures = bindSegBufferTextures(imgSegPair);

  const auto& U = m_appData.renderData().m_uniforms.at(*imgSegPair.first);
  const auto& domainU = (referenceImageUid && m_appData.renderData().m_uniforms.count(*referenceImageUid) > 0u)
                          ? m_appData.renderData().m_uniforms.at(*referenceImageUid)
                          : U;

  GLShaderProgram& program = renderWarped ? m_raycastIsoWarpedProgram : m_raycastIsoProgram;

  program.use();
  {
    setRaycastIsoUniforms(
      program,
      view,
      imgSegPair,
      *(domainImage ? domainImage : image),
      domainU,
      renderWarped,
      deformationUid);

    volumeRenderOneImage(
      view,
      program,
      domainU.imgTexture_T_world,
      CurrentImages{ImgSegPair{referenceImageUid.value_or(*imgSegPair.first), std::nullopt}});
  }
  program.stopUse();

  unbindTextures(boundDefTextures);
  unbindTextures(boundImageTextures);
  unbindBufferTextures(boundSegBufferTextures);
}
