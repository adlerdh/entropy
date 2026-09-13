#include "common/Types.h"
#include "rendering/TextureLayout.h"
#include "rendering/gl/GLBufferTypes.h"
#include "rendering/gl/GLDrawTypes.h"
#include "rendering/gl/GLShaderType.h"
#include "rendering/gl/GLTextureTypes.h"
#include "rendering/gl/GLUniformTypes.h"
#include "rendering/gl/Uniforms.h"
#include "rendering/mesh/AmbientOcclusionResources.h"
#include "rendering/mesh/MeshDdpResources.h"
#include "rendering/mesh/MeshGpuData.h"
#include "rendering/mesh/MeshHandle.h"
#include "rendering/mesh/MeshRenderable.h"
#include "rendering/mesh/MeshRenderer.h"
#include "rendering/mesh/MeshShadowMapResources.h"
#include "rendering/helpers/TextureSetupHelpers.h"
#include "rendering/gl/GLBufferObject.h"
#include "rendering/gl/GLBufferTexture.h"
#include "rendering/gl/GLFrameBufferObject.h"
#include "rendering/gl/GLShader.h"
#include "rendering/gl/GLShaderProgram.h"
#include "rendering/gl/GLTexture.h"
#include "rendering/gl/OpenGLStateGuard.h"
#include "rendering/gl/OpenGLRenderState.h"
#include "rendering/gl/GLVertexArrayObject.h"

#include <catch2/catch_test_macros.hpp>

#include <glad/glad.h>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <initializer_list>
#include <limits>
#include <optional>
#include <set>
#include <span>
#include <utility>
#include <vector>

namespace mesh = rendering::mesh;

namespace
{

#if !defined(__APPLE__)
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
#endif

} // namespace

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
  SKIP("Headless GLFW initialization can deadlock in non-interactive macOS test workers");
#else
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
#endif
}

TEST_CASE("render pass baseline restores mutable OpenGL state", "[rendering][gl][state]")
{
#if defined(__APPLE__)
  SKIP("Headless GLFW initialization can deadlock in non-interactive macOS test workers");
#else
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
#endif
}

TEST_CASE("mesh depth-only passes do not upload surface-shading uniforms", "[rendering][mesh][gl]")
{
#if defined(__APPLE__)
  SKIP("Headless GLFW initialization can deadlock in non-interactive macOS test workers");
#else
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
#endif
}
