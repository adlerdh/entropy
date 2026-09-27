#include "rendering/gl/GLBufferObject.h"
#include "rendering/gl/GLBufferTexture.h"
#include "rendering/gl/GLBufferTypes.h"
#include "rendering/gl/GLDrawTypes.h"
#include "rendering/gl/GLFrameBufferObject.h"
#include "rendering/gl/GLShader.h"
#include "rendering/gl/GLShaderProgram.h"
#include "rendering/gl/GLTexture.h"
#include "rendering/gl/GLTextureTypes.h"
#include "rendering/gl/GLVertexArrayObject.h"
#include "rendering/gl/OpenGLRenderState.h"
#include "rendering/gl/OpenGLStateGuard.h"
#include "rendering/helpers/TextureSetupHelpers.h"
#include "rendering/mesh/AmbientOcclusionResources.h"
#include "rendering/mesh/MeshDdpResources.h"
#include "rendering/mesh/MeshRenderer.h"
#include "rendering/mesh/MeshShadowMapResources.h"
#include "rendering/TextureLayout.h"
#include "rendering/ShaderPreprocessor.h"
#include "rendering/ShaderSourceSetup.h"
#include "rendering/RaycastShaderUniforms.h"

#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glad/glad.h>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <cstddef>
#include <cstdlib>
#include <limits>
#include <array>
#include <cmath>
#include <vector>

namespace mesh = rendering::mesh;

namespace
{

class HiddenOpenGlContext
{
public:
  HiddenOpenGlContext()
  {
    if (glfwInit() != GLFW_TRUE) {
      return;
    }

    glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#if defined(__APPLE__)
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
#endif
    m_window = glfwCreateWindow(32, 32, "Entropy OpenGL resource test", nullptr, nullptr);
    if (m_window == nullptr) {
      glfwTerminate();
      return;
    }

    glfwMakeContextCurrent(m_window);
    if (gladLoadGLLoader(reinterpret_cast<GLADloadproc>(glfwGetProcAddress)) == 0) {
      glfwDestroyWindow(m_window);
      m_window = nullptr;
      glfwTerminate();
    }
  }

  HiddenOpenGlContext(const HiddenOpenGlContext&) = delete;
  HiddenOpenGlContext& operator=(const HiddenOpenGlContext&) = delete;

  ~HiddenOpenGlContext()
  {
    if (m_window != nullptr) {
      glfwMakeContextCurrent(nullptr);
      glfwDestroyWindow(m_window);
      glfwTerminate();
    }
  }

  [[nodiscard]] bool ready() const noexcept
  {
    return m_window != nullptr;
  }

private:
  GLFWwindow* m_window = nullptr;
};

} // namespace

