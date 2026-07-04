#include "ui/GradientBackgroundRenderer.h"

#include <glad/glad.h>
#include <imgui/imgui.h>

#include <spdlog/spdlog.h>

#include <array>
#include <cmath>

namespace entropy::ui
{
namespace
{

GLuint compileShader(GLenum type, const char* source)
{
  const GLuint shader = glCreateShader(type);
  glShaderSource(shader, 1, &source, nullptr);
  glCompileShader(shader);

  GLint compiled = GL_FALSE;
  glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
  if (compiled == GL_TRUE) {
    return shader;
  }

  std::array<char, 1024> log{};
  glGetShaderInfoLog(shader, static_cast<GLsizei>(log.size()), nullptr, log.data());
  spdlog::error("Failed to compile gradient background shader: {}", log.data());
  glDeleteShader(shader);
  return 0;
}

GLuint shaderProgram()
{
  static GLuint program = 0;
  static bool initialized = false;
  if (initialized) {
    return program;
  }
  initialized = true;

  static constexpr const char* k_vertexSource = R"GLSL(
#version 330 core
out vec2 v_ndc;
void main()
{
  vec2 positions[3] = vec2[3](
    vec2(-1.0, -1.0),
    vec2( 3.0, -1.0),
    vec2(-1.0,  3.0));
  v_ndc = positions[gl_VertexID];
  gl_Position = vec4(v_ndc, 0.0, 1.0);
}
)GLSL";

  static constexpr const char* k_fragmentSource = R"GLSL(
#version 330 core
in vec2 v_ndc;
out vec4 fragColor;

uniform vec3 u_edgeColor;
uniform vec3 u_centerColor;
uniform float u_rectangularExponent;
uniform bool u_dither;

float interleavedGradientNoise(vec2 pixel)
{
  return fract(52.9829189 * fract(dot(pixel, vec2(0.06711056, 0.00583715))));
}

void main()
{
  // A high-order superellipse distance gives a rectangular vignette without
  // the diagonal derivative seams produced by max(abs(x), abs(y)).
  vec2 p = abs(v_ndc);
  float exponent = max(u_rectangularExponent, 1.0);
  float edgeDistance = clamp(pow(pow(p.x, exponent) + pow(p.y, exponent), 1.0 / exponent), 0.0, 1.0);
  vec3 color = mix(u_centerColor, u_edgeColor, edgeDistance);

  // Dark gradients span very few values in an 8-bit default framebuffer.
  // Sub-LSB dithering hides visible contour bands without shifting target colors.
  if (u_dither) {
    float dither = (interleavedGradientNoise(gl_FragCoord.xy) - 0.5) / 255.0;
    color = clamp(color + vec3(dither), 0.0, 1.0);
  }
  fragColor = vec4(color, 1.0);
}
)GLSL";

  const GLuint vertexShader = compileShader(GL_VERTEX_SHADER, k_vertexSource);
  const GLuint fragmentShader = compileShader(GL_FRAGMENT_SHADER, k_fragmentSource);
  if (vertexShader == 0 || fragmentShader == 0) {
    if (vertexShader != 0) {
      glDeleteShader(vertexShader);
    }
    if (fragmentShader != 0) {
      glDeleteShader(fragmentShader);
    }
    return 0;
  }

  program = glCreateProgram();
  glAttachShader(program, vertexShader);
  glAttachShader(program, fragmentShader);
  glLinkProgram(program);
  glDeleteShader(vertexShader);
  glDeleteShader(fragmentShader);

  GLint linked = GL_FALSE;
  glGetProgramiv(program, GL_LINK_STATUS, &linked);
  if (linked == GL_TRUE) {
    return program;
  }

  std::array<char, 1024> log{};
  glGetProgramInfoLog(program, static_cast<GLsizei>(log.size()), nullptr, log.data());
  spdlog::error("Failed to link gradient background shader program: {}", log.data());
  glDeleteProgram(program);
  program = 0;
  return 0;
}

