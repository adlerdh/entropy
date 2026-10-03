#include "rendering/deformation/FieldPassRunner.h"

#include "rendering/ShaderPreprocessor.h"
#include "rendering/deformation/NumericalState.h"
#include "rendering/gl/GLFrameBufferObject.h"
#include "rendering/gl/GLShader.h"
#include "rendering/gl/GLShaderProgram.h"
#include "rendering/gl/GLVertexArrayObject.h"
#include "rendering/gl/Uniforms.h"

#include <cmrc/cmrc.hpp>
#include <glm/geometric.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/matrix.hpp>

#include <array>
#include <algorithm>
#include <cmath>
#include <iterator>
#include <limits>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

CMRC_DECLARE(shaders);

namespace rendering::deformation
{
namespace
{
using ::deformation::FieldDomain;
using ::deformation::SpatialDimension;

float narrow(double value)
{
  if (!std::isfinite(value) || std::abs(value) > std::numeric_limits<float>::max()) {
    throw std::invalid_argument("Geometry or velocity cannot be represented in float32");
  }
  const auto result = static_cast<float>(value);
  if (value != 0.0 && result == 0.0f) {
    throw std::invalid_argument("Geometry underflows float32");
  }
  return result;
}
glm::vec3 narrow(const glm::dvec3& v)
{
  return {narrow(v.x), narrow(v.y), narrow(v.z)};
}
glm::mat3 narrow(const glm::dmat3& m)
{
  return {narrow(m[0]), narrow(m[1]), narrow(m[2])};
}
glm::dmat3 worldToIndex(const FieldDomain& domain)
{
  glm::dmat3 scale(1.0);
  for (int i = 0; i < 3; ++i) {
    scale[i][i] = 1.0 / domain.spacing()[i];
  }
  return scale * glm::inverse(domain.directions());
}
void requirePlane(const FieldDomain& domain)
{
  if (domain.dimension() != SpatialDimension::Plane) {
    throw std::invalid_argument("Stage 2 GPU passes require an explicit native-2D domain");
  }
}
void compatible(const FieldTexture& input, const FieldTexture& output)
{
  requirePlane(input.domain());
  requirePlane(output.domain());
  const auto& a = input.domain();
  const auto& b = output.domain();
  if (
    a.size() != b.size() || a.origin() != b.origin() || a.spacing() != b.spacing() ||
    a.directions() != b.directions() || a.validExtent().begin != b.validExtent().begin ||
    a.validExtent().end != b.validExtent().end)
  {
    throw std::invalid_argument("Stage 2 composition requires identical field grids");
  }
  if (input.texture().id() == output.texture().id()) {
    throw std::invalid_argument("A numerical pass may not sample its output texture");
  }
}
void requirePair(const FieldPair& pair)
{
  if (!pair.forward || !pair.inverse) {
    throw std::invalid_argument("Both field directions are required");
  }
  compatible(*pair.forward, *pair.inverse);
}
std::string shaderText(const std::string& path)
{
  const auto file = cmrc::shaders::get_filesystem().open("rendering/shaders/" + path);
  return {file.begin(), file.end()};
}
std::unique_ptr<GLShaderProgram> program(const char* fragment)
{
  detail::NumericalState state;
  const auto vertexSource = shaderText("mesh/FullScreenTriangle.vs");
  const auto fragmentSource = preprocessShaderSource(
    shaderText(std::string("deformation/") + fragment),
    {{"FIELD_SAMPLING", shaderText("deformation/FieldSampling.glsl")}});
  const GLShader vertex("Deformation triangle", ShaderType::Vertex, vertexSource.c_str());
  GLShader pixel(fragment, ShaderType::Fragment, fragmentSource.c_str());
  Uniforms uniforms;
  // One shared declaration set; each pass optimizes away the values it does not use.
  for (const auto* name : {"u_begin", "u_end"}) {
    uniforms.insertUniform(name, UniformType::IVec2, glm::ivec2(0), false);
  }
  for (const auto* name : {"u_worldToIndex", "u_directions", "u_latticeFromWorld"}) {
    uniforms.insertUniform(name, UniformType::Mat3, glm::mat3(1), false);
  }
  for (const auto* name : {"u_spacing", "u_latticeOrigin", "u_center"}) {
    uniforms.insertUniform(name, UniformType::Vec3, glm::vec3(0), false);
  }
  for (const auto* name : {"u_first", "u_second"}) {
    uniforms.insertUniform(name, UniformType::Sampler, Uniforms::SamplerIndexType{0}, false);
  }
  uniforms.insertUniform("u_scale", UniformType::Float, 1.0f, false);
  uniforms.insertUniform("u_radius", UniformType::Float, 1.0f, false);
  uniforms.insertUniform("u_identity", UniformType::Bool, false, false);
  uniforms.insertUniform("u_protectionCount", UniformType::Int, 0, false);
  uniforms.insertUniform("u_protection", UniformType::Vec4Vector, std::vector<glm::vec4>(32, glm::vec4(0)), false);
  uniforms.insertUniform("u_transition", UniformType::FloatVector, std::vector<float>(32, 1.0f), false);
  pixel.setRegisteredUniforms(std::move(uniforms));
  auto result = std::make_unique<GLShaderProgram>(fragment);
  if (
    !vertex.isCompiled() || !pixel.isCompiled() || !result->attachShader(vertex) || !result->attachShader(pixel) ||
    !result->link())
  {
    throw std::runtime_error(std::string("Cannot compile deformation shader: ") + fragment);
  }
  return result;
}
void uniform(GLuint p, const char* name, const glm::vec3& value)
{
  glUniform3fv(glGetUniformLocation(p, name), 1, glm::value_ptr(value));
}
void uniform(GLuint p, const char* name, const glm::mat3& value)
{
  glUniformMatrix3fv(glGetUniformLocation(p, name), 1, GL_FALSE, glm::value_ptr(value));
}
} // namespace

class FieldPassRunner::Impl
{
public:
  explicit Impl(std::size_t bytes)
    : budget(bytes)
    , flow(program("FlowSeed.fs"))
    , compose(program("ComposeDisplacement.fs"))
    , velocity(program("VelocityEvaluate.fs"))
    , quality(program("Quality.fs"))
  {
    detail::NumericalState state;
    framebuffer.generate();
    vao.generate();
  }