TEST_CASE(
  "Raycast acceleration matches analytic depth and preserves mesh depth handoff",
  "[rendering][gl][raycast][workflow]")
{
#if defined(__APPLE__)
  if (std::getenv("ENTROPY_TEST_GL33") == nullptr) SKIP("Interactive macOS GL worker required");
#endif
  HiddenOpenGlContext context;
  if (!context.ready()) SKIP("No OpenGL context is available on this test worker");
  const auto source = [](const std::string& name) {
    return rendering::shader_setup::loadEmbeddedShaderSource("rendering/shaders/" + name);
  };
  constexpr int n = 32;
  constexpr int extent = 16;
  constexpr std::size_t pixels = extent * extent;
  std::vector<float> ramp(n * n * n);
  std::vector<uint8_t> distances(ramp.size());
  for (std::size_t i = 0; i < ramp.size(); ++i) {
    const float z = (static_cast<float>(i / (n * n)) + 0.5f) / n;
    ramp[i] = z;
    // Conservative physical distance to the z=21mm plane, including the
    // voxel footprint. Nonzero skips exercise the accelerated path.
    distances[i] = static_cast<uint8_t>(std::max(0.0f, std::floor(std::abs(42.0f * z - 21.0f) - 2.0f)));
  }
  GLuint textures[4]{};
  glGenTextures(4, textures);
  for (int unit = 0; unit < 2; ++unit) {
    glActiveTexture(GL_TEXTURE0 + unit);
    glBindTexture(GL_TEXTURE_3D, textures[unit]);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, unit == 0 ? GL_LINEAR : GL_NEAREST);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, unit == 0 ? GL_LINEAR : GL_NEAREST);
    for (auto axis : {GL_TEXTURE_WRAP_S, GL_TEXTURE_WRAP_T, GL_TEXTURE_WRAP_R})
      glTexParameteri(GL_TEXTURE_3D, axis, GL_CLAMP_TO_EDGE);
    if (unit == 0)
      glTexImage3D(GL_TEXTURE_3D, 0, GL_R32F, n, n, n, 0, GL_RED, GL_FLOAT, ramp.data());
    else
      glTexImage3D(GL_TEXTURE_3D, 0, GL_R8UI, n, n, n, 0, GL_RED_INTEGER, GL_UNSIGNED_BYTE, distances.data());
  }
  glBindTexture(GL_TEXTURE_2D, textures[2]);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, extent, extent, 0, GL_RGBA, GL_FLOAT, nullptr);
  glBindTexture(GL_TEXTURE_2D, textures[3]);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT32F, extent, extent, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
  GLuint framebuffer = 0;
  glGenFramebuffers(1, &framebuffer);
  glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, textures[2], 0);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, textures[3], 0);
  REQUIRE(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);
  const std::array<float, 8> corners{-1, -1, 1, -1, -1, 1, 1, 1};
  GLuint vao = 0, buffer = 0;
  glGenVertexArrays(1, &vao);
  glBindVertexArray(vao);
  glGenBuffers(1, &buffer);
  glBindBuffer(GL_ARRAY_BUFFER, buffer);
  glBufferData(GL_ARRAY_BUFFER, sizeof(corners), corners.data(), GL_STATIC_DRAW);
  glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, nullptr);
  glEnableVertexAttribArray(0);
  glViewport(0, 0, extent, extent);
  glDisable(GL_BLEND);
  glDisable(GL_CULL_FACE);
  glDisable(GL_SCISSOR_TEST);
  glEnable(GL_DEPTH_TEST);
  glDepthMask(GL_TRUE);
  glClearDepth(1);
  const glm::mat4 identity{1};
  const auto worldFromTexture = glm::scale(identity, glm::vec3{14, 26, 42});
  const auto textureFromClip = glm::translate(identity, glm::vec3{0.5f}) * glm::scale(identity, glm::vec3{0.5f});
  const auto worldFromClip = worldFromTexture * textureFromClip;
  std::array<std::vector<float>, 2> colors, depths;
  for (int accelerated = 0; accelerated < 2; ++accelerated) {
    const auto vertexSource = source("RaycastIso.vs");
    const auto fragmentSource = rendering::preprocessShaderSource(
      source("RaycastIso.fs"),
      {{"SAMPLE_TEX_COORD_FUNCTION", source("functions/SampleTexCoord_Identity.glsl")},
       {"SAMPLE_IMAGE_VALUE_FUNCTION", source("functions/SampleImageValue_Identity.glsl")},
       {"RAYCAST_JUMP_DISTANCE_FUNCTION",
        source(
          accelerated ? "functions/RaycastJumpDistance_Texture.glsl"
                      : "functions/RaycastJumpDistance_Disabled.glsl")}});
    GLShader vertex("raycast reference vertex", ShaderType::Vertex, vertexSource.c_str());
    GLShader fragment("raycast reference fragment", ShaderType::Fragment, fragmentSource.c_str());
    vertex.setRegisteredUniforms(rendering::shader_setup::raycastVertexUniforms());
    fragment.setRegisteredUniforms(rendering::shader_setup::raycastFragmentUniforms(false, accelerated != 0));
    REQUIRE(vertex.isCompiled());
    REQUIRE(fragment.isCompiled());
    GLShaderProgram program("analytic raycast");
    REQUIRE(program.attachShader(vertex));
    REQUIRE(program.attachShader(fragment));
    REQUIRE(program.link());
    program.use();
    const auto loc = [&](const char* name) {
      return glGetUniformLocation(program.handle(), name);
    };
    const auto matrix = [&](const char* name, const glm::mat4& value) {
      glUniformMatrix4fv(loc(name), 1, GL_FALSE, glm::value_ptr(value));
    };
    matrix("u_view_T_clip", identity);
    matrix("u_world_T_clip", worldFromClip);
    matrix("u_tex_T_world", glm::inverse(worldFromTexture));
    matrix("u_world_T_tex", worldFromTexture);
    matrix("u_clip_T_imgTex", glm::inverse(textureFromClip));
    const glm::mat3 gradients{1.0f / n};
    glUniformMatrix3fv(loc("u_texGrads"), 1, GL_FALSE, glm::value_ptr(gradients));
    glUniform1f(loc("u_clipDepth"), -1);
    glUniform1i(loc("u_imgTex"), 0);
    glUniform1i(loc("u_jumpTex"), 1);
    glUniform3f(loc("u_imgInvDims"), 1.0f / n, 1.0f / n, 1.0f / n);
    glUniform1f(loc("u_samplingFactor"), 0.25f);
    glUniform1i(loc("u_numIsos"), 1);
    glUniform1f(loc("u_isoValues[0]"), 0.5f);
    glUniform1f(loc("u_isoOpacities[0]"), 1);
    glUniform3f(loc("u_isoColors[0]"), 0.2f, 0.4f, 0.7f);
    glUniform1f(loc("u_lightingAmbient"), 1);
    glUniform1i(loc("u_renderFrontFaces"), 1);
    glUniform1i(loc("u_renderBackFaces"), 1);
    glUniform1i(loc("u_noHitTransparent"), 1);
    glDepthFunc(GL_ALWAYS);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    colors[accelerated].resize(4 * pixels);
    depths[accelerated].resize(pixels);
    glReadPixels(0, 0, extent, extent, GL_RGBA, GL_FLOAT, colors[accelerated].data());
    glReadPixels(0, 0, extent, extent, GL_DEPTH_COMPONENT, GL_FLOAT, depths[accelerated].data());
    CHECK(glGetError() == GL_NO_ERROR);
    for (std::size_t p = 0; p < pixels; ++p) {
      CHECK(depths[accelerated][p] == Catch::Approx(0.5f).margin(0.001f));
      CHECK(colors[accelerated][4 * p] == Catch::Approx(0.2f).margin(1e-5));
      CHECK(colors[accelerated][4 * p + 3] == Catch::Approx(1.0f).margin(1e-5));
    }
  }
  for (std::size_t p = 0; p < pixels; ++p)
    CHECK(depths[0][p] == Catch::Approx(depths[1][p]).margin(0.001f));
  for (std::size_t p = 0; p < 4 * pixels; ++p)
    CHECK(colors[0][p] == Catch::Approx(colors[1][p]).margin(1e-5));

  // A mesh behind the raycast hit must fail the depth test; a nearer mesh must
  // win. This tests the shared depth-buffer contract, not a color-only image.
  GLShader meshVertex(
    "depth handoff vertex",
    ShaderType::Vertex,
    "#version 330 core\nlayout(location=0) in vec2 p; uniform float depth; void main(){gl_Position=vec4(p,depth,1);}");
  GLShader meshFragment(
    "depth handoff fragment",
    ShaderType::Fragment,
    "#version 330 core\nout vec4 color; void main(){color=vec4(1,0,0,1);}");
  Uniforms meshUniforms;
  meshUniforms.insertUniform("depth", UniformType::Float, 0.0f);
  meshVertex.setRegisteredUniforms(std::move(meshUniforms));
  GLShaderProgram meshProgram("depth handoff");
  REQUIRE(meshProgram.attachShader(meshVertex));
  REQUIRE(meshProgram.attachShader(meshFragment));
  REQUIRE(meshProgram.link());
  meshProgram.use();
  glDepthFunc(GL_LESS);
  glUniform1f(glGetUniformLocation(meshProgram.handle(), "depth"), 0.5f);
  glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
  std::array<float, 4> center{};
  glReadPixels(extent / 2, extent / 2, 1, 1, GL_RGBA, GL_FLOAT, center.data());
  CHECK(center[0] == Catch::Approx(0.2f).margin(1e-5));
  glUniform1f(glGetUniformLocation(meshProgram.handle(), "depth"), -0.5f);
  glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
  glReadPixels(extent / 2, extent / 2, 1, 1, GL_RGBA, GL_FLOAT, center.data());
  CHECK(center[0] == Catch::Approx(1.0f));
  CHECK(center[1] == Catch::Approx(0.0f));
  CHECK(glGetError() == GL_NO_ERROR);
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  glDeleteFramebuffers(1, &framebuffer);
  glDeleteBuffers(1, &buffer);
  glDeleteVertexArrays(1, &vao);
  glDeleteTextures(4, textures);
}

