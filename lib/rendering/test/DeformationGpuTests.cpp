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

std::size_t offset(const df::FieldDomain& domain, std::uint32_t x, std::uint32_t y)
{
  return static_cast<std::size_t>(y) * domain.size()[0] + x;
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
  REQUIRE_THROWS_AS(passes.identity(volume), std::invalid_argument);
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