  void reserve(const FieldTexture& field, std::size_t count) const
  {
    if (field.bytes() > budget / count) {
      throw std::invalid_argument("Deformation workspace budget exceeded");
    }
  }

  // The attachment is about to be written by GL, so output is logically mutable.
  // cppcheck-suppress constParameterReference
  void prepare(GLShaderProgram& shader, FieldTexture& output)
  {
    requirePlane(output.domain());
    const auto& domain = output.domain();
    // Validate float conversions before issuing a draw. Shader coordinates never contain the world origin.
    const auto spacing = narrow(domain.spacing());
    const auto directions = narrow(domain.directions());
    const auto indexMatrix = narrow(worldToIndex(domain));
    const auto physicalSize = domain.spacing() * glm::dvec3(domain.size()[0], domain.size()[1], 0);
    static_cast<void>(narrow(physicalSize));
    framebuffer.bind(fbo::TargetType::DrawAndRead);
    framebuffer.attach2DTexture(fbo::TargetType::DrawAndRead, fbo::AttachmentType::Color, output.texture(), 0);
    glDrawBuffer(GL_COLOR_ATTACHMENT0);
    glReadBuffer(GL_COLOR_ATTACHMENT0);
    glViewport(0, 0, static_cast<GLsizei>(domain.size()[0]), static_cast<GLsizei>(domain.size()[1]));
    vao.bind();
    shader.use();
    const GLuint p = shader.handle();
    for (GLuint unit = 0; unit < 2; ++unit) {
      glActiveTexture(GL_TEXTURE0 + unit);
      glBindTexture(GL_TEXTURE_2D, 0);
    }
    glUniform1i(glGetUniformLocation(p, "u_first"), 0);
    glUniform1i(glGetUniformLocation(p, "u_second"), 1);
    glUniform2i(
      glGetUniformLocation(p, "u_begin"),
      static_cast<GLint>(domain.validExtent().begin[0]),
      static_cast<GLint>(domain.validExtent().begin[1]));
    glUniform2i(
      glGetUniformLocation(p, "u_end"),
      static_cast<GLint>(domain.validExtent().end[0]),
      static_cast<GLint>(domain.validExtent().end[1]));
    uniform(p, "u_spacing", spacing);
    uniform(p, "u_directions", directions);
    uniform(p, "u_worldToIndex", indexMatrix);
  }

