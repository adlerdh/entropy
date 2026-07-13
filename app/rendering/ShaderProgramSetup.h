#pragma once

#include "rendering/common/ShaderType.h"
#include "rendering/helpers/PipelineHelpers.h"
#include "rendering/utility/containers/Uniforms.h"

#include <string>
#include <unordered_map>
#include <vector>

namespace rendering::shader_setup
{

/**
 * @brief Static description of one linked shader program.
 *
 * The filenames identify shader resources embedded in the application. Fragment shader replacements contain source
 * snippets that are substituted before compilation. Uniform sets describe the vertex and fragment uniforms expected by
 * the linked program.
 */
struct ShaderInfo
{
  std::string vsFileName;
  std::string fsFileName;
  std::unordered_map<std::string, std::string> fsReplacements;
  Uniforms vsUniforms;
  Uniforms fsUniforms;
};

/**
 * @brief Complete setup data needed to compile the main 2D image-rendering shader programs.
 */
struct ProgramSetup
{
  std::vector<ShaderProgramType> shaderTypes;
  std::unordered_map<ShaderProgramType, ShaderInfo> shaderInfo;
  TextureLookupReplacementSources lookupReplacementSources;
};

/**
 * @brief Build the shader program table for the main renderer.
 *
 * This function assembles shader source snippets, uniform declarations, and shader-type metadata, but does not create
 * or link OpenGL objects.
 */
ProgramSetup buildProgramSetup();

} // namespace rendering::shader_setup
