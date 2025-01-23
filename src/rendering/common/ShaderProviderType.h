#ifndef SHADER_PROVIDER_TYPE_H
#define SHADER_PROVIDER_TYPE_H

#include <functional>
#include <string>

class GLShaderProgram;
class Uniforms;

/// Functional activiting a shader program (setting it as the current program)
/// and returning a pointer to it if successful (nullptr it not successful).
using ShaderProgramActivatorType = std::function<GLShaderProgram*(const std::string& shaderName)>;

/// Functional returning the uniforms for a given shader program.
/// @todo Make this return pointer instead of reference and nullptr as sentinel.
using UniformsProviderType = std::function<const Uniforms&(const std::string& shaderName)>;

#endif // SHADER_PROVIDER_TYPE_H