  static void draw()
  {
    glDrawArrays(GL_TRIANGLES, 0, 3);
    // Do not retain deleted scratch storage through an unbound framebuffer attachment.
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, 0, 0);
    if (glGetError() != GL_NO_ERROR) {
      throw std::runtime_error("Deformation draw failed");
    }
  }

  std::size_t budget;
  GLFrameBufferObject framebuffer{"Deformation numerical pass"};
  GLVertexArrayObject vao;
  std::unique_ptr<GLShaderProgram> flow;
  std::unique_ptr<GLShaderProgram> compose;
  std::unique_ptr<GLShaderProgram> velocity;
  std::unique_ptr<GLShaderProgram> quality;
};

FieldPassRunner::FieldPassRunner(std::size_t workspaceBytes) : m_impl(std::make_unique<Impl>(workspaceBytes)) {}
FieldPassRunner::~FieldPassRunner() = default;

void FieldPassRunner::identity(FieldTexture& output)
{
  detail::NumericalState state;
  m_impl->prepare(*m_impl->flow, output);
  glUniform1i(glGetUniformLocation(m_impl->flow->handle(), "u_identity"), 1);
  Impl::draw();
}

void FieldPassRunner::copy(const FieldTexture& input, FieldTexture& output)
{
  seed(input, 1.0f, output);
}

void FieldPassRunner::seed(const FieldTexture& velocity, float scale, FieldTexture& output)
{
  compatible(velocity, output);
  if (!std::isfinite(scale)) {
    throw std::invalid_argument("Velocity scale must be finite");
  }
  detail::NumericalState state;
  m_impl->prepare(*m_impl->flow, output);
  velocity.texture().bind(0);
  glUniform1i(glGetUniformLocation(m_impl->flow->handle(), "u_identity"), 0);
  glUniform1f(glGetUniformLocation(m_impl->flow->handle(), "u_scale"), scale);
  Impl::draw();
}

void FieldPassRunner::compose(const FieldTexture& outer, const FieldTexture& inner, FieldTexture& output)
{
  compatible(outer, output);
  compatible(inner, output);
  detail::NumericalState state;
  m_impl->prepare(*m_impl->compose, output);
  outer.texture().bind(0);
  inner.texture().bind(1);
  Impl::draw();
}

