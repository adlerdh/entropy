#include "rendering/utility/containers/Uniforms.h"

#include <catch2/catch_test_macros.hpp>

#include <glm/mat3x3.hpp>
#include <glm/mat4x4.hpp>

#include <stdexcept>

TEST_CASE("uniform declarations reject mismatched C++ payloads", "[rendering][shaders][uniforms]")
{
  Uniforms uniforms;
  CHECK_THROWS_AS(uniforms.insertUniform("u_matrix", UniformType::Mat3, glm::mat4{1.0f}), std::invalid_argument);
  CHECK_NOTHROW(uniforms.insertUniform("u_matrix", UniformType::Mat3, glm::mat3{1.0f}));
  CHECK_THROWS_AS(uniforms.setValue("u_matrix", glm::mat4{1.0f}), std::invalid_argument);
}

TEST_CASE("uniform sampler declarations require explicit sampler-index wrappers", "[rendering][shaders][uniforms]")
{
  Uniforms uniforms;
  CHECK_THROWS_AS(uniforms.insertUniform("u_texture", UniformType::Sampler, 3), std::invalid_argument);
  CHECK_NOTHROW(uniforms.insertUniform("u_texture", UniformType::Sampler, Uniforms::SamplerIndexType{3}));
}

TEST_CASE("uniform sets reject conflicting declarations across shader stages", "[rendering][shaders][uniforms]")
{
  Uniforms vertex;
  vertex.insertUniform("u_transform", UniformType::Mat4, glm::mat4{1.0f});
  Uniforms fragment;
  fragment.insertUniform("u_transform", UniformType::Mat3, glm::mat3{1.0f});

  CHECK_THROWS_AS(vertex.insertUniforms(fragment), std::invalid_argument);
}

TEST_CASE("duplicate uniform declarations must retain the same type", "[rendering][shaders][uniforms]")
{
  Uniforms uniforms;
  CHECK(uniforms.insertUniform("u_value", UniformType::Float, 0.0f));
  CHECK_FALSE(uniforms.insertUniform("u_value", UniformType::Float, 1.0f));
  CHECK_THROWS_AS(uniforms.insertUniform("u_value", UniformType::Int, 0), std::invalid_argument);
}

TEST_CASE("boolean-vector uniforms retain their GLSL declaration type", "[rendering][shaders][uniforms]")
{
  Uniforms uniforms;
  CHECK(uniforms.insertUniform("u_flags", UniformType::BVec2, glm::ivec2{1, 0}));
  CHECK(Uniforms::valueMatchesType(UniformType::BVec2, uniforms.value("u_flags")));
  CHECK(Uniforms::getUniformTypeString(GL_BOOL_VEC2) == "bvec2");
}
