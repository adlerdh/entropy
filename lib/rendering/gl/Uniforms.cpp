#include "rendering/gl/Uniforms.h"

#include <glad/glad.h>

#include <spdlog/fmt/ostr.h>
#include <spdlog/spdlog.h>

#include <iterator>
#include <stdexcept>
#include <string>
#include <tuple>
#include <type_traits>
#include <utility>

namespace
{

void requireMatchingValue(const UniformType type, const Uniforms::ValueType& value)
{
  if (!Uniforms::valueMatchesType(type, value)) {
    throw std::invalid_argument(
      "Uniform value does not match declared type " + Uniforms::getUniformTypeString(static_cast<GLenum>(type)));
  }
}

} // namespace

Uniforms::Decl::Decl()
  : m_type(UniformType::Undefined), m_defaultValue(0), m_value(0), m_location(-1), m_isRequired(false), m_isDirty(true)
{
}

Uniforms::Decl::Decl(UniformType type, const ValueType& defaultValue, bool isRequired)
  : m_type(type)
  , m_defaultValue(defaultValue)
  , m_value(defaultValue)
  , m_location(-1)
  , m_isRequired(isRequired)
  , m_isDirty(true)
{
  requireMatchingValue(type, defaultValue);
}

void Uniforms::Decl::set(const ValueType& valueArg)
{
  requireMatchingValue(m_type, valueArg);
  m_value = valueArg;
  m_isDirty = true;
}

Uniforms::Uniforms(UniformsMap map) : m_uniformsMap(std::move(map))
{
  for (const auto& [name, uniform] : m_uniformsMap) {
    static_cast<void>(name);
    requireMatchingValue(uniform.m_type, uniform.m_defaultValue);
    requireMatchingValue(uniform.m_type, uniform.m_value);
  }
}

bool Uniforms::insertUniform(const std::string& name, const Uniforms::Decl& uniform)
{
  requireMatchingValue(uniform.m_type, uniform.m_defaultValue);
  requireMatchingValue(uniform.m_type, uniform.m_value);
  if (const auto existing = m_uniformsMap.find(name); existing != m_uniformsMap.end()) {
    if (existing->second.m_type != uniform.m_type) {
      throw std::invalid_argument("Conflicting declarations for uniform '" + name + "'");
    }
    return false;
  }
  auto result = m_uniformsMap.insert({name, uniform});
  return result.second;
}

bool Uniforms::insertUniform(const std::string& name, const UniformType& type, ValueType defaultValue, bool isRequired)
{
  requireMatchingValue(type, defaultValue);
  if (const auto existing = m_uniformsMap.find(name); existing != m_uniformsMap.end()) {
    if (existing->second.m_type != type) {
      throw std::invalid_argument("Conflicting declarations for uniform '" + name + "'");
    }
    return false;
  }

  auto result = m_uniformsMap.emplace(
    std::piecewise_construct,
    std::forward_as_tuple(name),
    std::forward_as_tuple(type, defaultValue, isRequired));

  return result.second;
}

void Uniforms::insertUniforms(const Uniforms& uniforms)
{
  for (const auto& [name, uniform] : uniforms()) {
    static_cast<void>(insertUniform(name, uniform));
  }
}

const Uniforms::Decl& Uniforms::operator()(const std::string& name) const
{
  return m_uniformsMap.at(name);
}

const Uniforms::UniformsMap& Uniforms::operator()() const
{
  return m_uniformsMap;
}

bool Uniforms::containsKey(const std::string& name) const
{
  auto itr = m_uniformsMap.find(name);
  return std::end(m_uniformsMap) != itr;
}

void Uniforms::resetAllToDefaults()
{
  for (auto& uniform : m_uniformsMap) {
    Decl& u = uniform.second;
    u.m_value = u.m_defaultValue;
    u.m_isDirty = true;
  }
}

void Uniforms::setValue(const std::string& name, const ValueType& valueArg)
{
  Decl& u = m_uniformsMap.at(name);
  requireMatchingValue(u.m_type, valueArg);
  u.m_value = valueArg;
  u.m_isDirty = true;
}