void FieldPassRunner::velocity(const ::deformation::VelocityLattice& lattice, FieldTexture& output)
{
  requirePlane(lattice.domain());
  requirePlane(output.domain());
  const auto& recipe = lattice.brush().definition();
  if (recipe.protection.size() > 32) {
    throw std::invalid_argument("GPU brushes support at most 32 protected disks");
  }
  static_cast<void>(output.domain().physicalToIndex(recipe.centerMm));
  if (std::abs(glm::dot(recipe.directions[2], output.domain().directions()[2])) < 1.0 - FieldDomain::directionTolerance)
  {
    throw std::invalid_argument("Brush and output must lie in the same plane");
  }
  FieldTexture coefficients(lattice.domain(), m_impl->budget);
  std::vector<glm::vec4> data;
  data.reserve(lattice.coefficients().size());
  std::ranges::transform(lattice.coefficients(), std::back_inserter(data), [](const auto& v) {
    return glm::vec4(narrow(v), 1.0f);
  });
  coefficients.upload(data);
  const auto center = narrow(recipe.centerMm - output.domain().origin());
  const auto origin = narrow(lattice.domain().origin() - output.domain().origin());
  const auto matrix = narrow(worldToIndex(lattice.domain()));
  const float radius = narrow(recipe.radiusMm);
  std::vector<glm::vec4> protection;
  std::vector<float> transition;
  for (const auto& region : recipe.protection) {
    protection.emplace_back(narrow(region.centerMm - output.domain().origin()), narrow(region.coreRadiusMm));
    transition.push_back(narrow(region.transitionMm));
  }
  detail::NumericalState state;
  m_impl->prepare(*m_impl->velocity, output);
  coefficients.texture().bind(0);
  const GLuint p = m_impl->velocity->handle();
  uniform(p, "u_center", center);
  uniform(p, "u_latticeOrigin", origin);
  uniform(p, "u_latticeFromWorld", matrix);
  glUniform1f(glGetUniformLocation(p, "u_radius"), radius);
  glUniform1i(glGetUniformLocation(p, "u_protectionCount"), static_cast<GLint>(protection.size()));
  if (!protection.empty()) {
    glUniform4fv(
      glGetUniformLocation(p, "u_protection[0]"),
      static_cast<GLsizei>(protection.size()),
      glm::value_ptr(protection[0]));
    glUniform1fv(
      glGetUniformLocation(p, "u_transition[0]"),
      static_cast<GLsizei>(transition.size()),
      transition.data());
  }
  Impl::draw();
}

FieldPair FieldPassRunner::exponential(const FieldTexture& velocity, unsigned squarings, const Cancel& cancel)
{
  requirePlane(velocity.domain());
  if (squarings > 20) {
    throw std::invalid_argument("Scaling and squaring requires 0 to 20 squarings");
  }
  m_impl->reserve(velocity, 3);
  const auto stopped = [&cancel] {
    return cancel && cancel();
  };
  if (stopped()) {
    return {};
  }
  FieldPair result{
    std::make_unique<FieldTexture>(velocity.domain(), m_impl->budget),
    std::make_unique<FieldTexture>(velocity.domain(), m_impl->budget)};
  auto scratch = std::make_unique<FieldTexture>(velocity.domain(), m_impl->budget);
  const float scale = std::ldexp(1.0f, -static_cast<int>(squarings));
  seed(velocity, scale, *result.forward);
  if (stopped()) {
    return {};
  }
  seed(velocity, -scale, *result.inverse);
  for (unsigned i = 0; i < squarings; ++i) {
    if (stopped()) {
      return {};
    }
    compose(*result.forward, *result.forward, *scratch);
    std::swap(result.forward, scratch);
    if (stopped()) {
      return {};
    }
    compose(*result.inverse, *result.inverse, *scratch);
    std::swap(result.inverse, scratch);
  }
  return stopped() ? FieldPair{} : std::move(result);
}

FieldPair FieldPassRunner::accumulate(const FieldPair& previous, const FieldPair& increment, const Cancel& cancel)
{
  requirePair(previous);
  requirePair(increment);
  m_impl->reserve(*previous.forward, 2);
  const auto stopped = [&cancel] {
    return cancel && cancel();
  };
  if (stopped()) {
    return {};
  }
  FieldPair result{
    std::make_unique<FieldTexture>(previous.forward->domain(), m_impl->budget),
    std::make_unique<FieldTexture>(previous.forward->domain(), m_impl->budget)};
  compose(*increment.forward, *previous.forward, *result.forward);
  if (stopped()) {
    return {};
  }
  compose(*previous.inverse, *increment.inverse, *result.inverse);
  return stopped() ? FieldPair{} : std::move(result);
}

void FieldPassRunner::quality(const FieldTexture& forward, const FieldTexture& inverse, FieldTexture& output)
{
  compatible(forward, output);
  compatible(inverse, output);
  detail::NumericalState state;
  m_impl->prepare(*m_impl->quality, output);
  forward.texture().bind(0);
  inverse.texture().bind(1);
  Impl::draw();
}
} // namespace rendering::deformation
