#include "rendering/Rendering.h"

#include "common/Exception.hpp"
#include "common/Expected.h"
#include "rendering/PixelEdgeRenderer.h"
#include "rendering/PrivateMethods.h"
#include "rendering/RenderData.h"
#include "rendering/ShaderProgramSetup.h"
#include "rendering/TextureLayout.h"
#include "rendering/ascii/AsciiRenderer.h"
#include "rendering/common/ShaderType.h"
#include "rendering/helpers/PipelineHelpers.h"
#include "rendering/utility/containers/Uniforms.h"
#include "rendering/utility/gl/GLShader.h"
#include "rendering/utility/gl/GLShaderProgram.h"
#include "rendering/utility/gl/GLShaderType.h"

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

entropy_expected::expected<std::unique_ptr<GLShaderProgram>, std::string> createShaderProgram(
  const std::string& programName,
  const std::string& vsName,
  const std::string& fsName,
  const std::unordered_map<std::string, std::string>& fsReplacements,
  const Uniforms& vsUniforms,
  const Uniforms& fsUniforms)
{
  static const std::string shaderPath("app/rendering/shaders/");

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
    return entropy_expected::unexpected(
      std::format("Exception loading shader for program {}: {}", programName, e.what()));
  }

  fsSource = rendering::replacePlaceholders(fsSource, fsReplacements);

  GLShader vs(vsName, ShaderType::Vertex, vsSource.c_str());
  vs.setRegisteredUniforms(vsUniforms);

  GLShader fs(fsName, ShaderType::Fragment, fsSource.c_str());
  fs.setRegisteredUniforms(fsUniforms);

  auto program = std::make_unique<GLShaderProgram>(programName);

  if (!program->attachShader(vs)) {
    return entropy_expected::unexpected(std::format("Unable to compile vertex shader {}", vsName));
  }
  spdlog::debug("Compiled vertex shader {}", vsName);

  if (!program->attachShader(fs)) {
    return entropy_expected::unexpected(std::format("Unable to compile fragment shader {}", fsName));
  }
  spdlog::debug("Compiled fragment shader {}", fsName);

  if (!program->link()) {
    return entropy_expected::unexpected(std::format("Failed to link shader program {}", programName));
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
        RenderData::TextureDimension::Texture3D,
        setup.lookupReplacementSources),
      info.vsUniforms,
      info.fsUniforms);

    if (prog) {
      m_shaderPrograms.emplace(shaderType, std::move(*prog));
    }
    else {
      spdlog::error(prog.error());
      throwDebug(std::format("Failed to create shader program {}", to_string(shaderType)));
    }

    auto prog2D = createShaderProgram(
      to_string(shaderType) + " - Texture2D",
      info.vsFileName,
      info.fsFileName,
      rendering::shaderReplacementsForTextureDimension(
        info.fsReplacements,
        RenderData::TextureDimension::Texture2D,
        setup.lookupReplacementSources),
      info.vsUniforms,
      info.fsUniforms);

    if (prog2D) {
      m_shaderPrograms2D.emplace(shaderType, std::move(*prog2D));
    }
    else {
      spdlog::error(prog2D.error());
      throwDebug(std::format("Failed to create 2D shader program {}", to_string(shaderType)));
    }
  }

  if (!createRaycastIsoProgram(m_raycastIsoProgram, false)) {
    throwDebug("Failed to create isosurface raycasting program");
  }
  if (!createRaycastIsoProgram(m_raycastIsoWarpedProgram, true)) {
    throwDebug("Failed to create warped isosurface raycasting program");
  }

  m_asciiRenderer.registerShaderPrograms(m_shaderPrograms);
  m_pixelEdgeRenderer.registerShaderPrograms(m_shaderPrograms);
}
