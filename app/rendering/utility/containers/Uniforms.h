#pragma once

#include "rendering/utility/gl/GLUniformTypes.h"

#include <glm/mat4x4.hpp>
#include <glm/vec4.hpp>

#include <glad/glad.h>

#include <array>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

class GLShaderProgram;

/**
 * @brief Registry of expected uniform variables for a GLSL shader program.
 *
 * `Uniforms` tracks a declared type, default value, current value, linked-program location, and dirty flag for each
 * GLSL uniform known to the renderer. Shader setup code uses this to validate linked programs, upload only active
 * values, and reset program state to known defaults.
 */
class Uniforms
{
public:
  // To avoid ambiguity, we define types used to specifically encapsulate sampler indices.
  // Note that OpenGL expects sampler indices to be set with int32_t (signed) in glUniform1i(v).
  // However, the signed integer type conflicts with other OpenGL function calls that expect
  // sampler indices to be unsigned. Let's just use unsigned uint32_t, with the understanding
  // that sampler indices will never exceed the maximum signed value.
  struct SamplerIndexType
  {
    std::int32_t index;
  };

  /// Value wrapper for sampler arrays uploaded with `glUniform1iv()`.
  struct SamplerIndexVectorType
  {
    std::vector<std::int32_t> indices;
  };

  /// All value payload types currently supported by `GLShaderProgram::applyUniforms()`.
  using ValueType = std::variant<
    bool,
    int,
    unsigned int,
    float,
    glm::ivec2,
    glm::vec2,
    glm::vec3,
    glm::vec4,
    glm::mat2,
    glm::mat3,
    glm::mat4,
    SamplerIndexType,

    SamplerIndexVectorType,
    std::vector<float>,
    std::vector<glm::vec2>,
    std::vector<glm::mat4>,
    std::vector<glm::vec3>,
    std::array<float, 2>,
    std::array<float, 3>,
    std::array<float, 4>,
    std::array<float, 5>,
    std::array<std::uint32_t, 5>,
    std::array<glm::vec3, 8> >;

  /**
   * @brief Declaration and current state for one uniform.
   */
  struct Decl
  {
    Decl();
    Decl(UniformType type, const ValueType& defaultValue, bool isRequired);

    Decl(const Decl&) = default;
    Decl& operator=(const Decl&) = default;

    Decl(Decl&&) = default;
    Decl& operator=(Decl&&) = default;

    ~Decl() = default;

    /// Replace the current value and mark the declaration dirty for the next upload.
    void set(const ValueType& value);

    UniformType m_type; //!< Expected OpenGL or Entropy-specific uniform type

    ValueType m_defaultValue; //!< Value restored by `resetAllToDefaults()`

    ValueType m_value; //!< Current value to upload when dirty

    GLint m_location; //!< Linked-program location, or `-1` if inactive, absent, or not yet queried

    bool m_isRequired; //!< Whether this uniform is expected to be active in every linked shader variant

    bool m_isDirty; //!< Whether the current value must be uploaded before the next draw
  };

  /// Hash map of uniforms, keyed by uniform name used in the GLSL code.
  using UniformsMap = std::unordered_map<std::string, Decl>;

  explicit Uniforms() = default;
  explicit Uniforms(UniformsMap map);

  Uniforms(const Uniforms&) = default;
  Uniforms& operator=(const Uniforms&) = default;

  Uniforms(Uniforms&&) = default;
  Uniforms& operator=(Uniforms&&) = default;

  ~Uniforms() = default;

  /// Insert a single uniform declaration. Existing declarations with the same name are left unchanged.
  bool insertUniform(const std::string& name, const Decl& uniform);

  /// Insert a single uniform declaration from its pieces. Existing declarations with the same name are left unchanged.
  bool insertUniform(const std::string& name, const UniformType& type, ValueType defaultValue, bool isRequired = true);

  /// Insert another set of uniforms. Existing declarations with the same names are left unchanged.
  void insertUniforms(const Uniforms& uniforms);

  /// Restore every current value to its default and mark every declaration dirty.
  void resetAllToDefaults();

  /// @throws If a uniform with the given name does not exist.
  void setValue(const std::string& name, const ValueType& value);

  /// @throws If a uniform with the given name does not exist.
  ValueType value(const std::string& name) const;

  /// Store the already queried linked-program location for a uniform.
  void setLocation(const std::string& name, GLint loc);

  /// Return the stored linked-program location, if this registry contains the uniform.
  std::optional<GLint> location(const std::string& name) const;

  /// Query and store one linked-program location using the supplied lookup function.
  GLint queryAndSetLocation(const std::string& name, const std::function<GLint(const std::string&)>& locationGetter);

  /// Query and store locations for every declared uniform.
  int queryAndSetAllLocations(const std::function<GLint(const std::string&)>& locationGetter);

  /// Mark one uniform declaration dirty or clean.
  void setDirty(const std::string& name, bool isDirty);

  /// Return whether a uniform declaration must be uploaded.
  bool isDirty(const std::string& name) const;

  /// @throws If a uniform with the given name does not exist.
  const Decl& operator()(const std::string& name) const;

  /// Return every registered declaration keyed by GLSL uniform name.
  const UniformsMap& operator()() const;

  /// Return whether this registry contains a declaration with the given name.
  bool containsKey(const std::string& name) const;

  static std::string getUniformTypeString(const GLenum type);

private:
  UniformsMap m_uniformsMap;
};
