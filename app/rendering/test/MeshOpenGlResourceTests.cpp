#include "rendering/mesh/AmbientOcclusionResources.h"
#include "rendering/mesh/MeshDdpResources.h"
#include "rendering/mesh/MeshShadowMapResources.h"

#include <catch2/catch_test_macros.hpp>

#include <glad/glad.h>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <glm/vec2.hpp>

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

TEST_CASE("mesh framebuffer resources allocate, resize, and release in an OpenGL context", "[rendering][mesh][gl]")
{
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

  CHECK(glGetError() == GL_NO_ERROR);
}
