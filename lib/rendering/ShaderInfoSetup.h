#pragma once

#include "rendering/ShaderProgramSetup.h"
#include "rendering/ShaderSourceSetup.h"
#include "rendering/ShaderUniformSetup.h"
#include "rendering/common/ShaderType.h"

#include <unordered_map>
#include <vector>

namespace rendering::shader_setup
{

/**
 * @brief Return the main renderer shader program types in creation order.
 *
 * The order is the order used when OpenGL programs are compiled during renderer initialization. It should include only
 * the main image-rendering programs; specialized post-processing and raycast programs are built separately.
 */
std::vector<ShaderProgramType> buildShaderTypeList();

/**
 * @brief Build static shader metadata keyed by shader program type.
 *
 * The returned map describes shader filenames, fragment-source replacements, and uniform sets. It does not read shader
 * resources or create OpenGL objects; those responsibilities remain in the source setup and renderer initialization
 * layers.
 */
std::unordered_map<ShaderProgramType, ShaderInfo> buildShaderInfoMap(
  const ShaderSourceSet& sources,
  const ShaderUniformSet& uniforms);

} // namespace rendering::shader_setup
