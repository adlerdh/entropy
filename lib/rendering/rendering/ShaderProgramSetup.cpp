#include "rendering/ShaderProgramSetup.h"
#include "rendering/ShaderInfoSetup.h"
#include "rendering/ShaderSourceSetup.h"
#include "rendering/ShaderUniformSetup.h"

namespace rendering::shader_setup
{

ProgramSetup buildProgramSetup()
{
  const ShaderSourceSet sources = buildShaderSourceSet();
  const ShaderUniformSet uniforms = buildShaderUniformSet();

  return ProgramSetup{
    .shaderTypes = buildShaderTypeList(),
    .shaderInfo = buildShaderInfoMap(sources, uniforms),
    .lookupReplacementSources = sources.textureLookupReplacementSources()};
}

} // namespace rendering::shader_setup
