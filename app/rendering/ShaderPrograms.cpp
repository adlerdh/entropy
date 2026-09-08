#include "rendering/Rendering.h"

#include "common/Exception.hpp"
#include "rendering/PixelEdgeRenderer.h"
#include "rendering/PrivateMethods.h"
#include "rendering/RenderResources.h"
#include "rendering/RenderSettings.h"
#include "rendering/ShaderProgramSetup.h"
#include "rendering/ShaderPreprocessor.h"
#include "rendering/ShaderTextureDimension.h"
#include "rendering/TextureLayout.h"
#include "rendering/ascii/AsciiRenderer.h"
#include "rendering/common/ShaderType.h"
#include "rendering/gl/Uniforms.h"
#include "rendering/gl/GLShader.h"
#include "rendering/gl/GLShaderProgram.h"
#include "rendering/gl/GLShaderType.h"

#include <cmrc/cmrc.hpp>
#include <spdlog/spdlog.h>

#include <exception>
#include <expected>
#include <format>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

CMRC_DECLARE(shaders);

namespace
{

std::expected<std::unique_ptr<GLShaderProgram>, std::string> createShaderProgram(
  const std::string& programName,
  const std::string& vsName,
  const std::string& fsName,
  const std::unordered_map<std::string, std::string>& fsReplacements,
  const Uniforms& vsUniforms,
  const Uniforms& fsUniforms)
{
  static const std::string shaderPath("rendering/shaders/");

  spdlog::debug("Creating shader program '{}'", programName);

  const auto filesystem = cmrc::shaders::get_filesystem();
  std::string vsSource;
  std::string fsSource;

  try {
    const cmrc::file vsData = filesystem.open(shaderPath + vsName);
    const cmrc::file fsData = filesystem.open(shaderPath + fsName);
    vsSource = std::string(vsData.begin(), vsData.end());
    fsSource = std::string(fsData.begin(), fsData.end());
  }
  catch (const std::exception& e) {
    return std::unexpected(std::format("Exception loading shader for program {}: {}", programName, e.what()));
  }

  fsSource = rendering::preprocessShaderSource(fsSource, fsReplacements);

  GLShader vs(vsName, ShaderType::Vertex, vsSource.c_str());
  vs.setRegisteredUniforms(vsUniforms);

  GLShader fs(fsName, ShaderType::Fragment, fsSource.c_str());
  fs.setRegisteredUniforms(fsUniforms);

  auto program = std::make_unique<GLShaderProgram>(programName);

  if (!program->attachShader(vs)) {
    return std::unexpected(std::format("Unable to compile vertex shader {}", vsName));
  }
  spdlog::debug("Compiled vertex shader {}", vsName);

  if (!program->attachShader(fs)) {
    return std::unexpected(std::format("Unable to compile fragment shader {}", fsName));
  }
  spdlog::debug("Compiled fragment shader {}", fsName);

  if (!program->link()) {
    return std::unexpected(std::format("Failed to link shader program {}", programName));
  }

  spdlog::debug("Linked shader program {}", programName);
  return program;
}

} // namespace