Uniforms::ValueType Uniforms::value(const std::string& name) const
{
  return m_uniformsMap.at(name).m_value;
}

void Uniforms::setLocation(const std::string& name, GLint loc)
{
  Decl& u = m_uniformsMap.at(name);
  u.m_location = loc;
  u.m_isDirty = true;
}

std::optional<GLint> Uniforms::location(const std::string& name) const
{
  const auto itr = m_uniformsMap.find(name);
  if (std::end(m_uniformsMap) != itr) {
    return itr->second.m_location;
  }
  else {
    return std::nullopt;
  }
}

GLint Uniforms::queryAndSetLocation(
  const std::string& name,
  const std::function<GLint(const std::string&)>& locationGetter)
{
  const GLint loc = locationGetter(name);
  setLocation(name, loc);

  if (-1 == loc) {
    const Decl& uniform = m_uniformsMap.at(name);
    if (uniform.m_isRequired) {
      spdlog::warn("Required uniform '{}' is not active in the linked shader program", name);
    }
    else {
      spdlog::trace("Optional uniform '{}' is not active in the linked shader program; skipping location setup", name);
    }
    return loc;
  }
  return loc;
}

void Uniforms::queryAndSetAllLocations(const std::function<GLint(const std::string&)>& locationGetter)
{
  for (const auto& uniform : m_uniformsMap) {
    static_cast<void>(queryAndSetLocation(uniform.first, locationGetter));
  }
}

void Uniforms::setDirty(const std::string& name, bool isDirtyArg)
{
  m_uniformsMap.at(name).m_isDirty = isDirtyArg;
}

void Uniforms::setRequired(const std::string& name, bool isRequired)
{
  m_uniformsMap.at(name).m_isRequired = isRequired;
}

bool Uniforms::isDirty(const std::string& name) const
{
  return m_uniformsMap.at(name).m_isDirty;
}

bool Uniforms::valueMatchesType(const UniformType type, const ValueType& value) noexcept
{
  return std::visit(
    [type](const auto& payload) {
      using Payload = std::decay_t<decltype(payload)>;
      if (type == UniformType::Undefined) {
        return true;
      }
      if constexpr (std::is_same_v<Payload, bool>) {
        return type == UniformType::Bool;
      }
      else if constexpr (std::is_same_v<Payload, int>) {
        return type == UniformType::Int;
      }
      else if constexpr (std::is_same_v<Payload, unsigned int>) {
        return type == UniformType::UInt;
      }
      else if constexpr (std::is_same_v<Payload, float>) {
        return type == UniformType::Float;
      }
      else if constexpr (std::is_same_v<Payload, glm::ivec2>) {
        return type == UniformType::IVec2 || type == UniformType::BVec2;
      }
      else if constexpr (std::is_same_v<Payload, glm::vec2>) {
        return type == UniformType::Vec2;
      }
      else if constexpr (std::is_same_v<Payload, glm::vec3>) {
        return type == UniformType::Vec3;
      }
      else if constexpr (std::is_same_v<Payload, glm::vec4>) {
        return type == UniformType::Vec4;
      }
      else if constexpr (std::is_same_v<Payload, glm::mat2>) {
        return type == UniformType::Mat2;
      }
      else if constexpr (std::is_same_v<Payload, glm::mat3>) {
        return type == UniformType::Mat3;
      }
      else if constexpr (std::is_same_v<Payload, glm::mat4>) {
        return type == UniformType::Mat4;
      }
      else if constexpr (std::is_same_v<Payload, SamplerIndexType>) {
        return type == UniformType::Sampler;
      }
      else if constexpr (std::is_same_v<Payload, SamplerIndexVectorType>) {
        return type == UniformType::SamplerVector;
      }
      else if constexpr (std::is_same_v<Payload, std::vector<float>>) {
        return type == UniformType::FloatVector;
      }
      else if constexpr (std::is_same_v<Payload, std::vector<glm::vec2>>) {
        return type == UniformType::Vec2Vector;
      }
      else if constexpr (std::is_same_v<Payload, std::vector<glm::mat4>>) {
        return type == UniformType::Mat4Vector;
      }
      else if constexpr (std::is_same_v<Payload, std::vector<glm::vec3>>) {
        return type == UniformType::Vec3Vector;
      }
      else if constexpr (std::is_same_v<Payload, std::vector<glm::vec4>>) {
        return type == UniformType::Vec4Vector;
      }
      else if constexpr (std::is_same_v<Payload, std::vector<int>>) {
        return type == UniformType::IntVector;
      }
      else if constexpr (std::is_same_v<Payload, std::array<float, 2>>) {
        return type == UniformType::FloatArray2;
      }
      else if constexpr (std::is_same_v<Payload, std::array<float, 3>>) {
        return type == UniformType::FloatArray3;
      }
      else if constexpr (std::is_same_v<Payload, std::array<float, 4>>) {
        return type == UniformType::FloatArray4;
      }
      else if constexpr (std::is_same_v<Payload, std::array<float, 5>>) {
        return type == UniformType::FloatArray5;
      }
      else if constexpr (std::is_same_v<Payload, std::array<std::uint32_t, 5>>) {
        return type == UniformType::UIntArray5;
      }
      else if constexpr (std::is_same_v<Payload, std::array<glm::vec3, 8>>) {
        return type == UniformType::Vec3Array8;
      }
      else {
        return false;
      }
    },
    value);
}

