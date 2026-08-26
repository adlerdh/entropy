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

void Rendering::renderVolumeImagesForView(const View& view, const bool interactiveOverlay)
{
  const CurrentImages imageSegPairs = raycastImagesForView(view);
  if (imageSegPairs.empty() || !imageSegPairs.front().first) {
    return;
  }

  const std::optional<ActiveIsosurfaceEdit> activeEdit = activeIsosurfaceEdit(imageSegPairs);
  if (activeEdit) {
    m_isosurfaceRaycastHandoff = activeEdit;
  }

  bool meshSceneWasRendered = false;
  if (!interactiveOverlay && m_appData.renderData().m_isosurfaceMeshRenderingEnabled) {
    const bool allMeshesReady = renderIsosurfaceMeshesForView(view, imageSegPairs);
    meshSceneWasRendered = true;
    if (allMeshesReady) {
      m_isosurfaceRaycastHandoff.reset();
      renderMeshLandmarksForView(view);
      return;
    }
  }

  // The raycast shader remains a single-volume fallback. During isovalue edits, render only the edited surface through
  // this live path so all committed meshes are hidden until the edit finishes.
  const std::optional<ActiveIsosurfaceEdit> handoffSurface = activeEdit ? activeEdit : m_isosurfaceRaycastHandoff;
  const ImgSegPair imgSegPair = handoffSurface ? handoffSurface->imageSegPair : imageSegPairs.front();
  const std::optional<uuid> onlyIsosurfaceUid =
    handoffSurface ? std::optional<uuid>{handoffSurface->isosurfaceUid} : std::nullopt;
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
      imgSegPair,
      *(domainImage ? domainImage : image),
      domainU,
      renderWarped,
      deformationUid);
    if (interactiveOverlay || meshSceneWasRendered) {
      // The live edit preview is composited after the mesh scene. Do not paint the raycast background over it: only
      // fragments contributed by the edited isosurface should change the existing framebuffer color.
      program.setUniform("u_bgColor", glm::vec4{0.0f});
      program.setUniform("u_bgEdgeBrighteningEnabled", false);
      program.setUniform("u_noHitTransparent", true);
    }

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
