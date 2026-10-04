#include "logic/app/DeformationEditBackend.h"

#include <catch2/catch_test_macros.hpp>
#include <glad/glad.h>
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <cstddef>
#include <memory>
#include <new>
#include <stdexcept>

namespace df = deformation;
namespace edit = deformation_edit;

namespace
{
constexpr std::size_t budget = std::size_t{64} * 1024 * 1024;

class Context
{
public:
  Context()
  {
    if (glfwInit() != GLFW_TRUE) throw std::runtime_error("GL deformation edit tests require a display");
    glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#if defined(__APPLE__)
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
#endif
    m_window = glfwCreateWindow(32, 32, "Deformation edit tests", nullptr, nullptr);
    if (m_window) {
      glfwMakeContextCurrent(m_window);
      if (gladLoadGLLoader(reinterpret_cast<GLADloadproc>(glfwGetProcAddress)) != 0 && GLAD_GL_VERSION_3_3 != 0) return;
      glfwDestroyWindow(m_window);
    }
    glfwTerminate();
    throw std::runtime_error("GL deformation edit tests require OpenGL 3.3 core");
  }
  ~Context()
  {
    glfwDestroyWindow(m_window);
    glfwTerminate();
  }
  Context(const Context&) = delete;
  Context& operator=(const Context&) = delete;

private:
  GLFWwindow* m_window = nullptr;
};

df::FieldDomain domain()
{
  df::DomainGeometry geometry;
  geometry.dimension = df::SpatialDimension::Plane;
  geometry.size = {17, 17, 1};
  geometry.spacing = {0.5, 0.5, 1};
  geometry.origin = {-4, -4, 0};
  return df::FieldDomain(geometry);
}

df::FieldDomain volumeDomain()
{
  df::DomainGeometry geometry;
  geometry.dimension = df::SpatialDimension::Volume;
  geometry.size = {17, 17, 17};
  geometry.spacing = {0.5, 0.5, 0.5};
  geometry.origin = {-4, -4, -4};
  return df::FieldDomain(geometry);
}

std::shared_ptr<const df::FieldCheckpoint> identity(const df::FieldDomain& field)
{
  auto checkpoint = std::make_shared<df::FieldCheckpoint>();
  checkpoint->forward.assign(field.sampleCount(), {0, 0, 0, 1});
  checkpoint->inverse.assign(field.sampleCount(), {0, 0, 0, 1});
  return checkpoint;
}

edit::Dependencies dependencies()
{
  edit::Dependencies live;
  live.provenance = {"source", "reference", "baseline", 0};
  live.sourcePixelRevision = 1;
  live.sourceGeometryRevision = 1;
  live.referenceGeometryRevision = 1;
  live.baselineRevision = 1;
  return live;
}

df::BrushDefinition brush()
{
  df::BrushDefinition recipe;
  recipe.dimension = df::SpatialDimension::Plane;
  recipe.radiusMm = 2.5;
  recipe.motion = df::PushMotion{{0.02, 0.008, 0}};
  return recipe;
}

edit::DeformationEditSession session(df::QualityPolicy policy = {})
{
  const auto field = domain();
  return edit::DeformationEditSession(
    df::EditHistory(field, field, dependencies().provenance, policy.version, identity(field)),
    policy);
}
} // namespace

TEST_CASE("GL edit completion publishes a complete pair only at stroke end", "[deformation-edit-gpu]")
{
  const Context context;
  auto live = dependencies();
  auto state = session();
  rendering::deformation::FieldWorkspace workspace(budget + 100'000);
  edit::DeformationEditBackend backend(workspace, budget, 0.5, 3, 3);
  edit::DeformationEditController controller(state, backend, [&] { return live; });
  const auto root = state.active();
  REQUIRE(controller.beginStroke());
  REQUIRE(controller.appendStep(brush()) == edit::Completion::Provisional);
  REQUIRE(state.active() == root);
  REQUIRE(state.preview());
  REQUIRE(state.preview()->acceptedRevision == root->id());
  REQUIRE(controller.endStroke());
  const auto accepted = state.active();
  REQUIRE(accepted->id() != root->id());
  REQUIRE(accepted->checkpoint().forward != root->checkpoint().forward);
  REQUIRE(accepted->checkpoint().inverse != root->checkpoint().inverse);
  REQUIRE(controller.undo());
  REQUIRE(state.active() == root);
  REQUIRE(controller.redo(accepted->id()));
  REQUIRE(state.active() == accepted);
  REQUIRE(workspace.usedBytes() == 0);

  REQUIRE(state.beginStroke(live));
  const auto pending = state.prepareStep(brush(), live);
  REQUIRE(pending);
  auto result = backend.evaluate(*pending);
  ++live.sourcePixelRevision;
  REQUIRE(state.complete(pending, std::move(result), live) == edit::Completion::Stale);
  REQUIRE(state.active() == accepted);
}

TEST_CASE("GL edit workspace exhaustion preserves the accepted revision", "[deformation-edit-gpu]")
{
  const Context context;
  auto live = dependencies();
  auto state = session();
  const auto root = state.active();
  rendering::deformation::FieldWorkspace workspace(budget);
  edit::DeformationEditBackend backend(workspace, budget, 0.5, 3, 3);
  edit::DeformationEditController controller(state, backend, [&] { return live; });
  REQUIRE(controller.beginStroke());
  REQUIRE_THROWS_AS(controller.appendStep(brush()), std::bad_alloc);
  REQUIRE_FALSE(state.strokeActive());
  REQUIRE(state.active() == root);
  REQUIRE(workspace.usedBytes() == 0);
}

TEST_CASE("A 3D edit fits its declared full-domain workspace without resolution reduction", "[deformation-edit-gpu]")
{
  const Context context;
  auto live = dependencies();
  const auto field = volumeDomain();
  edit::DeformationEditSession state(df::EditHistory(field, field, live.provenance, {1}, identity(field)), {});
  const std::size_t fieldBytes = field.sampleCount() * sizeof(glm::vec4);
  rendering::deformation::FieldWorkspace workspace(budget + 5 * fieldBytes);
  edit::DeformationEditBackend backend(workspace, budget, 0.5, 3, 3);
  edit::DeformationEditController controller(state, backend, [&] { return live; });
  auto recipe = brush();
  recipe.dimension = df::SpatialDimension::Volume;
  recipe.motion = df::PushMotion{{0.02, 0.008, 0.004}};
  REQUIRE(controller.beginStroke());
  REQUIRE(controller.appendStep(recipe) == edit::Completion::Provisional);
  REQUIRE(controller.endStroke());
  REQUIRE(state.active()->checkpoint().forward.size() == field.sampleCount());
  REQUIRE(state.active()->checkpoint().inverse.size() == field.sampleCount());
  REQUIRE(workspace.usedBytes() == 0);
}