GLuint vertexArrayObject()
{
  static GLuint vao = 0;
  if (vao == 0) {
    glGenVertexArrays(1, &vao);
  }
  return vao;
}

} // namespace

void renderGradientBackground(const GradientBackgroundOptions& options)
{
  const GLuint program = shaderProgram();
  if (program == 0) {
    return;
  }

  GLint previousProgram = 0;
  GLint previousVertexArray = 0;
  GLint previousDrawFramebuffer = 0;
  GLint previousViewport[4] = {0, 0, 0, 0};
  const GLboolean previousScissorEnabled = glIsEnabled(GL_SCISSOR_TEST);
  const GLboolean previousDepthEnabled = glIsEnabled(GL_DEPTH_TEST);
  const GLboolean previousStencilEnabled = glIsEnabled(GL_STENCIL_TEST);
  const GLboolean previousBlendEnabled = glIsEnabled(GL_BLEND);
  const GLboolean previousCullEnabled = glIsEnabled(GL_CULL_FACE);
  glGetIntegerv(GL_CURRENT_PROGRAM, &previousProgram);
  glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &previousVertexArray);
  glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &previousDrawFramebuffer);
  glGetIntegerv(GL_VIEWPORT, previousViewport);

  const ImGuiIO& io = ImGui::GetIO();
  const glm::ivec4 viewportPx = options.viewportPx.value_or(glm::ivec4{
    0,
    0,
    static_cast<int>(std::round(io.DisplaySize.x * io.DisplayFramebufferScale.x)),
    static_cast<int>(std::round(io.DisplaySize.y * io.DisplayFramebufferScale.y))});
  if (viewportPx.z <= 0 || viewportPx.w <= 0) {
    return;
  }

  glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
  glViewport(viewportPx.x, viewportPx.y, viewportPx.z, viewportPx.w);
  glDisable(GL_SCISSOR_TEST);
  glDisable(GL_DEPTH_TEST);
  glDisable(GL_STENCIL_TEST);
  glDisable(GL_BLEND);
  glDisable(GL_CULL_FACE);
  glUseProgram(program);
  glUniform3f(
    glGetUniformLocation(program, "u_edgeColor"),
    options.edgeColor.r,
    options.edgeColor.g,
    options.edgeColor.b);
  glUniform3f(
    glGetUniformLocation(program, "u_centerColor"),
    options.centerColor.r,
    options.centerColor.g,
    options.centerColor.b);
  glUniform1f(glGetUniformLocation(program, "u_rectangularExponent"), options.rectangularExponent);
  glUniform1i(glGetUniformLocation(program, "u_dither"), options.dither ? GL_TRUE : GL_FALSE);
  glBindVertexArray(vertexArrayObject());
  glDrawArrays(GL_TRIANGLES, 0, 3);

  glBindVertexArray(static_cast<GLuint>(previousVertexArray));
  glUseProgram(static_cast<GLuint>(previousProgram));
  glBindFramebuffer(GL_DRAW_FRAMEBUFFER, static_cast<GLuint>(previousDrawFramebuffer));
  glViewport(previousViewport[0], previousViewport[1], previousViewport[2], previousViewport[3]);
  previousScissorEnabled ? glEnable(GL_SCISSOR_TEST) : glDisable(GL_SCISSOR_TEST);
  previousDepthEnabled ? glEnable(GL_DEPTH_TEST) : glDisable(GL_DEPTH_TEST);
  previousStencilEnabled ? glEnable(GL_STENCIL_TEST) : glDisable(GL_STENCIL_TEST);
  previousBlendEnabled ? glEnable(GL_BLEND) : glDisable(GL_BLEND);
  previousCullEnabled ? glEnable(GL_CULL_FACE) : glDisable(GL_CULL_FACE);
}

} // namespace entropy::ui
