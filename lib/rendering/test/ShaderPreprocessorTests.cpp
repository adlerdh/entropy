#include "rendering/ShaderPreprocessor.h"

#include <catch2/catch_test_macros.hpp>

#include <stdexcept>
#include <string>

TEST_CASE("shader preprocessor expands explicit includes and inline substitutions", "[rendering][shaders]")
{
  const rendering::ShaderReplacements replacements{
    {"HELPERS", "float saturate(float v) { return clamp(v, 0.0, 1.0); }"},
    {"SAMPLER_TYPE", "sampler3D"}};
  const std::string source = "#version 330 core\n#include \"entropy/HELPERS.glsl\"\nuniform ${SAMPLER_TYPE} u_image;\n";

  const std::string result = rendering::preprocessShaderSource(source, replacements);
  CHECK(result.find("float saturate") != std::string::npos);
  CHECK(result.find("uniform sampler3D u_image") != std::string::npos);
  CHECK(result.find("#include") == std::string::npos);
  CHECK(result.find("${") == std::string::npos);
}

TEST_CASE("shader preprocessor recursively expands Entropy includes", "[rendering][shaders]")
{
  const rendering::ShaderReplacements replacements{
    {"OUTER", "#include \"entropy/INNER.glsl\""},
    {"INNER", "float includedFunction() { return 1.0; }"}};

  CHECK(
    rendering::preprocessShaderSource("#include \"entropy/OUTER.glsl\"", replacements) ==
    "float includedFunction() { return 1.0; }");
}

TEST_CASE("shader preprocessor rejects unresolved or legacy directives", "[rendering][shaders]")
{
  const rendering::ShaderReplacements replacements;
  CHECK_THROWS_AS(
    rendering::preprocessShaderSource("#include \"entropy/MISSING.glsl\"", replacements),
    std::runtime_error);
  CHECK_THROWS_AS(rendering::preprocessShaderSource("uniform ${MISSING} u_value;", replacements), std::runtime_error);
  CHECK_THROWS_AS(rendering::preprocessShaderSource("$$HELPERS$$", replacements), std::runtime_error);
}

TEST_CASE("shader preprocessor rejects malformed and recursive includes", "[rendering][shaders]")
{
  CHECK_THROWS(rendering::preprocessShaderSource("#include \"ordinary.glsl\"", {}));
  CHECK_THROWS(
    rendering::preprocessShaderSource("#include \"entropy/LOOP.glsl\"", {{"LOOP", "#include \"entropy/LOOP.glsl\""}}));
}
