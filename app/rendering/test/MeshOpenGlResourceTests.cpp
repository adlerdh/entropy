#include "rendering/mesh/AmbientOcclusionResources.h"
#include "rendering/mesh/MeshDdpResources.h"
#include "rendering/mesh/MeshShadowMapResources.h"
#include "rendering/helpers/TextureSetupHelpers.h"
#include "rendering/utility/gl/GLTexture.h"

#include <catch2/catch_test_macros.hpp>

#include <glad/glad.h>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <glm/vec2.hpp>

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

  CHECK(glGetError() == GL_NO_ERROR);
#endif
}