void Rendering::createShaderPrograms()
{
  const auto setup = rendering::shader_setup::buildProgramSetup();

  for (const auto& shaderType : setup.shaderTypes) {
    const auto& info = setup.shaderInfo.at(shaderType);

    auto prog = createShaderProgram(
      to_string(shaderType),
      info.vsFileName,
      info.fsFileName,
      rendering::shaderReplacementsForTextureDimension(
        info.fsReplacements,
        rendering::TextureDimension::Texture3D,
        setup.lookupReplacementSources),
      info.vsUniforms,
      info.fsUniforms);

    if (prog) {
      m_shaderPrograms.emplace(shaderType, std::move(*prog));
    }
    else {
      spdlog::critical("{}; Entropy cannot start without its rendering shaders", prog.error());
      throwDebug(std::format("Failed to create shader program {}", to_string(shaderType)));
    }

    auto prog2D = createShaderProgram(
      to_string(shaderType) + " - Texture2D",
      info.vsFileName,
      info.fsFileName,
      rendering::shaderReplacementsForTextureDimension(
        info.fsReplacements,
        rendering::TextureDimension::Texture2D,
        setup.lookupReplacementSources),
      info.vsUniforms,
      info.fsUniforms);

    if (prog2D) {
      m_shaderPrograms2D.emplace(shaderType, std::move(*prog2D));
    }
    else {
      spdlog::critical("{}; Entropy cannot start without its rendering shaders", prog2D.error());
      throwDebug(std::format("Failed to create 2D shader program {}", to_string(shaderType)));
    }
  }

  if (!createRaycastIsoProgram(m_raycastIsoProgram, false)) {
    throwDebug("Failed to create isosurface raycasting program");
  }
  if (!createRaycastIsoProgram(m_raycastIsoWarpedProgram, true)) {
    throwDebug("Failed to create warped isosurface raycasting program");
  }
  if (!createMeshProgram(m_meshProgram)) {
    throwDebug("Failed to create mesh program");
  }
  if (!createMeshEdgesProgram(m_meshEdgesProgram)) {
    throwDebug("Failed to create mesh triangle-edges program");
  }
  if (!createMeshShadowDepthProgram(m_meshShadowDepthProgram)) {
    throwDebug("Failed to create mesh shadow-depth program");
  }
  if (!createMeshAmbientOcclusionGeometryProgram(m_meshAmbientOcclusionGeometryProgram)) {
    throwDebug("Failed to create mesh ambient occlusion geometry program");
  }
  if (!createMeshAmbientOcclusionResolveProgram(m_meshAmbientOcclusionResolveProgram)) {
    throwDebug("Failed to create mesh ambient occlusion resolve program");
  }
  if (!createMeshAmbientOcclusionFilterProgram(m_meshAmbientOcclusionFilterProgram)) {
    throwDebug("Failed to create mesh ambient occlusion filter program");
  }
  if (!createMeshImagePlaneGrayLinearProgram(m_meshImagePlaneGrayLinearProgram)) {
    throwDebug("Failed to create mesh image-plane grayscale program");
  }
  if (!createMeshImagePlaneGrayLinearTexture2DProgram(m_meshImagePlaneGrayLinearTexture2DProgram)) {
    throwDebug("Failed to create mesh image-plane grayscale Texture2D program");
  }
  if (!createMeshImagePlaneIsoContourProgram(m_meshImagePlaneIsoContourProgram)) {
    throwDebug("Failed to create mesh image-plane isocontour program");
  }
  if (!createMeshImagePlaneIsoContourTexture2DProgram(m_meshImagePlaneIsoContourTexture2DProgram)) {
    throwDebug("Failed to create mesh image-plane isocontour Texture2D program");
  }
  if (!createMeshImagePlaneDdpInitProgram(m_meshImagePlaneDdpInitProgram)) {
    throwDebug("Failed to create mesh image-plane DDP init program");
  }
  if (!createMeshImagePlaneDdpInitTexture2DProgram(m_meshImagePlaneDdpInitTexture2DProgram)) {
    throwDebug("Failed to create mesh image-plane DDP init Texture2D program");
  }
  if (!createMeshImagePlaneDdpPeelProgram(m_meshImagePlaneDdpPeelProgram)) {
    throwDebug("Failed to create mesh image-plane DDP peel program");
  }
  if (!createMeshImagePlaneDdpPeelTexture2DProgram(m_meshImagePlaneDdpPeelTexture2DProgram)) {
    throwDebug("Failed to create mesh image-plane DDP peel Texture2D program");
  }
  if (!createMeshImagePlaneCompositeProgram(m_meshImagePlaneCompositeProgram)) {
    throwDebug("Failed to create mesh image-plane composite program");
  }
  if (!createMeshImagePlaneCompositeTexture2DProgram(m_meshImagePlaneCompositeTexture2DProgram)) {
    throwDebug("Failed to create mesh image-plane composite Texture2D program");
  }
  if (!createMeshImagePlaneBorderProgram(m_meshImagePlaneBorderProgram)) {
    throwDebug("Failed to create mesh image-plane border program");
  }
  if (!createMeshImagePlaneCompositeDdpInitProgram(m_meshImagePlaneCompositeDdpInitProgram)) {
    throwDebug("Failed to create mesh image-plane composite DDP init program");
  }
  if (!createMeshImagePlaneCompositeDdpPeelProgram(m_meshImagePlaneCompositeDdpPeelProgram)) {
    throwDebug("Failed to create mesh image-plane composite DDP peel program");
  }
  if (!createMeshDdpInitProgram(m_meshDdpInitProgram)) {
    throwDebug("Failed to create mesh DDP init program");
  }
  if (!createMeshDdpInitEdgesProgram(m_meshDdpInitEdgesProgram)) {
    throwDebug("Failed to create mesh DDP triangle-edges init program");
  }
  if (!createMeshDdpPeelProgram(m_meshDdpPeelProgram)) {
    throwDebug("Failed to create mesh DDP peel program");
  }
  if (!createMeshDdpPeelEdgesProgram(m_meshDdpPeelEdgesProgram)) {
    throwDebug("Failed to create mesh DDP triangle-edges peel program");
  }
  if (!createMeshDdpCompletionProgram(m_meshDdpCompletionProgram)) {
    throwDebug("Failed to create mesh DDP completion program");
  }
  if (!createMeshDdpBackBlendProgram(m_meshDdpBackBlendProgram)) {
    throwDebug("Failed to create mesh DDP back-blend program");
  }
  if (!createMeshDdpResolveProgram(m_meshDdpResolveProgram)) {
    throwDebug("Failed to create mesh DDP resolve program");
  }

  m_asciiRenderer.registerShaderPrograms(m_shaderPrograms);
  m_pixelEdgeRenderer.registerShaderPrograms(m_shaderPrograms);
}
