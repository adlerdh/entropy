#include "deformation/BrushStep.h"
#include "deformation/FieldDomain.h"
#include "deformation/VelocityLattice.h"
#include "reference/ReferenceFlow.h"
#include "rendering/deformation/FieldPassRunner.h"
#include "rendering/deformation/FieldTextures.h"
#include "rendering/gl/GLFrameBufferObject.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <glad/glad.h>
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <glm/geometric.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/mat3x3.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <stdexcept>
#include <vector>

namespace df = deformation;
namespace gpu = rendering::deformation;

namespace
{
constexpr std::size_t budget = std::size_t{64} * 1024 * 1024;

class Context
{
public:
  Context()
  {
    if (glfwInit() != GLFW_TRUE) {
      throw std::runtime_error("GPU deformation tests require a working GLFW display");
    }
    glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#if defined(__APPLE__)
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
#endif
    m_window = glfwCreateWindow(32, 32, "Deformation GPU tests", nullptr, nullptr);
    if (m_window != nullptr) {
      glfwMakeContextCurrent(m_window);
      if (gladLoadGLLoader(reinterpret_cast<GLADloadproc>(glfwGetProcAddress)) != 0 && GLAD_GL_VERSION_3_3 != 0) {
        return;
      }
      glfwDestroyWindow(m_window);
    }
    glfwTerminate();
    throw std::runtime_error("GPU deformation tests require OpenGL 3.3 core");
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

df::DomainGeometry geometry(std::uint32_t n = 65, double spacing = 0.25)
{
  df::DomainGeometry result;
  result.dimension = df::SpatialDimension::Plane;
  result.size = {n, n, 1};
  result.spacing = {spacing, spacing, 1};
  const double half = 0.5 * static_cast<double>(n - 1) * spacing;
  result.origin = {-half, -half, 0};
  return result;
}

df::DomainGeometry volumeGeometry(std::uint32_t n = 33, double spacing = 0.25)
{
  auto result = geometry(n, spacing);
  result.dimension = df::SpatialDimension::Volume;
  result.size[2] = n;
  result.spacing.z = spacing;
  result.origin.z = result.origin.x;
  return result;
}

std::size_t offset(const df::FieldDomain& domain, std::uint32_t x, std::uint32_t y)
{
  return static_cast<std::size_t>(y) * domain.size()[0] + x;
}

std::size_t offset3(const df::FieldDomain& domain, std::uint32_t x, std::uint32_t y, std::uint32_t z)
{
  return (static_cast<std::size_t>(z) * domain.size()[1] + y) * domain.size()[0] + x;
}

df::BrushStep brush(df::BrushMotion motion = df::PushMotion{{1, 0.3, 0}})
{
  df::BrushDefinition definition;
  definition.dimension = df::SpatialDimension::Plane;
  definition.radiusMm = 6;
  definition.motion = motion;
  return df::BrushStep(definition);
}

double endpointError(const gpu::FieldTexture& field, const df::BrushStep& step, df::MapDirection direction)
{
  const auto values = field.readback();
  double maximum = 0;
  for (std::uint32_t y = 8; y + 8 < field.domain().size()[1]; y += 7) {
    for (std::uint32_t x = 8; x + 8 < field.domain().size()[0]; x += 7) {
      const auto p = field.domain().indexToPhysical({x, y, 0});
      const auto reference =
        df::reference::integrateConverged([&step](const auto& q) { return step.velocity(q); }, p, {}, direction);
      REQUIRE(reference.converged);
      const auto value = values[offset(field.domain(), x, y)];
      REQUIRE(value.w == 1);
      maximum = std::max(maximum, glm::length(glm::dvec3(value) - (reference.pointMm - p)));
    }
  }
  return maximum;
}
} // namespace

TEST_CASE("GPU field storage and native dimension contracts", "[deformation-gpu]")
{
  const Context context;
  const df::FieldDomain domain(geometry(9));
  REQUIRE_THROWS_AS(gpu::FieldTexture(domain, domain.sampleCount() * 16 - 1), std::invalid_argument);
  gpu::FieldTexture field(domain, budget);
  REQUIRE(field.bytes() == domain.sampleCount() * 16);
  auto data = field.readback();
  REQUIRE(std::ranges::all_of(data, [](auto v) { return v == glm::vec4(0); }));
  for (std::size_t i = 0; i < data.size(); ++i) {
    data[i] = {static_cast<float>(i), -2, 0, 1};
  }
  field.upload(data);
  REQUIRE(field.readback() == data);
  REQUIRE_THROWS_AS(field.upload({}), std::invalid_argument);
  gpu::FieldPassRunner passes(budget);
  passes.identity(field);
  REQUIRE(std::ranges::all_of(field.readback(), [](auto v) { return v == glm::vec4(0, 0, 0, 1); }));
  gpu::FieldTexture copy(domain, budget);
  passes.copy(field, copy);
  REQUIRE(copy.readback() == field.readback());
  REQUIRE_THROWS_AS(passes.copy(field, field), std::invalid_argument);
  REQUIRE_THROWS_AS(passes.compose(copy, field, field), std::invalid_argument);
  auto volumeGeometry = geometry(9);
  volumeGeometry.dimension = df::SpatialDimension::Volume;
  gpu::FieldTexture volume(df::FieldDomain(volumeGeometry), budget);
  passes.identity(volume);
  REQUIRE(std::ranges::all_of(volume.readback(), [](auto v) { return v == glm::vec4(0, 0, 0, 1); }));
  gpu::FieldTexture other(df::FieldDomain(geometry(7)), budget);
  REQUIRE_THROWS_AS(passes.copy(other, copy), std::invalid_argument);
  REQUIRE_THROWS_AS(passes.exponential(field, 21), std::invalid_argument);
  REQUIRE_THROWS_AS(gpu::FieldPassRunner(1).exponential(field, 3), std::invalid_argument);
  REQUIRE(glGetError() == GL_NO_ERROR);
}

TEST_CASE("GPU framebuffer attaches exactly one allocated volume layer", "[deformation-gpu]")
{
  const Context context;
  auto spec = geometry(5);
  spec.dimension = df::SpatialDimension::Volume;
  spec.size[2] = 3;
  gpu::FieldTexture volume(df::FieldDomain(spec), budget);
  GLFrameBufferObject framebuffer("Layer test");
  framebuffer.generate();
  REQUIRE_THROWS(
    framebuffer.attachTextureLayer(fbo::TargetType::DrawAndRead, fbo::AttachmentType::Color, volume.texture(), 1, 0));
  framebuffer.bind(fbo::TargetType::DrawAndRead);
  REQUIRE_THROWS(
    framebuffer.attachTextureLayer(fbo::TargetType::DrawAndRead, fbo::AttachmentType::Color, volume.texture(), -1, 0));
  REQUIRE_THROWS(
    framebuffer.attachTextureLayer(fbo::TargetType::DrawAndRead, fbo::AttachmentType::Color, volume.texture(), 3, 0));
  REQUIRE_THROWS(
    framebuffer.attachTextureLayer(fbo::TargetType::DrawAndRead, fbo::AttachmentType::Color, volume.texture(), 1));
  REQUIRE_THROWS(
    framebuffer.attachTextureLayer(fbo::TargetType::DrawAndRead, fbo::AttachmentType::Depth, volume.texture(), 1, 0));
  gpu::FieldTexture plane(df::FieldDomain(geometry(5)), budget);
  REQUIRE_THROWS(
    framebuffer.attachTextureLayer(fbo::TargetType::DrawAndRead, fbo::AttachmentType::Color, plane.texture(), 0, 0));
  framebuffer.attachTextureLayer(fbo::TargetType::DrawAndRead, fbo::AttachmentType::Color, volume.texture(), 1, 0);
  REQUIRE(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);
  const std::array<float, 4> color{1, 2, 3, 4};
  glClearBufferfv(GL_COLOR, 0, color.data());
  const auto data = volume.readback();
  for (std::size_t i = 0; i < data.size(); ++i) {
    REQUIRE(data[i] == (i >= 25 && i < 50 ? glm::vec4(1, 2, 3, 4) : glm::vec4(0)));
  }
  REQUIRE(glGetError() == GL_NO_ERROR);
}

TEST_CASE("GPU volume passes write every layer and reject invalid trilinear contributors", "[deformation-gpu]")
{
  const Context context;
  auto spec = geometry(6, 0.7);
  spec.dimension = df::SpatialDimension::Volume;
  spec.size = {6, 5, 4};
  spec.spacing = {0.7, 1.3, 2.1};
  spec.validExtent = df::IndexExtent{{1, 1, 0}, {5, 4, 4}};
  spec.directions = glm::dmat3(glm::rotate(glm::dmat4(1), 0.53, glm::normalize(glm::dvec3(1, 2, 3))));
  spec.directions[0] *= -1;
  spec.origin = {1e6, -2e6, 3e6};
  const df::FieldDomain domain(spec);
  gpu::FieldPassRunner passes(budget);
  gpu::FieldTexture outer(domain, budget), inner(domain, budget), output(domain, budget);
  std::vector<glm::vec4> outerData(domain.sampleCount());
  std::vector<glm::vec4> innerData(domain.sampleCount());
  const glm::vec3 halfLayer = glm::vec3(domain.indexVectorToPhysical({0, 0, 0.5}));
  for (std::uint32_t z = 0; z < spec.size[2]; ++z) {
    for (std::uint32_t y = 0; y < spec.size[1]; ++y) {
      for (std::uint32_t x = 0; x < spec.size[0]; ++x) {
        const auto i = offset3(domain, x, y, z);
        outerData[i] = {0.1f * x, 0.2f * y, 0.3f * z, 1};
        innerData[i] = glm::vec4(halfLayer, 1);
      }
    }
  }
  outerData[offset3(domain, 2, 2, 2)].w = 0;
  outerData[offset3(domain, 4, 2, 2)].x = std::numeric_limits<float>::quiet_NaN();
  outer.upload(outerData);
  inner.upload(innerData);
  output.upload(std::vector<glm::vec4>(domain.sampleCount(), {99, 99, 99, 1}));
  passes.identity(output);
  const auto identity = output.readback();
  for (std::uint32_t z = 0; z < spec.size[2]; ++z) {
    REQUIRE(identity[offset3(domain, 3, 2, z)] == glm::vec4(0, 0, 0, 1));
    REQUIRE(identity[offset3(domain, 0, 2, z)] == glm::vec4(0));
  }
  passes.copy(outer, output);
  const auto copied = output.readback();
  REQUIRE(copied[offset3(domain, 3, 2, 3)] == outerData[offset3(domain, 3, 2, 3)]);
  REQUIRE(copied[offset3(domain, 2, 2, 1)] == outerData[offset3(domain, 2, 2, 1)]);
  REQUIRE(copied[offset3(domain, 2, 2, 2)].w == 0);
  REQUIRE(copied[offset3(domain, 4, 2, 2)].w == 0);
  REQUIRE(copied[offset3(domain, 0, 2, 1)].w == 0);
  passes.compose(outer, inner, output);
  const auto composed = output.readback();
  for (std::uint32_t z = 0; z < 3; ++z) {
    const auto value = composed[offset3(domain, 3, 2, z)];
    REQUIRE(value.w == 1);
    const glm::vec3 expected = halfLayer + glm::vec3(0.3f, 0.4f, 0.3f * (static_cast<float>(z) + 0.5f));
    REQUIRE(glm::length(glm::vec3(value) - expected) < 2e-6f);
  }
  REQUIRE(composed[offset3(domain, 2, 2, 1)].w == 0);
  REQUIRE(composed[offset3(domain, 4, 2, 1)].w == 0);
  REQUIRE(composed[offset3(domain, 3, 2, 3)].w == 0);
  REQUIRE(composed[offset3(domain, 0, 2, 1)].w == 0);
  REQUIRE(glGetError() == GL_NO_ERROR);
}

TEST_CASE("GPU tile composition matches a full pass and leaves other samples untouched", "[deformation-gpu]")
{
  const Context context;
  gpu::FieldPassRunner passes(budget);
  for (const bool volume : {false, true}) {
    auto spec = volume ? volumeGeometry(8, 0.75) : geometry(8, 0.75);
    spec.size = {8, 7, volume ? 5u : 1u};
    const df::FieldDomain domain(spec);
    gpu::FieldTexture outer(domain, budget), inner(domain, budget), full(domain, budget), tiled(domain, budget);
    std::vector<glm::vec4> outerData(domain.sampleCount());
    std::vector<glm::vec4> innerData(domain.sampleCount());
    for (std::uint32_t z = 0; z < spec.size[2]; ++z) {
      for (std::uint32_t y = 0; y < spec.size[1]; ++y) {
        for (std::uint32_t x = 0; x < spec.size[0]; ++x) {
          const auto index = offset3(domain, x, y, z);
          outerData[index] = {0.1f * x, -0.2f * y, volume ? 0.05f * z : 0.0f, 1};
          innerData[index] = {0.5625f, -0.1875f, volume ? 0.375f : 0.0f, 1};
        }
      }
    }
    outerData[offset3(domain, 5, 3, volume ? 2u : 0u)].w = 0;
    outer.upload(outerData);
    inner.upload(innerData);
    const glm::vec4 marker{17, 18, 19, 0};
    tiled.upload(std::vector<glm::vec4>(domain.sampleCount(), marker));
    passes.compose(outer, inner, full);
    const df::IndexExtent tile{{2, 1, volume ? 1u : 0u}, {7, 6, volume ? 4u : 1u}};
    passes.composeTile(outer, inner, tiled, tile);
    const auto expected = full.readback();
    const auto actual = tiled.readbackLayers();
    REQUIRE(actual.size() == expected.size());
    for (std::uint32_t z = 0; z < spec.size[2]; ++z) {
      for (std::uint32_t y = 0; y < spec.size[1]; ++y) {
        for (std::uint32_t x = 0; x < spec.size[0]; ++x) {
          const auto index = offset3(domain, x, y, z);
          const bool inside = x >= tile.begin[0] && x < tile.end[0] && y >= tile.begin[1] && y < tile.end[1] &&
                              z >= tile.begin[2] && z < tile.end[2];
          REQUIRE(actual[index] == (inside ? expected[index] : marker));
        }
      }
    }
    REQUIRE_THROWS(passes.composeTile(outer, inner, tiled, {{0, 0, 0}, {9, 7, 1}}));
  }
  REQUIRE(glGetError() == GL_NO_ERROR);
}

TEST_CASE("GPU singleton-axis volumes preserve paired cancellation and complete sweeps", "[deformation-gpu]")
{
  const Context context;
  auto spec = geometry(17, 0.8);
  spec.dimension = df::SpatialDimension::Volume;
  spec.size = {17, 17, 1};
  spec.spacing = {0.8, 1.4, 2.2};
  const df::FieldDomain domain(spec);
  gpu::FieldPassRunner passes(budget);
  gpu::FieldTexture velocity(domain, budget);
  const glm::vec4 shift{0.2f, 0.0f, 0.0f, 1.0f};
  velocity.upload(std::vector<glm::vec4>(domain.sampleCount(), shift));
  gpu::FieldTexture seeded(domain, budget);
  passes.seed(velocity, 1.0f, seeded);
  REQUIRE(seeded.readback()[offset3(domain, 8, 8, 0)] == shift);
  auto pair = passes.exponential(velocity, 4);
  REQUIRE(pair.forward);
  REQUIRE(pair.inverse);
  REQUIRE(pair.forward->readback()[offset3(domain, 8, 8, 0)].w == 1);
  REQUIRE(glm::length(glm::vec3(pair.forward->readback()[offset3(domain, 8, 8, 0)]) - glm::vec3(shift)) < 2e-6f);
  REQUIRE(glm::length(glm::vec3(pair.inverse->readback()[offset3(domain, 8, 8, 0)]) + glm::vec3(shift)) < 2e-6f);
  gpu::FieldTexture quality(domain, budget);
  passes.quality(*pair.forward, *pair.inverse, quality);
  REQUIRE(std::ranges::all_of(quality.readback(), [](auto value) { return value.w == 0; }));
  REQUIRE(passes.reduceQuality(quality).requested == 0);
  const auto original = pair.forward->readback();
  int polls = 0;
  const auto cancelled = passes.exponential(velocity, 4, [&polls] { return ++polls == 4; });
  REQUIRE_FALSE(cancelled.forward);
  REQUIRE_FALSE(cancelled.inverse);
  polls = 0;
  const auto cancelledAccumulation = passes.accumulate(pair, pair, [&polls] { return ++polls == 2; });
  REQUIRE_FALSE(cancelledAccumulation.forward);
  REQUIRE_FALSE(cancelledAccumulation.inverse);
  REQUIRE(pair.forward->readback() == original);
  REQUIRE(glGetError() == GL_NO_ERROR);
}

TEST_CASE("GPU cubic volume velocity reproduces oblique physical brush flows", "[deformation-gpu]")
{
  const Context context;
  auto spec = volumeGeometry(21, 0.4);
  spec.directions = glm::dmat3(glm::rotate(glm::dmat4(1), 0.57, glm::normalize(glm::dvec3(1, 2, 3))));
  spec.directions[0] *= -1;
  spec.origin = glm::dvec3(1e6, -2e6, 3e6) - spec.directions * glm::dvec3(4);
  const df::FieldDomain domain(spec);
  df::BrushDefinition recipe;
  recipe.dimension = df::SpatialDimension::Volume;
  recipe.directions = spec.directions;
  recipe.centerMm = domain.indexToPhysical({10, 10, 10});
  recipe.radiusMm = 3.5;
  recipe.strength = 0.8;
  recipe.protection.push_back({domain.indexToPhysical({12, 10, 10}), 0.5, 0.7});
  gpu::FieldPassRunner passes(budget);
  gpu::FieldTexture output(domain, budget);
  for (const df::BrushMotion& motion : std::vector<df::BrushMotion>{
         df::PushMotion{spec.directions * glm::dvec3(0.7, -0.2, 0.3)},
         df::RadialMotion{0.4},
         df::TwirlMotion{spec.directions[2], 0.6}})
  {
    recipe.motion = motion;
    const df::BrushStep step(recipe);
    const df::VelocityLattice lattice(step, 0.8);
    passes.velocity(lattice, output);
    const auto data = output.readback();
    for (std::uint32_t z = 3; z < 19; z += 5) {
      for (std::uint32_t y = 3; y < 19; y += 5) {
        for (std::uint32_t x = 3; x < 19; x += 5) {
          const auto p = domain.indexToPhysical({x, y, z});
          const auto value = data[offset3(domain, x, y, z)];
          REQUIRE(value.w == 1);
          REQUIRE(glm::length(glm::dvec3(value) - lattice.velocity(p)) < 2e-5);
          REQUIRE(glm::length(lattice.velocity(p) - step.velocity(p)) < 1e-8);
        }
      }
    }
  }
  REQUIRE(glGetError() == GL_NO_ERROR);
}

TEST_CASE("GPU volume exponentials track forward and inverse double precision flows", "[deformation-gpu]")
{
  const Context context;
  const df::FieldDomain domain(volumeGeometry());
  gpu::FieldPassRunner passes(budget);
  gpu::FieldTexture velocity(domain, budget);
  df::BrushDefinition recipe;
  recipe.dimension = df::SpatialDimension::Volume;
  recipe.radiusMm = 3.5;
  for (const df::BrushMotion& motion : std::vector<df::BrushMotion>{
         df::PushMotion{{0.7, -0.2, 0.3}},
         df::RadialMotion{0.4},
         df::TwirlMotion{{0, 0, 1}, 0.6}})
  {
    recipe.motion = motion;
    const df::BrushStep step(recipe);
    passes.velocity(df::VelocityLattice(step, 0.8), velocity);
    auto pair = passes.exponential(velocity, 8);
    for (const auto direction : {df::MapDirection::Forward, df::MapDirection::Inverse}) {
      const auto values = (direction == df::MapDirection::Forward ? pair.forward : pair.inverse)->readback();
      for (std::uint32_t z = 12; z <= 20; z += 4) {
        for (std::uint32_t y = 12; y <= 20; y += 4) {
          for (std::uint32_t x = 12; x <= 20; x += 4) {
            const auto p = domain.indexToPhysical({x, y, z});
            const auto reference =
              df::reference::integrateConverged([&step](const auto& q) { return step.velocity(q); }, p, {}, direction);
            REQUIRE(reference.converged);
            const auto value = values[offset3(domain, x, y, z)];
            REQUIRE(value.w == 1);
            REQUIRE(glm::length(glm::dvec3(value) - (reference.pointMm - p)) < 0.025);
          }
        }
      }
    }
  }
  REQUIRE(glGetError() == GL_NO_ERROR);
}

TEST_CASE("GPU volume Jacobians and inverse residuals use the intrinsic physical frame", "[deformation-gpu]")
{
  const Context context;
  auto spec = volumeGeometry(11, 0.6);
  spec.spacing = {0.6, 1.1, 1.7};
  spec.directions = glm::dmat3(glm::rotate(glm::dmat4(1), 0.45, glm::normalize(glm::dvec3(2, 1, 3))));
  spec.directions[1] *= -1;
  spec.origin = glm::dvec3(1e6, -2e6, 3e6) - spec.directions * (spec.spacing * glm::dvec3(5));
  const df::FieldDomain domain(spec);
  const glm::dmat3 linear({1.1, 0.08, 0.02}, {-0.12, 0.93, 0.04}, {0.01, 0.03, 1.05});
  const glm::dmat3 inverseLinear = glm::inverse(linear);
  const glm::dvec3 translation(0.1, -0.2, 0.15);
  std::vector<glm::vec4> forwardData(domain.sampleCount());
  std::vector<glm::vec4> inverseData(domain.sampleCount());
  for (std::uint32_t z = 0; z < spec.size[2]; ++z) {
    for (std::uint32_t y = 0; y < spec.size[1]; ++y) {
      for (std::uint32_t x = 0; x < spec.size[0]; ++x) {
        const glm::dvec3 local = (glm::dvec3(x, y, z) - glm::dvec3(5)) * spec.spacing;
        const auto i = offset3(domain, x, y, z);
        forwardData[i] = glm::vec4(spec.directions * (linear * local + translation - local), 1);
        inverseData[i] = glm::vec4(spec.directions * (inverseLinear * (local - translation) - local), 1);
      }
    }
  }
  gpu::FieldTexture forward(domain, budget), inverse(domain, budget), output(domain, budget);
  forward.upload(forwardData);
  inverse.upload(inverseData);
  gpu::FieldPassRunner passes(budget);
  const auto intrinsic = df::analyzeJacobian(linear, df::SpatialDimension::Volume);
  const auto sampled = passes.analyzeDirection(forward, inverse);
  REQUIRE(sampled.requested == 9 * 9 * 9);
  REQUIRE(sampled.evaluated > 0);
  REQUIRE(sampled.minSingularValue <= intrinsic.minSingularValue + 1e-4);
  REQUIRE(sampled.maxSingularValue >= intrinsic.maxSingularValue - 1e-4);
  for (const bool reverse : {false, true}) {
    passes.quality(reverse ? inverse : forward, reverse ? forward : inverse, output);
    const auto reduced = passes.reduceQuality(output);
    REQUIRE(reduced.requested == 9 * 9 * 9);
    REQUIRE(reduced.evaluated > 0);
    REQUIRE(reduced.outside == reduced.requested - reduced.evaluated);
    REQUIRE(reduced.nonFinite == 0);
    REQUIRE(
      reduced.minDeterminant ==
      Catch::Approx(reverse ? 1.0 / glm::determinant(linear) : glm::determinant(linear)).margin(1e-5));
    const auto values = output.readback();
    REQUIRE(values[offset3(domain, 0, 5, 5)].w == 0);
    REQUIRE(values[offset3(domain, 5, 5, 0)].w == 0);
    for (std::uint32_t z = 3; z <= 7; z += 2) {
      for (std::uint32_t y = 3; y <= 7; y += 2) {
        for (std::uint32_t x = 3; x <= 7; x += 2) {
          const auto value = values[offset3(domain, x, y, z)];
          REQUIRE(value.w == 1);
          REQUIRE(
            value.x == Catch::Approx(reverse ? 1.0 / glm::determinant(linear) : glm::determinant(linear)).margin(1e-5));
          REQUIRE(value.y < 1e-5f);
          REQUIRE(value.z < 1e-5f);
        }
      }
    }
  }
  REQUIRE(glGetError() == GL_NO_ERROR);
}

TEST_CASE("GPU quality reduction preserves counts and extrema across 2D and 3D pyramids", "[deformation-gpu]")
{
  const Context context;
  gpu::FieldPassRunner passes(budget);
  for (const bool volume : {false, true}) {
    auto spec = volume ? volumeGeometry(11, 0.5) : geometry(19, 0.5);
    if (volume) {
      spec.size = {11, 9, 7};
    }
    else {
      spec.size = {19, 13, 1};
      spec.validExtent = df::IndexExtent{{1, 1, 0}, {18, 12, 1}};
    }
    const df::FieldDomain domain(spec);
    gpu::FieldTexture qualityMap(domain, budget);
    std::vector<glm::vec4> data(domain.sampleCount(), {1.25f, 0.2f, 0.4f, 1});
    const auto at = [&](std::uint32_t x, std::uint32_t y, std::uint32_t z = 0) {
      return offset3(domain, x, y, z);
    };
    const std::uint32_t z = volume ? 3 : 0;
    data[at(3, 3, z)] = {-0.2f, 0.1f, 0.3f, 1};
    data[at(4, 3, z)] = {3.5f, 2.0f, 4.0f, 1};
    data[at(5, 3, z)].w = 0;
    data[at(6, 3, z)].x = std::numeric_limits<float>::quiet_NaN();
    data[at(0, 0, 0)] = {-100.0f, 100.0f, 100.0f, 1}; // Outside the requested stencil interior.
    qualityMap.upload(data);
    const auto reduced = passes.reduceQuality(qualityMap);
    const std::size_t requested = volume ? 9 * 7 * 5 : 15 * 9;
    REQUIRE(reduced.requested == requested);
    REQUIRE(reduced.evaluated == requested - 2);
    REQUIRE(reduced.outside == 2);
    REQUIRE(reduced.nonFinite == 1);
    REQUIRE(reduced.minDeterminant == Catch::Approx(-0.2).margin(1e-7));
    REQUIRE(reduced.maxDeterminant == Catch::Approx(3.5));
    REQUIRE(reduced.maxResidualMm == Catch::Approx(2.0));
    REQUIRE(reduced.maxResidualVoxels == Catch::Approx(4.0));
    REQUIRE_THROWS_AS(gpu::FieldPassRunner(1).reduceQuality(qualityMap), std::invalid_argument);
  }
  REQUIRE(glGetError() == GL_NO_ERROR);
}

TEST_CASE("GPU sampled directional evidence supplies conservative stretch bounds", "[deformation-gpu]")
{
  const Context context;
  gpu::FieldPassRunner passes(budget);
  for (const bool volume : {false, true}) {
    const df::FieldDomain domain(volume ? volumeGeometry(9, 0.5) : geometry(9, 0.5));
    gpu::FieldPair pair{
      std::make_unique<gpu::FieldTexture>(domain, budget),
      std::make_unique<gpu::FieldTexture>(domain, budget)};
    passes.identity(*pair.forward);
    passes.identity(*pair.inverse);
    const auto direction = passes.analyzeDirection(*pair.forward, *pair.inverse);
    const std::size_t expected = volume ? 7 * 7 * 7 : 7 * 7;
    REQUIRE(direction.requested == expected);
    REQUIRE(direction.evaluated == expected);
    REQUIRE(direction.outside == 0);
    REQUIRE(direction.finite);
    REQUIRE(direction.minDeterminant == Catch::Approx(1.0));
    REQUIRE(direction.maxDeterminant == Catch::Approx(1.0));
    REQUIRE(direction.minSingularValue > 0.6);
    REQUIRE(direction.minSingularValue < 1.0);
    REQUIRE(direction.maxSingularValue > 1.0);
    REQUIRE(direction.maxSingularValue < 2.0);
    REQUIRE(direction.maxResidualMm == 0);
    REQUIRE(direction.maxResidualVoxels == 0);
    const auto report = passes.sampledReport(pair);
    REQUIRE(report.forward.requested == expected);
    REQUIRE(report.inverse.requested == expected);
    REQUIRE_FALSE(report.protectionChecked);
    REQUIRE_FALSE(report.convergenceChecked);
    REQUIRE_FALSE(report.cellsVerified);
    const auto cells = passes.verifyCells(pair, {});
    REQUIRE(cells.complete());
    REQUIRE(cells.forward.requested == (volume ? 8 * 8 * 8 : 8 * 8));
    REQUIRE(df::assessCandidate(report).decision == df::CandidateDecision::Refine);
    REQUIRE(df::assessCandidate(report).reason == df::QualityReason::MissingEvidence);
  }
  REQUIRE(glGetError() == GL_NO_ERROR);
}

TEST_CASE("GPU candidate acceptance publishes only complete verified pairs", "[deformation-gpu]")
{
  const Context context;
  gpu::FieldPassRunner passes(budget);
  for (const bool volume : {false, true}) {
    auto spec = volume ? volumeGeometry(9, 0.5) : geometry(9, 0.5);
    spec.spacing = {0.5, 0.75, volume ? 1.25 : 1.0};
    spec.directions = glm::dmat3(glm::rotate(glm::dmat4(1), 0.37, glm::normalize(glm::dvec3(1, 2, 3))));
    spec.directions[0] *= -1;
    spec.origin = {1e6, -2e6, 3e6};
    const df::FieldDomain domain(spec);
    gpu::FieldTexture velocity(domain, budget);
    passes.identity(velocity);
    gpu::FieldPair previous{
      std::make_unique<gpu::FieldTexture>(domain, budget),
      std::make_unique<gpu::FieldTexture>(domain, budget)};
    passes.identity(*previous.forward);
    passes.identity(*previous.inverse);
    const auto originalForward = previous.forward->readback();
    const auto originalInverse = previous.inverse->readback();
    const df::ProtectedRegion core{domain.indexToPhysical({4, 4, volume ? 4.0 : 0.0}), 0.2, 1.0};
    const auto zeroCore = passes.precheckProtectedCores(previous, std::span(&core, 1));
    REQUIRE(zeroCore.checked);
    REQUIRE(zeroCore.exactlyZero);
    const auto accepted = passes.acceptVelocity(velocity, &previous, std::span(&core, 1), {}, 2, 2);
    REQUIRE(accepted.assessment.decision == df::CandidateDecision::Accept);
    REQUIRE(accepted.attempts == 1);
    REQUIRE(accepted.pair.forward);
    REQUIRE(accepted.pair.inverse);
    REQUIRE(accepted.cells.complete());
    REQUIRE(accepted.protection.checked);
    REQUIRE(accepted.refinement.checked());
    REQUIRE(accepted.report.cellsVerified);
    REQUIRE(accepted.report.protectionChecked);
    REQUIRE(accepted.report.convergenceChecked);
    REQUIRE(previous.forward->readback() == originalForward);
    REQUIRE(previous.inverse->readback() == originalInverse);

    const auto canceled = passes.acceptVelocity(velocity, &previous, {}, {}, 2, 2, [] { return true; });
    REQUIRE(canceled.canceled);
    REQUIRE_FALSE(canceled.pair.forward);
    REQUIRE_FALSE(canceled.pair.inverse);
    REQUIRE(previous.forward->readback() == originalForward);

    auto invalid = velocity.readback();
    invalid[offset3(domain, 4, 4, volume ? 4 : 0)].w = 0;
    velocity.upload(invalid);
    const auto rejected = passes.acceptVelocity(velocity, &previous, {}, {}, 2, 2);
    REQUIRE(rejected.assessment.decision != df::CandidateDecision::Accept);
    REQUIRE_FALSE(rejected.pair.forward);
    REQUIRE(previous.forward->readback() == originalForward);
    REQUIRE(previous.inverse->readback() == originalInverse);

    df::BrushDefinition moving;
    moving.dimension = volume ? df::SpatialDimension::Volume : df::SpatialDimension::Plane;
    moving.centerMm = core.centerMm;
    moving.directions = spec.directions;
    moving.radiusMm = 1.0;
    moving.motion = df::PushMotion{0.04 * spec.directions[0]};
    passes.velocity(df::VelocityLattice(df::BrushStep(moving), 0.5), velocity);
    const auto movingPair = passes.exponential(velocity, 2);
    const auto movingCore = passes.precheckProtectedCores(movingPair, std::span(&core, 1));
    REQUIRE(movingCore.checked);
    REQUIRE_FALSE(movingCore.exactlyZero);
    const auto unprotected = passes.acceptVelocity(velocity, &previous, std::span(&core, 1), {}, 2, 2);
    REQUIRE(unprotected.assessment.decision != df::CandidateDecision::Accept);
    REQUIRE(unprotected.assessment.reason == df::QualityReason::Protection);
    REQUIRE_FALSE(unprotected.pair.forward);
    REQUIRE(previous.forward->readback() == originalForward);
  }
  REQUIRE(glGetError() == GL_NO_ERROR);
}

TEST_CASE("GPU compact refinement matches physical CPU center and cell comparisons", "[deformation-gpu]")
{
  const Context context;
  gpu::FieldPassRunner passes(budget);
  for (bool volume : {false, true}) {
    auto spec = volume ? volumeGeometry(9, 0.5) : geometry(9, 0.5);
    spec.spacing = {0.5, 0.75, volume ? 1.25 : 1.0};
    spec.directions = glm::dmat3(glm::rotate(glm::dmat4(1), 0.37, glm::normalize(glm::dvec3(1, 2, 3))));
    spec.directions[0] *= -1;
    spec.origin = {1e6, -2e6, 3e6};
    const df::FieldDomain domain(spec);
    gpu::FieldPair first{
      std::make_unique<gpu::FieldTexture>(domain, budget),
      std::make_unique<gpu::FieldTexture>(domain, budget)};
    gpu::FieldPair second{
      std::make_unique<gpu::FieldTexture>(domain, budget),
      std::make_unique<gpu::FieldTexture>(domain, budget)};
    for (auto* field : {first.forward.get(), first.inverse.get(), second.forward.get(), second.inverse.get()}) {
      passes.identity(*field);
    }
    auto values = second.inverse->readback();
    values[offset3(domain, 4, 4, volume ? 4 : 0)] = glm::vec4(0.1 * domain.directions()[0], 1.0);
    second.inverse->upload(values);
    const auto cpu = passes.compareRefinement(first, second);
    const auto compact = passes.reduceRefinement(first, second);
    REQUIRE(cpu.checked());
    REQUIRE(compact.checked());
    REQUIRE(compact.requested == cpu.requested);
    REQUIRE(compact.maxErrorMm >= cpu.maxErrorMm - 1e-7);
    REQUIRE(compact.maxErrorMm < cpu.maxErrorMm + 1e-4);
    values[offset3(domain, 4, 4, volume ? 4 : 0)].w = 0;
    second.inverse->upload(values);
    const auto missing = passes.reduceRefinement(first, second);
    REQUIRE_FALSE(missing.checked());
    REQUIRE(missing.unavailable > 0);
  }
  REQUIRE(glGetError() == GL_NO_ERROR);
}

TEST_CASE("GPU accepted overlapping motion keeps the prior pair intact on failure", "[deformation-gpu]")
{
  const Context context;
  gpu::FieldPassRunner passes(budget);
  for (bool volume : {false, true}) {
    const df::FieldDomain domain(volume ? volumeGeometry(17, 0.5) : geometry(17, 0.5));
    df::BrushDefinition recipe;
    recipe.dimension = volume ? df::SpatialDimension::Volume : df::SpatialDimension::Plane;
    recipe.radiusMm = 2.5;
    recipe.motion = df::PushMotion{{0.02, 0.008, volume ? 0.004 : 0.0}};
    const df::VelocityLattice lattice(df::BrushStep(recipe), 0.5);
    gpu::FieldTexture velocity(domain, budget);
    passes.velocity(lattice, velocity);
    gpu::FieldPair accepted;
    for (int step = 0; step < 20; ++step) {
      const auto before = accepted.forward ? accepted.forward->readback() : std::vector<glm::vec4>{};
      auto next = passes.acceptVelocity(velocity, accepted.forward ? &accepted : nullptr, {}, {}, 3, 3);
      INFO(
        "dimension=" << (volume ? 3 : 2) << " step=" << step << " reason=" << static_cast<int>(next.assessment.reason));
      if (step < 5) REQUIRE(next.assessment.decision == df::CandidateDecision::Accept);
      if (next.assessment.decision == df::CandidateDecision::Accept) {
        REQUIRE(next.pair.forward);
        accepted = std::move(next.pair);
      }
      else {
        REQUIRE_FALSE(next.pair.forward);
        REQUIRE(accepted.forward->readback() == before);
      }
    }
    const auto beforeForward = accepted.forward->readback();
    const auto beforeInverse = accepted.inverse->readback();
    df::QualityPolicy strict;
    strict.maxResidualMm = 0.0;
    strict.maxResidualVoxels = 0.0;
    const auto exhausted = passes.acceptVelocity(velocity, &accepted, {}, strict, 3, 1);
    REQUIRE(exhausted.assessment.decision != df::CandidateDecision::Accept);
    REQUIRE_FALSE(exhausted.pair.forward);
    REQUIRE(accepted.forward->readback() == beforeForward);
    REQUIRE(accepted.inverse->readback() == beforeInverse);
    const auto canceled = passes.acceptVelocity(velocity, &accepted, {}, {}, 3, 3, [] { return true; });
    REQUIRE(canceled.canceled);
    REQUIRE_FALSE(canceled.pair.forward);
    REQUIRE(accepted.forward->readback() == beforeForward);
    REQUIRE(accepted.inverse->readback() == beforeInverse);
  }
  REQUIRE(glGetError() == GL_NO_ERROR);
}

TEST_CASE("GPU reduction sums more than one million samples from exact compact tiles", "[deformation-gpu]")
{
  const Context context;
  const df::FieldDomain domain(volumeGeometry(129, 0.25));
  gpu::FieldTexture qualityMap(domain, budget);
  qualityMap.upload(std::vector<glm::vec4>(domain.sampleCount(), {1.0f, 0.0f, 0.0f, 1.0f}));
  gpu::FieldPassRunner passes(budget);
  const auto reduced = passes.reduceQuality(qualityMap);
  REQUIRE(reduced.requested == std::size_t{127} * 127 * 127);
  REQUIRE(reduced.evaluated == reduced.requested);
  REQUIRE(reduced.outside == 0);
  REQUIRE(reduced.nonFinite == 0);
  REQUIRE(reduced.minDeterminant == 1.0);
  REQUIRE(reduced.maxDeterminant == 1.0);
  REQUIRE(glGetError() == GL_NO_ERROR);
}

TEST_CASE("GPU lattice velocity agrees off grid in an oblique reflected plane", "[deformation-gpu]")
{
  const Context context;
  auto spec = geometry(35, 0.31);
  spec.directions = glm::dmat3(glm::rotate(glm::dmat4(1), 0.63, glm::normalize(glm::dvec3(1, 2, 3))));
  spec.directions[0] *= -1;
  spec.origin = glm::dvec3(1e6, -2e6, 3e6);
  const df::FieldDomain domain(spec);
  df::BrushDefinition recipe;
  recipe.dimension = df::SpatialDimension::Plane;
  recipe.directions = spec.directions;
  recipe.centerMm = domain.indexToPhysical({17.3, 15.7, 0});
  recipe.radiusMm = 4;
  recipe.strength = 0.7;
  recipe.protection.push_back({domain.indexToPhysical({19.1, 18.2, 0}), 0.6, 0.5});
  gpu::FieldPassRunner passes(budget);
  gpu::FieldTexture field(domain, budget);
  for (const df::BrushMotion& motion : std::vector<df::BrushMotion>{
         df::PushMotion{spec.directions * glm::dvec3(1, -0.4, 0)},
         df::RadialMotion{-0.4},
         df::TwirlMotion{spec.directions[2], 0.7}})
  {
    recipe.motion = motion;
    const df::BrushStep step(recipe);
    const df::VelocityLattice lattice(step, 0.73);
    passes.velocity(lattice, field);
    const auto values = field.readback();
    double maximum = 0;
    for (std::uint32_t y = 0; y < spec.size[1]; ++y) {
      for (std::uint32_t x = 0; x < spec.size[0]; ++x) {
        const auto p = domain.indexToPhysical({x, y, 0});
        const auto value = values[offset(domain, x, y)];
        REQUIRE(value.w == 1);
        maximum = std::max(maximum, glm::length(glm::dvec3(value) - lattice.velocity(p)));
        REQUIRE(glm::length(lattice.velocity(p) - step.velocity(p)) < 1e-8);
      }
    }
    INFO("Maximum velocity discrepancy (mm): " << maximum);
    REQUIRE(maximum < 2e-6);
  }
  REQUIRE(glGetError() == GL_NO_ERROR);
}

TEST_CASE("GPU translation composition and unknown coverage", "[deformation-gpu]")
{
  const Context context;
  auto spec = geometry(17, 0.5);
  spec.spacing.y = 2;
  spec.validExtent = df::IndexExtent{{1, 1, 0}, {16, 16, 1}};
  const df::FieldDomain domain(spec);
  gpu::FieldPassRunner passes(budget);
  gpu::FieldTexture velocity(domain, budget);
  velocity.upload(std::vector<glm::vec4>(domain.sampleCount(), {1, 0, 0, 1}));
  auto pair = passes.exponential(velocity, 4);
  const auto forward = pair.forward->readback();
  const auto inverse = pair.inverse->readback();
  REQUIRE(forward[offset(domain, 8, 8)] == glm::vec4(1, 0, 0, 1));
  REQUIRE(inverse[offset(domain, 8, 8)] == glm::vec4(-1, 0, 0, 1));
  REQUIRE(forward[offset(domain, 15, 8)].w == 0);
  REQUIRE(inverse[offset(domain, 1, 8)].w == 0);
  REQUIRE(forward[offset(domain, 0, 8)].w == 0);
  gpu::FieldTexture quality(domain, budget);
  passes.quality(*pair.forward, *pair.inverse, quality);
  REQUIRE(quality.readback()[offset(domain, 8, 8)] == glm::vec4(1, 0, 0, 1));
  REQUIRE(quality.readback()[offset(domain, 1, 8)].w == 0);
  REQUIRE(glGetError() == GL_NO_ERROR);
}

TEST_CASE("GPU radial and twirl exponentials converge toward the double oracle", "[deformation-gpu]")
{
  const Context context;
  gpu::FieldPassRunner passes(budget);
  for (const df::BrushMotion& motion : std::vector<df::BrushMotion>{
         df::PushMotion{{1, 0.3, 0}},
         df::RadialMotion{0.4},
         df::RadialMotion{-0.4},
         df::TwirlMotion{{0, 0, 1}, 0.8}})
  {
    const auto step = brush(motion);
    const df::VelocityLattice lattice(step, 1.1);
    std::array<double, 3> errors{};
    for (std::size_t refinement = 0; refinement < errors.size(); ++refinement) {
      const auto factor = static_cast<std::uint32_t>(1U << refinement);
      const df::FieldDomain domain(geometry(64 * factor + 1, 0.25 / factor));
      gpu::FieldTexture velocity(domain, budget);
      passes.velocity(lattice, velocity);
      auto pair = passes.exponential(velocity, 8);
      errors[refinement] = std::max(
        endpointError(*pair.forward, step, df::MapDirection::Forward),
        endpointError(*pair.inverse, step, df::MapDirection::Inverse));
      gpu::FieldTexture quality(domain, budget);
      passes.quality(*pair.forward, *pair.inverse, quality);
      double residual = 0;
      for (const auto value : quality.readback()) {
        if (value.w == 1) {
          REQUIRE(value.x > 0.3f);
          residual = std::max(residual, static_cast<double>(value.y));
        }
      }
      INFO(
        "Grid spacing: " << domain.spacing().x << "; endpoint error: " << errors[refinement]
                         << "; inverse residual: " << residual);
      REQUIRE(errors[refinement] < 0.012);
      REQUIRE(residual < 0.015);
    }
    INFO("Coarse/fine errors: " << errors[0] << ", " << errors[1] << ", " << errors[2]);
    REQUIRE(errors[2] < errors[0] * 0.6);
  }
  REQUIRE(glGetError() == GL_NO_ERROR);
}

TEST_CASE("GPU protected cores stay fixed through integration", "[deformation-gpu]")
{
  const Context context;
  auto recipe = brush().definition();
  recipe.protection.push_back({{0, 0, 0}, 1.2, 0.7});
  const df::BrushStep step(recipe);
  const df::FieldDomain domain(geometry());
  gpu::FieldTexture velocity(domain, budget);
  gpu::FieldPassRunner passes(budget);
  passes.velocity(df::VelocityLattice(step, 1.3), velocity);
  auto pair = passes.exponential(velocity, 8);
  for (const auto* field : {pair.forward.get(), pair.inverse.get()}) {
    const auto data = field->readback();
    for (std::uint32_t y = 0; y < domain.size()[1]; ++y) {
      for (std::uint32_t x = 0; x < domain.size()[0]; ++x) {
        if (glm::length(domain.indexToPhysical({x, y, 0})) <= 1.2) {
          REQUIRE(data[offset(domain, x, y)] == glm::vec4(0, 0, 0, 1));
        }
      }
    }
  }
  const double coarseError = endpointError(*pair.forward, step, df::MapDirection::Forward);
  // The 0.7 mm protection transition is under-resolved by a 0.25 mm field.
  // Refine the field, rather than hiding the error with extra squaring steps.
  gpu::FieldTexture fineVelocity(df::FieldDomain(geometry(257, 0.0625)), budget);
  passes.velocity(df::VelocityLattice(step, 1.3), fineVelocity);
  const auto fine = passes.exponential(fineVelocity, 8);
  const double fineError = endpointError(*fine.forward, step, df::MapDirection::Forward);
  INFO("Protected transition error, coarse/fine (mm): " << coarseError << ", " << fineError);
  REQUIRE(fineError < 0.01);
  REQUIRE(fineError < coarseError * 0.6);
}

TEST_CASE("GPU paired accumulation preserves composition order and cancellation", "[deformation-gpu]")
{
  const Context context;
  const df::FieldDomain domain(geometry(129, 0.125));
  gpu::FieldPassRunner passes(budget);
  gpu::FieldTexture velocity(domain, budget);
  const auto push = brush();
  const auto twirl = brush(df::TwirlMotion{{0, 0, 1}, 0.8});
  passes.velocity(df::VelocityLattice(push, 1), velocity);
  auto first = passes.exponential(velocity, 8);
  passes.velocity(df::VelocityLattice(twirl, 1), velocity);
  auto increment = passes.exponential(velocity, 8);
  auto accumulated = passes.accumulate(first, increment);
  accumulated = passes.accumulate(accumulated, increment);
  const std::array<df::reference::VectorFunction, 3> flows{
    [&push](auto p) { return push.velocity(p); },
    [&twirl](auto p) { return twirl.velocity(p); },
    [&twirl](auto p) {
      return twirl.velocity(p);
    }};
  for (const auto direction : {df::MapDirection::Forward, df::MapDirection::Inverse}) {
    const auto data = (direction == df::MapDirection::Forward ? accumulated.forward : accumulated.inverse)->readback();
    for (std::uint32_t x = 48; x <= 80; x += 8) {
      const auto p = domain.indexToPhysical({x, 64, 0});
      const auto expected = df::reference::composeFlows(flows, p, 256, direction) - p;
      REQUIRE(data[offset(domain, x, 64)].w == 1);
      REQUIRE(glm::length(glm::dvec3(data[offset(domain, x, 64)]) - expected) < 0.02);
    }
  }
  const auto original = first.forward->readback();
  int polls = 0;
  const auto cancelled = passes.exponential(velocity, 8, [&polls] { return ++polls == 4; });
  REQUIRE_FALSE(cancelled.forward);
  REQUIRE_FALSE(cancelled.inverse);
  polls = 0;
  const auto cancelledAccumulation = passes.accumulate(first, increment, [&polls] { return ++polls == 2; });
  REQUIRE_FALSE(cancelledAccumulation.forward);
  REQUIRE_FALSE(cancelledAccumulation.inverse);
  REQUIRE(first.forward->readback() == original);
  REQUIRE(glGetError() == GL_NO_ERROR);
}

TEST_CASE("GPU diagnostics expose folds nonfinite vectors and incomplete stencils", "[deformation-gpu]")
{
  const Context context;
  const df::FieldDomain domain(geometry(17, 0.5));
  gpu::FieldPassRunner passes(budget);
  gpu::FieldTexture forward(domain, budget), inverse(domain, budget), output(domain, budget);
  std::vector<glm::vec4> data(domain.sampleCount(), {0, 0, 0, 1});
  for (std::uint32_t y = 0; y < domain.size()[1]; ++y) {
    for (std::uint32_t x = 0; x < domain.size()[0]; ++x) {
      // F reflects x; F is its own inverse and has determinant -1.
      data[offset(domain, x, y)].x = static_cast<float>(-2 * domain.indexToPhysical({x, y, 0}).x);
    }
  }
  forward.upload(data);
  inverse.upload(data);
  passes.quality(forward, inverse, output);
  REQUIRE(output.readback()[offset(domain, 8, 8)] == glm::vec4(-1, 0, 0, 1));
  REQUIRE(output.readback()[0].w == 0);
  const auto folded = passes.analyzeDirection(forward, inverse);
  df::QualityReport foldedReport;
  foldedReport.forward = folded;
  foldedReport.inverse = folded;
  REQUIRE(df::assessCandidate(foldedReport).reason == df::QualityReason::Folding);
  data[offset(domain, 8, 8)].x = std::numeric_limits<float>::quiet_NaN();
  data[offset(domain, 7, 8)].z = 0.1f;
  forward.upload(data);
  passes.copy(forward, output);
  REQUIRE(output.readback()[offset(domain, 8, 8)].w == 0);
  REQUIRE(output.readback()[offset(domain, 7, 8)].w == 0);
  REQUIRE(output.readback()[offset(domain, 9, 8)].w == 1);
  passes.quality(forward, inverse, output);
  REQUIRE(output.readback()[offset(domain, 8, 8)].w == 0);
  REQUIRE(output.readback()[offset(domain, 9, 8)].w == 0);
  REQUIRE(glGetError() == GL_NO_ERROR);
}

TEST_CASE("GPU numerical passes isolate hostile renderer and transfer state", "[deformation-gpu]")
{
  const Context context;
  const df::FieldDomain domain(geometry(9));
  gpu::FieldTexture input(domain, budget), output(domain, budget);
  gpu::FieldPassRunner passes(budget);
  GLuint sampler = 0;
  GLuint buffer = 0;
  glGenSamplers(1, &sampler);
  glSamplerParameteri(sampler, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_NEAREST);
  glBindSampler(0, sampler);
  glGenBuffers(1, &buffer);
  glBindBuffer(GL_PIXEL_PACK_BUFFER, buffer);
  glBindBuffer(GL_PIXEL_UNPACK_BUFFER, buffer);
  glBufferData(GL_PIXEL_PACK_BUFFER, 16, nullptr, GL_STATIC_DRAW);
  glPixelStorei(GL_PACK_ROW_LENGTH, 23);
  glPixelStorei(GL_UNPACK_SKIP_PIXELS, 5);
  glPixelStorei(GL_PACK_SWAP_BYTES, GL_TRUE);
  glEnable(GL_BLEND);
  glDisablei(GL_BLEND, 1);
  glEnable(GL_SCISSOR_TEST);
  glScissor(0, 0, 0, 0);
  glEnable(GL_RASTERIZER_DISCARD);
  glEnable(GL_FRAMEBUFFER_SRGB);
  glEnable(GL_CLIP_DISTANCE0);
  glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
  glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
  glColorMaski(1, GL_TRUE, GL_FALSE, GL_TRUE, GL_FALSE);
  glViewport(3, 4, 5, 6);
  glActiveTexture(GL_TEXTURE5);
  const std::vector<glm::vec4> data(domain.sampleCount(), {0.5f, 0, 0, 1});
  input.upload(data);
  passes.copy(input, output);
  REQUIRE(output.readback() == data);
  REQUIRE(passes.reduceQuality(output).evaluated == 7 * 7);
  gpu::FieldTexture constructed(domain, budget);
  passes.identity(constructed);
  REQUIRE(std::ranges::all_of(constructed.readback(), [](auto v) { return v == glm::vec4(0, 0, 0, 1); }));
  REQUIRE_THROWS_AS(passes.compose(input, output, input), std::invalid_argument);
  auto tinyGeometry = geometry(3, 1e-50);
  gpu::FieldTexture tiny(df::FieldDomain(tinyGeometry), budget);
  REQUIRE_THROWS_AS(passes.identity(tiny), std::invalid_argument);
  int polls = 0;
  const auto cancelled = passes.exponential(input, 3, [&polls] { return ++polls == 3; });
  REQUIRE_FALSE(cancelled.forward);
  GLint value = 0;
  glGetIntegerv(GL_ACTIVE_TEXTURE, &value);
  REQUIRE(value == GL_TEXTURE5);
  glGetIntegerv(GL_PIXEL_PACK_BUFFER_BINDING, &value);
  REQUIRE(value == static_cast<GLint>(buffer));
  glGetIntegerv(GL_PIXEL_UNPACK_BUFFER_BINDING, &value);
  REQUIRE(value == static_cast<GLint>(buffer));
  glGetIntegerv(GL_PACK_ROW_LENGTH, &value);
  REQUIRE(value == 23);
  glGetIntegerv(GL_UNPACK_SKIP_PIXELS, &value);
  REQUIRE(value == 5);
  glGetIntegerv(GL_PACK_SWAP_BYTES, &value);
  REQUIRE(value == GL_TRUE);
  glGetIntegeri_v(GL_SAMPLER_BINDING, 0, &value);
  REQUIRE(value == static_cast<GLint>(sampler));
  std::array<GLint, 4> viewport{};
  glGetIntegerv(GL_VIEWPORT, viewport.data());
  REQUIRE(viewport == std::array<GLint, 4>{3, 4, 5, 6});
  REQUIRE(glIsEnabled(GL_BLEND) == GL_TRUE);
  REQUIRE(glIsEnabledi(GL_BLEND, 1) == GL_FALSE);
  std::array<GLboolean, 4> mask{};
  glGetBooleani_v(GL_COLOR_WRITEMASK, 1, mask.data());
  REQUIRE(mask == std::array<GLboolean, 4>{GL_TRUE, GL_FALSE, GL_TRUE, GL_FALSE});
  REQUIRE(glIsEnabled(GL_SCISSOR_TEST) == GL_TRUE);
  REQUIRE(glIsEnabled(GL_RASTERIZER_DISCARD) == GL_TRUE);
  REQUIRE(glIsEnabled(GL_FRAMEBUFFER_SRGB) == GL_TRUE);
  REQUIRE(glIsEnabled(GL_CLIP_DISTANCE0) == GL_TRUE);
  glDeleteBuffers(1, &buffer);
  glDeleteSamplers(1, &sampler);
  REQUIRE(glGetError() == GL_NO_ERROR);
}

TEST_CASE("GPU physical Jacobians and inverse residuals respect oblique anisotropic geometry", "[deformation-gpu]")
{
  const Context context;
  auto spec = geometry(33, 0.5);
  spec.spacing.y = 1.25;
  spec.directions = glm::dmat3(glm::rotate(glm::dmat4(1), 0.6, glm::normalize(glm::dvec3(1, 2, 3))));
  spec.directions[0] *= -1;
  spec.origin = {1e6, -2e6, 3e6};
  const df::FieldDomain domain(spec);
  const glm::dmat3 linear({1.1, 0.15, 0}, {-0.2, 0.9, 0}, {0, 0, 1});
  const auto inverseLinear = glm::inverse(linear);
  const auto center = domain.indexToPhysical({16, 16, 0});
  const glm::dvec3 translation(0.3, -0.1, 0);
  std::vector<glm::vec4> forwardData(domain.sampleCount());
  std::vector<glm::vec4> inverseData(domain.sampleCount());
  for (std::uint32_t y = 0; y < spec.size[1]; ++y) {
    for (std::uint32_t x = 0; x < spec.size[0]; ++x) {
      const glm::dvec3 local(
        (static_cast<double>(x) - 16) * spec.spacing.x,
        (static_cast<double>(y) - 16) * spec.spacing.y,
        0);
      forwardData[offset(domain, x, y)] = glm::vec4(spec.directions * (linear * local + translation - local), 1);
      inverseData[offset(domain, x, y)] =
        glm::vec4(spec.directions * (inverseLinear * (local - translation) - local), 1);
    }
  }
  gpu::FieldTexture forward(domain, budget), inverse(domain, budget), output(domain, budget);
  forward.upload(forwardData);
  inverse.upload(inverseData);
  gpu::FieldPassRunner passes(budget);
  const auto referenceJacobian = df::reference::physicalJacobian(
    [&](const auto& p) {
      return center + spec.directions * (linear * (glm::inverse(spec.directions) * (p - center)) + translation);
    },
    center,
    domain,
    0.1);
  const double determinant =
    referenceJacobian[0][0] * referenceJacobian[1][1] - referenceJacobian[0][1] * referenceJacobian[1][0];
  for (const bool reverse : {false, true}) {
    passes.quality(reverse ? inverse : forward, reverse ? forward : inverse, output);
    const auto data = output.readback();
    for (std::uint32_t y = 12; y <= 20; y += 4) {
      for (std::uint32_t x = 12; x <= 20; x += 4) {
        const auto value = data[offset(domain, x, y)];
        REQUIRE(value.w == 1);
        REQUIRE(value.x == Catch::Approx(reverse ? 1.0 / determinant : determinant).margin(2e-6));
        REQUIRE(value.y < 3e-6f);
        REQUIRE(value.z < 5e-6f);
      }
    }
  }
  REQUIRE(glGetError() == GL_NO_ERROR);
}

TEST_CASE("GPU integration refinement reduces seed error without changing field resolution", "[deformation-gpu]")
{
  const Context context;
  gpu::FieldPassRunner passes(budget);
  const auto step = brush(df::TwirlMotion{{0, 0, 1}, 1.0});
  gpu::FieldTexture velocity(df::FieldDomain(geometry(129, 0.125)), budget);
  passes.velocity(df::VelocityLattice(step, 0.9), velocity);
  std::array<double, 3> errors{};
  const std::array<unsigned, 3> squarings{0, 4, 8};
  for (std::size_t i = 0; i < squarings.size(); ++i) {
    auto pair = passes.exponential(velocity, squarings[i]);
    errors[i] = endpointError(*pair.forward, step, df::MapDirection::Forward);
  }
  INFO("Seed/refined endpoint errors (mm): " << errors[0] << ", " << errors[1] << ", " << errors[2]);
  REQUIRE(errors[1] < errors[0] * 0.15);
  REQUIRE(errors[2] < errors[1] * 0.5);
  REQUIRE(glGetError() == GL_NO_ERROR);
}