TEST_CASE("OpenGL wrappers reject invalid CPU-side configuration", "[rendering][gl]")
{
  GLTexture texture2d(tex::Target::Texture2D);
  CHECK_THROWS(texture2d.setSize({4u, 4u, 2u}));

  GLTexture cubeMap(tex::Target::TextureCubeMap);
  CHECK_THROWS(cubeMap.setSize({4u, 3u, 1u}));

  GLTexture bufferTexture(tex::Target::TextureBuffer);
  CHECK_THROWS(bufferTexture.setSize({4u, 1u, 1u}));

  GLTexture::PixelStoreSettings invalidPixelStore;
  invalidPixelStore.m_alignment = 3;
  CHECK_THROWS(texture2d.setPixelUnpackSettings(invalidPixelStore));

  CHECK_THROWS(GLVertexArrayObject::IndexedDrawParams(
    PrimitiveMode::Triangles,
    static_cast<std::size_t>(std::numeric_limits<GLsizei>::max()) + 1u,
    IndexType::UInt32,
    0u));

  GLBufferObject buffer(BufferType::VertexArray, BufferUsagePattern::StaticDraw);
  CHECK_THROWS(buffer.allocate(4u, nullptr));
}

TEST_CASE("mesh framebuffer and planar texture resources work in an OpenGL context", "[rendering][mesh][gl]")
{
#if defined(__APPLE__)
  if (std::getenv("ENTROPY_TEST_GL33") == nullptr) {
    SKIP("Set ENTROPY_TEST_GL33 on an interactive graphics worker to enable macOS context tests");
  }
#endif
  HiddenOpenGlContext context;
  if (!context.ready()) {
    SKIP("No OpenGL context is available on this test worker");
  }

  {
    mesh::MeshDdpResources resources;
    CHECK(resources.ensureSize(glm::uvec2{31u, 29u}));
    CHECK(resources.initialized());
    CHECK(resources.size() == glm::uvec2{31u, 29u});
    for (std::size_t orientation = 0; orientation < mesh::MeshDdpResources::k_imagePlaneCompositeCount; ++orientation) {
      resources.bindImagePlaneCompositeTarget(orientation);
      resources.imagePlaneCompositeColorTexture(orientation).bind(0u);
      resources.imagePlaneCompositeColorTexture(orientation).unbind(0u);
      resources.imagePlaneCompositeDepthTexture(orientation).bind(0u);
      resources.imagePlaneCompositeDepthTexture(orientation).unbind(0u);
      CHECK(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);
    }
    CHECK_FALSE(resources.ensureSize(glm::uvec2{31u, 29u}));
    CHECK(resources.ensureSize(glm::uvec2{17u, 19u}));
    resources.clear();
    CHECK_FALSE(resources.initialized());
  }

  {
    mesh::MeshAmbientOcclusionResources resources;
    CHECK(resources.ensureSize(glm::uvec2{31u, 29u}));
    CHECK(resources.initialized());
    CHECK(resources.size() == glm::uvec2{31u, 29u});
    CHECK(resources.ensureSize(glm::uvec2{17u, 19u}));
    resources.clear();
    CHECK_FALSE(resources.initialized());
  }

  {
    mesh::MeshShadowMapResources resources;
    CHECK(resources.ensureSize(32u));
    CHECK(resources.initialized());
    CHECK(resources.sizePixels() == 32u);
    resources.depthTexture().bind(0u);
    GLint minFilter = 0;
    GLint magFilter = 0;
    glGetTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, &minFilter);
    glGetTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, &magFilter);
    CHECK(minFilter == GL_NEAREST);
    CHECK(magFilter == GL_NEAREST);
    resources.depthTexture().unbind(0u);
    CHECK(resources.ensureSize(16u));
    resources.clear();
    CHECK_FALSE(resources.initialized());
  }

  {
    GLTexture::PixelStoreSettings pixelStore;
    pixelStore.m_alignment = 1;
    GLTexture texture(tex::Target::Texture2D, GLTexture::MultisampleSettings{}, pixelStore, pixelStore);
    texture.generate();
    texture.setSize({5u, 4u, 1u});

    std::vector<uint8_t> pixels(20u, 0u);
    texture.setData(
      0,
      GLTexture::getSizedInternalNormalizedRedFormat(ComponentType::UInt8),
      GLTexture::getBufferPixelNormalizedRedFormat(ComponentType::UInt8),
      GLTexture::getBufferPixelDataType(ComponentType::UInt8),
      pixels.data());
    texture.bind(0u);
    GLint defaultMinFilter = 0;
    glGetTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, &defaultMinFilter);
    CHECK(defaultMinFilter == GL_LINEAR);
    texture.unbind(0u);
    CHECK_THROWS(texture.setData(
      1,
      GLTexture::getSizedInternalNormalizedRedFormat(ComponentType::UInt8),
      GLTexture::getBufferPixelNormalizedRedFormat(ComponentType::UInt8),
      GLTexture::getBufferPixelDataType(ComponentType::UInt8),
      pixels.data()));
    CHECK_THROWS(texture.setSize({5u, 4u, 2u}));

    const auto region = rendering::texture_setup::textureUploadRegion(
      {.dimension = rendering::TextureDimension::Texture2D, .axes = {1, 2}},
      {1u, 5u, 4u},
      {0u, 2u, 1u},
      {1u, 2u, 2u});
    REQUIRE(region);
    const std::vector<uint8_t> update{1u, 2u, 3u, 4u};
    texture.setSubData(
      0,
      region->offset,
      region->size,
      GLTexture::getBufferPixelNormalizedRedFormat(ComponentType::UInt8),
      GLTexture::getBufferPixelDataType(ComponentType::UInt8),
      update.data());

    CHECK(glGetError() == GL_NO_ERROR);
  }

  {
    GLTexture::PixelStoreSettings pixelStore;
    pixelStore.m_alignment = 1;
    GLTexture texture(tex::Target::TextureRectangle, GLTexture::MultisampleSettings{}, pixelStore, pixelStore);
    texture.generate();
    texture.setSize({4u, 3u, 1u});
    std::vector<uint8_t> pixels(12u, 0u);
    texture.setData(
      0,
      GLTexture::getSizedInternalNormalizedRedFormat(ComponentType::UInt8),
      GLTexture::getBufferPixelNormalizedRedFormat(ComponentType::UInt8),
      GLTexture::getBufferPixelDataType(ComponentType::UInt8),
      pixels.data());
    const std::array<uint8_t, 2> update{9u, 7u};
    texture.setSubData(
      0,
      {1u, 1u, 0u},
      {2u, 1u, 1u},
      GLTexture::getBufferPixelNormalizedRedFormat(ComponentType::UInt8),
      GLTexture::getBufferPixelDataType(ComponentType::UInt8),
      update.data());
    CHECK(glGetError() == GL_NO_ERROR);
  }

  {
    GLTexture::PixelStoreSettings pixelStore;
    pixelStore.m_alignment = 1;
    GLTexture texture(tex::Target::Texture3D, GLTexture::MultisampleSettings{}, pixelStore, pixelStore);
    texture.generate();
    texture.setSize({4u, 3u, 2u});

    std::vector<uint8_t> voxels(24u, 0u);
    texture.setData(
      0,
      GLTexture::getSizedInternalNormalizedRedFormat(ComponentType::UInt8),
      GLTexture::getBufferPixelNormalizedRedFormat(ComponentType::UInt8),
      GLTexture::getBufferPixelDataType(ComponentType::UInt8),
      voxels.data());

    const auto region = rendering::texture_setup::textureUploadRegion(
      {.dimension = rendering::TextureDimension::Texture3D},
      {4u, 3u, 2u},
      {1u, 1u, 0u},
      {2u, 2u, 2u});
    REQUIRE(region);
    const std::vector<uint8_t> update(8u, 7u);
    texture.setSubData(
      0,
      region->offset,
      region->size,
      GLTexture::getBufferPixelNormalizedRedFormat(ComponentType::UInt8),
      GLTexture::getBufferPixelDataType(ComponentType::UInt8),
      update.data());

    CHECK_THROWS(texture.setSubData(
      0,
      {3u, 0u, 0u},
      {2u, 1u, 1u},
      GLTexture::getBufferPixelNormalizedRedFormat(ComponentType::UInt8),
      GLTexture::getBufferPixelDataType(ComponentType::UInt8),
      update.data()));
    CHECK(glGetError() == GL_NO_ERROR);
  }

  {
    GLBufferObject previous(BufferType::VertexArray, BufferUsagePattern::StaticDraw);
    GLBufferObject source(BufferType::VertexArray, BufferUsagePattern::DynamicDraw);
    GLBufferObject destination(BufferType::VertexArray, BufferUsagePattern::DynamicDraw);
    previous.generate();
    source.generate();
    destination.generate();
    previous.allocate(sizeof(uint32_t), nullptr);
    source.allocate(4u * sizeof(uint32_t), nullptr);
    destination.allocate(4u * sizeof(uint32_t), nullptr);

    previous.bind();
    const std::array<uint32_t, 4> values{3u, 5u, 7u, 11u};
    source.write(0u, sizeof(values), values.data());
    GLint boundBuffer = 0;
    glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &boundBuffer);
    CHECK(static_cast<GLuint>(boundBuffer) == previous.id());

    GLBufferObject::copyData(source, destination, 0u, 0u, sizeof(values));
    std::array<uint32_t, 4> copied{};
    destination.read(0u, sizeof(copied), copied.data());
    CHECK(copied == values);
    CHECK_THROWS(destination.write(destination.size(), sizeof(uint32_t), values.data()));

    auto* mappedValues = static_cast<uint32_t*>(destination.mapRange(
      0u,
      2u * sizeof(uint32_t),
      {BufferMapRangeAccessFlag::MapWriteBit, BufferMapRangeAccessFlag::FlushExplicitBit}));
    REQUIRE(mappedValues != nullptr);
    mappedValues[1] = 13u;
    destination.flushMappedRange(sizeof(uint32_t), sizeof(uint32_t));
    CHECK(destination.unmap());
    destination.read(sizeof(uint32_t), sizeof(uint32_t), copied.data());
    CHECK(copied.front() == 13u);

    const GLuint sourceId = source.id();
    source.generate();
    CHECK(source.id() == sourceId);
    previous.unbind();
    CHECK(glGetError() == GL_NO_ERROR);
  }

  {
    GLTexture previousTexture(tex::Target::TextureBuffer);
    previousTexture.generate();
    previousTexture.bind(3u);
    glActiveTexture(GL_TEXTURE5);

    GLBufferTexture bufferTexture(tex::SizedInternalBufferTextureFormat::R32F, BufferUsagePattern::DynamicDraw);
    bufferTexture.generate();
    const std::array<float, 4> values{1.0f, 2.0f, 3.0f, 4.0f};
    bufferTexture.allocate(sizeof(values), values.data());
    bufferTexture.bind(3u);

    GLint activeTexture = 0;
    glGetIntegerv(GL_ACTIVE_TEXTURE, &activeTexture);
    CHECK(activeTexture == GL_TEXTURE5);
    CHECK(bufferTexture.isBound(3u));
    CHECK_FALSE(previousTexture.isBound(3u));
    glActiveTexture(GL_TEXTURE3);
    GLint backingBuffer = 0;
    glGetTexLevelParameteriv(GL_TEXTURE_BUFFER, 0, GL_TEXTURE_BUFFER_DATA_STORE_BINDING, &backingBuffer);
    CHECK(backingBuffer != 0);
    glActiveTexture(GL_TEXTURE5);
    bufferTexture.unbind(3u);
    CHECK(glGetError() == GL_NO_ERROR);
    CHECK_THROWS(bufferTexture.allocate(sizeof(values) - 1u, values.data()));
    previousTexture.unbind(3u);
  }

  {
    GLFrameBufferObject framebuffer("wrapper lifecycle test");
    framebuffer.generate();
    const GLuint id = framebuffer.id();
    framebuffer.generate();
    CHECK(framebuffer.id() == id);
    framebuffer.destroy();
    CHECK(framebuffer.id() == 0u);
    framebuffer.destroy();
    CHECK(glGetError() == GL_NO_ERROR);
  }

  {
    GLVertexArrayObject vertexArray;
    vertexArray.generate();
    vertexArray.bind();
    CHECK_THROWS(vertexArray.setAttributeIntegerBuffer(0u, 3, BufferComponentType::Float, 0, 0u));
    CHECK_THROWS(
      vertexArray.setAttributeBuffer(0u, 3, BufferComponentType::UInt_2_10_10_10, BufferNormalizeValues::True, 0, 0u));
    CHECK_THROWS(
      vertexArray
        .setAttributeBuffer(0u, 4, BufferComponentType::UInt_10F_11F_11F, BufferNormalizeValues::False, 0, 0u));
    GLVertexArrayObject::unbind();
    CHECK(glGetError() == GL_NO_ERROR);
  }

  {
    GLTexture texture2d(tex::Target::Texture2D);
    GLTexture texture3d(tex::Target::Texture3D);
    texture2d.generate();
    texture3d.generate();
    const GLuint texture3dId = texture3d.id();
    texture2d = std::move(texture3d);
    CHECK(texture2d.target() == tex::Target::Texture3D);
    CHECK(texture2d.id() == texture3dId);
    // cppcheck-suppress accessMoved -- move assignment specifies that the source wrapper is reset
    CHECK(texture3d.id() == 0u); // NOLINT(bugprone-use-after-move)
    // cppcheck-suppress accessMoved -- querying the specified reset state is intentional
    CHECK_FALSE(texture3d.isBound()); // NOLINT(bugprone-use-after-move)
  }

  {
    GLTexture previousTexture(tex::Target::Texture2D);
    previousTexture.generate();
    previousTexture.setSize({1u, 1u, 1u});
    previousTexture.setData(
      0,
      tex::SizedInternalFormat::RGBA8_UNorm,
      tex::BufferPixelFormat::RGBA,
      tex::BufferPixelDataType::UInt8,
      nullptr);
    previousTexture.bind(2u);
    glActiveTexture(GL_TEXTURE4);
    glViewport(2, 3, 19, 23);
    glScissor(5, 7, 11, 13);
    glEnable(GL_SCISSOR_TEST);
    glEnable(GL_BLEND);
    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    glStencilFuncSeparate(GL_FRONT, GL_EQUAL, 3, 0x0fu);
    glStencilMaskSeparate(GL_FRONT, 0x33u);
    glStencilOpSeparate(GL_FRONT, GL_REPLACE, GL_INCR, GL_DECR);
    glStencilFuncSeparate(GL_BACK, GL_NOTEQUAL, 5, 0xf0u);
    glStencilMaskSeparate(GL_BACK, 0xccu);
    glStencilOpSeparate(GL_BACK, GL_ZERO, GL_INVERT, GL_KEEP);

    {
      const OpenGLStateGuard guard{{2u, GL_TEXTURE_2D}};
      glViewport(0, 0, 1, 1);
      glScissor(0, 0, 1, 1);
      glDisable(GL_SCISSOR_TEST);
      glDisable(GL_BLEND);
      glEnable(GL_DEPTH_TEST);
      glDepthMask(GL_TRUE);
      glStencilFunc(GL_ALWAYS, 0, 0xffffffffu);
      glStencilMask(0xffffffffu);
      glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
      glActiveTexture(GL_TEXTURE2);
      glBindTexture(GL_TEXTURE_2D, 0u);
      glActiveTexture(GL_TEXTURE0);
    }

    std::array<GLint, 4> viewport{};
    std::array<GLint, 4> scissor{};
    glGetIntegerv(GL_VIEWPORT, viewport.data());
    glGetIntegerv(GL_SCISSOR_BOX, scissor.data());
    CHECK(viewport == std::array<GLint, 4>{2, 3, 19, 23});
    CHECK(scissor == std::array<GLint, 4>{5, 7, 11, 13});
    CHECK(glIsEnabled(GL_SCISSOR_TEST) == GL_TRUE);
    CHECK(glIsEnabled(GL_BLEND) == GL_TRUE);
    CHECK(glIsEnabled(GL_DEPTH_TEST) == GL_FALSE);
    GLboolean depthMask = GL_TRUE;
    glGetBooleanv(GL_DEPTH_WRITEMASK, &depthMask);
    CHECK(depthMask == GL_FALSE);
    GLint frontStencilFunction = 0;
    GLint frontStencilReference = 0;
    GLint frontStencilWriteMask = 0;
    GLint backStencilFunction = 0;
    GLint backStencilReference = 0;
    GLint backStencilWriteMask = 0;
    glGetIntegerv(GL_STENCIL_FUNC, &frontStencilFunction);
    glGetIntegerv(GL_STENCIL_REF, &frontStencilReference);
    glGetIntegerv(GL_STENCIL_WRITEMASK, &frontStencilWriteMask);
    glGetIntegerv(GL_STENCIL_BACK_FUNC, &backStencilFunction);
    glGetIntegerv(GL_STENCIL_BACK_REF, &backStencilReference);
    glGetIntegerv(GL_STENCIL_BACK_WRITEMASK, &backStencilWriteMask);
    CHECK(frontStencilFunction == GL_EQUAL);
    CHECK(frontStencilReference == 3);
    CHECK(frontStencilWriteMask == 0x33);
    CHECK(backStencilFunction == GL_NOTEQUAL);
    CHECK(backStencilReference == 5);
    CHECK(backStencilWriteMask == 0xcc);
    GLint activeTexture = 0;
    glGetIntegerv(GL_ACTIVE_TEXTURE, &activeTexture);
    CHECK(activeTexture == GL_TEXTURE4);
    CHECK(previousTexture.isBound(2u));
    previousTexture.unbind(2u);
    glStencilFunc(GL_ALWAYS, 0, 0xffffffffu);
    glStencilMask(0xffffffffu);
    glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
  }

  {
    static constexpr char vertexSource[] =
      "#version 330 core\nvoid main() { gl_Position = vec4(0.0, 0.0, 0.0, 1.0); }\n";
    static constexpr char fragmentSource[] = "#version 330 core\nout vec4 color;\nvoid main() { color = vec4(1.0); }\n";
    GLShader vertexShader("wrapper vertex test", ShaderType::Vertex, vertexSource);
    GLShader fragmentShader("wrapper fragment test", ShaderType::Fragment, fragmentSource);
    REQUIRE(vertexShader.isCompiled());
    REQUIRE(fragmentShader.isCompiled());
    GLShaderProgram program("wrapper program test");
    REQUIRE(program.attachShader(vertexShader));
    REQUIRE(program.attachShader(fragmentShader));
    REQUIRE(program.link());
    program.use();
    GLint currentProgram = 0;
    glGetIntegerv(GL_CURRENT_PROGRAM, &currentProgram);
    CHECK(static_cast<GLuint>(currentProgram) == program.handle());
    GLShaderProgram::stopUse();

    static constexpr char invalidSource[] = "#version 330 core\nthis is not valid GLSL\n";
    GLShader invalidShader("invalid wrapper shader", ShaderType::Vertex, invalidSource);
    CHECK_FALSE(invalidShader.isCompiled());
    GLShaderProgram invalidProgram("invalid wrapper program");
    CHECK_FALSE(invalidProgram.attachShader(invalidShader));
  }

  CHECK(glGetError() == GL_NO_ERROR);
}

