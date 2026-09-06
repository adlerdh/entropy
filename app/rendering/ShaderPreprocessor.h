#pragma once

#include <string>
#include <string_view>
#include <unordered_map>

namespace rendering
{

using ShaderReplacements = std::unordered_map<std::string, std::string>;

/**
 * @brief Assemble a GLSL template from Entropy-owned include snippets and scalar substitutions.
 *
 * Source snippets use an explicit, familiar include form on a line by itself:
 * `#include "entropy/HELPERS.glsl"`. Inline substitutions use `${SAMPLER_TYPE}`. Every directive in the input must
 * have a corresponding entry in @p replacements. Missing substitutions, malformed Entropy includes, recursive
 * includes, and the retired `$$TOKEN$$` syntax are rejected before OpenGL sees the source.
 *
 * The include syntax is intentionally handled by Entropy rather than relying on
 * `GL_ARB_shading_language_include`, which is not universally available on the supported OpenGL implementations.
 */
std::string preprocessShaderSource(std::string_view source, const ShaderReplacements& replacements);

} // namespace rendering