std::string Uniforms::getUniformTypeString(const GLenum type)
{
  switch (type) {
    case GL_BOOL:
      return "bool";
    case GL_INT:
      return "int";
    case GL_UNSIGNED_INT:
      return "uint";
    case GL_FLOAT:
      return "float";
    case GL_DOUBLE:
      return "double";
    case GL_FLOAT_VEC2:
      return "vec2";
    case GL_INT_VEC2:
      return "ivec2";
    case GL_BOOL_VEC2:
      return "bvec2";
    case GL_FLOAT_VEC3:
      return "vec3";
    case GL_FLOAT_VEC4:
      return "vec4";
    case GL_FLOAT_MAT2:
      return "mat2";
    case GL_FLOAT_MAT3:
      return "mat3";
    case GL_FLOAT_MAT4:
      return "mat4";
    case GL_SAMPLER_1D:
      return "sampler1D";
    case GL_SAMPLER_2D:
      return "sampler2D";
    case GL_SAMPLER_3D:
      return "sampler3D";
    case GL_SAMPLER_CUBE:
      return "samplerCube";
    case GL_SAMPLER_BUFFER:
      return "samplerBuffer";
    case GL_INT_SAMPLER_1D:
      return "isampler1D";
    case GL_INT_SAMPLER_2D:
      return "isampler2D";
    case GL_INT_SAMPLER_3D:
      return "isampler3D";
    case GL_INT_SAMPLER_BUFFER:
      return "isamplerBuffer";
    case GL_UNSIGNED_INT_SAMPLER_1D:
      return "usampler1D";
    case GL_UNSIGNED_INT_SAMPLER_2D:
      return "usampler2D";
    case GL_UNSIGNED_INT_SAMPLER_3D:
      return "usampler3D";
    case GL_UNSIGNED_INT_SAMPLER_BUFFER:
      return "usamplerBuffer";

    case 1:
      return "Sampler";
    case 2:
      return "SamplerIndexVector";
    case 3:
      return "FloatVector";
    case 4:
      return "Vec2Vector";
    case 5:
      return "Mat4Vector";
    case 6:
      return "Vec3Vector";
    case 7:
      return "FloatArray2";
    case 8:
      return "FloatArray3";
    case 9:
      return "FloatArray4";
    case 10:
      return "FloatArray5";
    case 11:
      return "UintArray5";
    case 12:
      return "Vec3Array8";
    case 13:
      return "Vec4Vector";
    case 14:
      return "IntVector";

    default:
      return "unknown";
  }
}