TEST_CASE("render pass baseline restores mutable OpenGL state", "[rendering][gl][state]")
{
#if defined(__APPLE__)
  if (std::getenv("ENTROPY_TEST_GL33") == nullptr) {
    SKIP("Set ENTROPY_TEST_GL33 on an interactive graphics worker to enable macOS context tests");
  }
#endif
  HiddenOpenGlContext context;
  if (!context.ready()) {
    SKIP("No OpenGL context is available on this test worker");
  }

  glDisable(GL_BLEND);
  glEnable(GL_CULL_FACE);
  glEnable(GL_DEPTH_TEST);
  glEnable(GL_SCISSOR_TEST);
  glDepthMask(GL_FALSE);
  glDepthFunc(GL_ALWAYS);
  glActiveTexture(GL_TEXTURE3);

  rendering::restoreOpenGLRenderState();

  GLint activeTexture = 0;
  GLint depthFunction = 0;
  GLboolean depthWrite = GL_FALSE;
  glGetIntegerv(GL_ACTIVE_TEXTURE, &activeTexture);
  glGetIntegerv(GL_DEPTH_FUNC, &depthFunction);
  glGetBooleanv(GL_DEPTH_WRITEMASK, &depthWrite);
  CHECK(glIsEnabled(GL_BLEND) == GL_TRUE);
  CHECK(glIsEnabled(GL_CULL_FACE) == GL_FALSE);
  CHECK(glIsEnabled(GL_DEPTH_TEST) == GL_FALSE);
  CHECK(glIsEnabled(GL_SCISSOR_TEST) == GL_FALSE);
  CHECK(glIsEnabled(GL_MULTISAMPLE) == GL_TRUE);
  CHECK(activeTexture == GL_TEXTURE0);
  CHECK(depthFunction == GL_LESS);
  CHECK(depthWrite == GL_TRUE);
}

TEST_CASE("mesh depth-only passes do not upload surface-shading uniforms", "[rendering][mesh][gl]")
{
#if defined(__APPLE__)
  if (std::getenv("ENTROPY_TEST_GL33") == nullptr) {
    SKIP("Set ENTROPY_TEST_GL33 on an interactive graphics worker to enable macOS context tests");
  }
#endif
  HiddenOpenGlContext context;
  if (!context.ready()) {
    SKIP("No OpenGL context is available on this test worker");
  }

  static constexpr char vertexSource[] = R"(
#version 330 core
uniform mat4 u_clip_T_world;
void main()
{
  gl_Position = u_clip_T_world * vec4(0.0, 0.0, 0.0, 1.0);
}
)";
  static constexpr char fragmentSource[] = R"(
#version 330 core
layout(location = 0) out vec2 outDepthBounds;
void main()
{
  outDepthBounds = vec2(-gl_FragCoord.z, gl_FragCoord.z);
}
)";

  Uniforms vertexUniforms;
  vertexUniforms.insertUniform("u_clip_T_world", UniformType::Mat4, glm::mat4{1.0f});
  GLShader vertexShader("mesh depth-only regression vertex", ShaderType::Vertex, vertexSource);
  vertexShader.setRegisteredUniforms(std::move(vertexUniforms));
  GLShader fragmentShader("mesh depth-only regression fragment", ShaderType::Fragment, fragmentSource);
  REQUIRE(vertexShader.isCompiled());
  REQUIRE(fragmentShader.isCompiled());

  GLShaderProgram program("mesh depth-only regression program");
  REQUIRE(program.attachShader(vertexShader));
  REQUIRE(program.attachShader(fragmentShader));
  REQUIRE(program.link());

  mesh::MeshDrawContext drawContext;
  drawContext.meshLookup = [](const mesh::MeshHandle&) -> const mesh::MeshGpuData* {
    return nullptr;
  };
  const std::span<const std::reference_wrapper<const mesh::MeshRenderable> > noRenderables;
  for (const mesh::MeshDrawPass pass :
       {mesh::MeshDrawPass::DepthBounds, mesh::MeshDrawPass::ShadowDepth, mesh::MeshDrawPass::AmbientOcclusionGeometry})
  {
    CHECK_NOTHROW(mesh::MeshRenderer::drawBucket(noRenderables, drawContext, program, pass));
  }
  CHECK(glGetError() == GL_NO_ERROR);
}
