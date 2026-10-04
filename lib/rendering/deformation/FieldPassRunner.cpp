#include "rendering/deformation/FieldPassRunner.h"

#include "deformation/ContractError.h"
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
#include <cstdint>
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
void compatible(const FieldTexture& input, const FieldTexture& output)
{
  const auto& a = input.domain();
  const auto& b = output.domain();
  if (
    a.dimension() != b.dimension() || a.size() != b.size() || a.origin() != b.origin() || a.spacing() != b.spacing() ||
    a.directions() != b.directions() || a.validExtent().begin != b.validExtent().begin ||
    a.validExtent().end != b.validExtent().end)
  {
    throw std::invalid_argument("Composition requires identical field grids");
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
  for (const auto* name : {"u_begin3", "u_end3"}) {
    uniforms.insertUniform(name, UniformType::Vec3, glm::vec3(0), false);
  }
  for (const auto* name : {"u_worldToIndex", "u_directions", "u_latticeFromWorld"}) {
    uniforms.insertUniform(name, UniformType::Mat3, glm::mat3(1), false);
  }
  for (const auto* name :
       {"u_spacing", "u_latticeOrigin", "u_center", "u_sourceSize", "u_requestBegin", "u_requestEnd"})
  {
    uniforms.insertUniform(name, UniformType::Vec3, glm::vec3(0), false);
  }
  for (const auto* name : {"u_first", "u_second"}) {
    uniforms.insertUniform(name, UniformType::Sampler, Uniforms::SamplerIndexType{0}, false);
  }
  uniforms.insertUniform("u_scale", UniformType::Float, 1.0f, false);
  uniforms.insertUniform("u_radius", UniformType::Float, 1.0f, false);
  uniforms.insertUniform("u_identity", UniformType::Bool, false, false);
  uniforms.insertUniform("u_layer", UniformType::Int, 0, false);
  uniforms.insertUniform("u_mode", UniformType::Int, 0, false);
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
    , flow3D(program("FlowSeed3D.fs"))
    , compose(program("ComposeDisplacement.fs"))
    , compose3D(program("ComposeDisplacement3D.fs"))
    , velocity(program("VelocityEvaluate.fs"))
    , velocity3D(program("VelocityEvaluate3D.fs"))
    , quality(program("Quality.fs"))
    , quality3D(program("Quality3D.fs"))
    , reduce2D(program("QualityReduce2D.fs"))
    , reduce3D(program("QualityReduce3D.fs"))
    , stretch2D(program("Stretch2D.fs"))
    , stretch3D(program("Stretch3D.fs"))
    , refinement2D(program("Refinement2D.fs"))
    , refinement3D(program("Refinement3D.fs"))
    , protected2D(program("ProtectedCore2D.fs"))
    , protected3D(program("ProtectedCore3D.fs"))
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
    const auto& domain = output.domain();
    // Validate float conversions before issuing a draw. Shader coordinates never contain the world origin.
    const auto spacing = narrow(domain.spacing());
    const auto directions = narrow(domain.directions());
    const auto indexMatrix = narrow(worldToIndex(domain));
    const auto physicalSize = domain.spacing() * glm::dvec3(domain.size()[0], domain.size()[1], domain.size()[2]);
    static_cast<void>(narrow(physicalSize));
    framebuffer.bind(fbo::TargetType::DrawAndRead);
    if (domain.dimension() == SpatialDimension::Plane) {
      framebuffer.attach2DTexture(fbo::TargetType::DrawAndRead, fbo::AttachmentType::Color, output.texture(), 0);
    }
    glDrawBuffer(GL_COLOR_ATTACHMENT0);
    glReadBuffer(GL_COLOR_ATTACHMENT0);
    glViewport(0, 0, static_cast<GLsizei>(domain.size()[0]), static_cast<GLsizei>(domain.size()[1]));
    vao.bind();
    shader.use();
    const GLuint p = shader.handle();
    for (GLuint unit = 0; unit < 2; ++unit) {
      glActiveTexture(GL_TEXTURE0 + unit);
      glBindTexture(GL_TEXTURE_2D, 0);
      glBindTexture(GL_TEXTURE_3D, 0);
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
    glUniform3f(
      glGetUniformLocation(p, "u_begin3"),
      static_cast<GLint>(domain.validExtent().begin[0]),
      static_cast<GLint>(domain.validExtent().begin[1]),
      static_cast<GLint>(domain.validExtent().begin[2]));
    glUniform3f(
      glGetUniformLocation(p, "u_end3"),
      static_cast<GLint>(domain.validExtent().end[0]),
      static_cast<GLint>(domain.validExtent().end[1]),
      static_cast<GLint>(domain.validExtent().end[2]));
    uniform(p, "u_spacing", spacing);
    uniform(p, "u_directions", directions);
    uniform(p, "u_worldToIndex", indexMatrix);
  }

  void draw(FieldTexture& output, GLuint programHandle)
  {
    if (output.domain().dimension() == SpatialDimension::Volume) {
      for (std::uint32_t layer = 0; layer < output.domain().size()[2]; ++layer) {
        framebuffer.attachTextureLayer(
          fbo::TargetType::DrawAndRead,
          fbo::AttachmentType::Color,
          output.texture(),
          static_cast<GLint>(layer),
          0);
        glUniform1i(glGetUniformLocation(programHandle, "u_layer"), static_cast<GLint>(layer));
        glDrawArrays(GL_TRIANGLES, 0, 3);
      }
    }
    else {
      glDrawArrays(GL_TRIANGLES, 0, 3);
    }
    // Do not retain deleted scratch storage through an unbound framebuffer attachment.
    glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, 0, 0);
    if (glGetError() != GL_NO_ERROR) {
      throw std::runtime_error("Deformation draw failed");
    }
  }

  std::size_t budget;
  GLFrameBufferObject framebuffer{"Deformation numerical pass"};
  GLVertexArrayObject vao;
  std::unique_ptr<GLShaderProgram> flow;
  std::unique_ptr<GLShaderProgram> flow3D;
  std::unique_ptr<GLShaderProgram> compose;
  std::unique_ptr<GLShaderProgram> compose3D;
  std::unique_ptr<GLShaderProgram> velocity;
  std::unique_ptr<GLShaderProgram> velocity3D;
  std::unique_ptr<GLShaderProgram> quality;
  std::unique_ptr<GLShaderProgram> quality3D;
  std::unique_ptr<GLShaderProgram> reduce2D;
  std::unique_ptr<GLShaderProgram> reduce3D;
  std::unique_ptr<GLShaderProgram> stretch2D;
  std::unique_ptr<GLShaderProgram> stretch3D;
  std::unique_ptr<GLShaderProgram> refinement2D;
  std::unique_ptr<GLShaderProgram> refinement3D;
  std::unique_ptr<GLShaderProgram> protected2D;
  std::unique_ptr<GLShaderProgram> protected3D;
};

FieldPassRunner::FieldPassRunner(std::size_t workspaceBytes) : m_impl(std::make_unique<Impl>(workspaceBytes)) {}
FieldPassRunner::~FieldPassRunner() = default;

void FieldPassRunner::identity(FieldTexture& output)
{
  detail::NumericalState state;
  auto& shader = output.domain().dimension() == SpatialDimension::Plane ? *m_impl->flow : *m_impl->flow3D;
  m_impl->prepare(shader, output);
  glUniform1i(glGetUniformLocation(shader.handle(), "u_identity"), 1);
  m_impl->draw(output, shader.handle());
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
  auto& shader = output.domain().dimension() == SpatialDimension::Plane ? *m_impl->flow : *m_impl->flow3D;
  m_impl->prepare(shader, output);
  velocity.texture().bind(0);
  glUniform1i(glGetUniformLocation(shader.handle(), "u_identity"), 0);
  glUniform1f(glGetUniformLocation(shader.handle(), "u_scale"), scale);
  m_impl->draw(output, shader.handle());
}

void FieldPassRunner::compose(const FieldTexture& outer, const FieldTexture& inner, FieldTexture& output)
{
  compatible(outer, output);
  compatible(inner, output);
  detail::NumericalState state;
  auto& shader = output.domain().dimension() == SpatialDimension::Plane ? *m_impl->compose : *m_impl->compose3D;
  m_impl->prepare(shader, output);
  outer.texture().bind(0);
  inner.texture().bind(1);
  m_impl->draw(output, shader.handle());
}

void FieldPassRunner::velocity(const ::deformation::VelocityLattice& lattice, FieldTexture& output)
{
  if (lattice.domain().dimension() != output.domain().dimension()) {
    throw std::invalid_argument("Velocity lattice and output dimensions must match");
  }
  const auto& recipe = lattice.brush().definition();
  if (recipe.protection.size() > 32) {
    throw std::invalid_argument("GPU brushes support at most 32 protected regions");
  }
  static_cast<void>(output.domain().physicalToIndex(recipe.centerMm));
  if (
    output.domain().dimension() == SpatialDimension::Plane &&
    std::abs(glm::dot(recipe.directions[2], output.domain().directions()[2])) < 1.0 - FieldDomain::directionTolerance)
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
  auto& shader = output.domain().dimension() == SpatialDimension::Plane ? *m_impl->velocity : *m_impl->velocity3D;
  m_impl->prepare(shader, output);
  coefficients.texture().bind(0);
  const GLuint p = shader.handle();
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
  m_impl->draw(output, p);
}

FieldPair FieldPassRunner::exponential(const FieldTexture& velocity, unsigned squarings, const Cancel& cancel)
{
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
  auto& shader = output.domain().dimension() == SpatialDimension::Plane ? *m_impl->quality : *m_impl->quality3D;
  m_impl->prepare(shader, output);
  forward.texture().bind(0);
  inverse.texture().bind(1);
  m_impl->draw(output, shader.handle());
}

ReducedDirectionQuality FieldPassRunner::reduceQuality(const FieldTexture& qualityMap, bool inset)
{
  constexpr std::size_t maxTileSamples = std::size_t{1} << 20;
  const auto& sourceDomain = qualityMap.domain();
  const bool volume = sourceDomain.dimension() == SpatialDimension::Volume;
  const auto originalSize = sourceDomain.size();
  auto currentSize = originalSize;
  const auto& extent = sourceDomain.validExtent();
  const glm::vec3 requestBegin(
    static_cast<float>(extent.begin[0] + (inset ? 1 : 0)),
    static_cast<float>(extent.begin[1] + (inset ? 1 : 0)),
    static_cast<float>(volume ? extent.begin[2] + (inset ? 1 : 0) : 0));
  const glm::vec3 requestEnd(
    static_cast<float>(extent.end[0] - (inset ? 1 : 0)),
    static_cast<float>(extent.end[1] - (inset ? 1 : 0)),
    static_cast<float>(volume ? extent.end[2] - (inset ? 1 : 0) : 1));
  std::unique_ptr<FieldTexture> extrema;
  std::unique_ptr<FieldTexture> counts;
  std::size_t tileEdge = 1;
  bool first = true;

  const auto render = [this, &requestBegin, &requestEnd](const FieldTexture& input, FieldTexture& output, int mode) {
    detail::NumericalState state;
    auto& shader = output.domain().dimension() == SpatialDimension::Plane ? *m_impl->reduce2D : *m_impl->reduce3D;
    m_impl->prepare(shader, output);
    input.texture().bind(0);
    const GLuint p = shader.handle();
    const auto& size = input.domain().size();
    uniform(p, "u_sourceSize", glm::vec3(size[0], size[1], size[2]));
    uniform(p, "u_requestBegin", requestBegin);
    uniform(p, "u_requestEnd", requestEnd);
    glUniform1i(glGetUniformLocation(p, "u_mode"), mode);
    m_impl->draw(output, p);
  };

  while (true) {
    const std::size_t nextEdge = tileEdge * 4;
    std::size_t represented = 1;
    for (int axis = 0; axis < (volume ? 3 : 2); ++axis) {
      represented *= std::min<std::size_t>(originalSize[axis], nextEdge);
    }
    if (!first && represented > maxTileSamples) break;

    ::deformation::DomainGeometry scratchGeometry;
    scratchGeometry.dimension = sourceDomain.dimension();
    for (int axis = 0; axis < (volume ? 3 : 2); ++axis) {
      scratchGeometry.size[axis] = (currentSize[axis] + 3) / 4;
    }
    const FieldDomain scratchDomain(scratchGeometry);
    const std::size_t held = (extrema ? extrema->bytes() + counts->bytes() : 0);
    if (held > m_impl->budget || scratchDomain.sampleCount() > (m_impl->budget - held) / (2 * sizeof(glm::vec4))) {
      throw std::invalid_argument("Quality reduction workspace budget exceeded");
    }
    auto nextExtrema = std::make_unique<FieldTexture>(scratchDomain, m_impl->budget);
    auto nextCounts = std::make_unique<FieldTexture>(scratchDomain, m_impl->budget);
    render(first ? qualityMap : *extrema, *nextExtrema, first ? 0 : 2);
    render(first ? qualityMap : *counts, *nextCounts, first ? 1 : 3);
    extrema = std::move(nextExtrema);
    counts = std::move(nextCounts);
    currentSize = scratchDomain.size();
    tileEdge = nextEdge;
    first = false;
    if (currentSize[0] == 1 && currentSize[1] == 1 && (!volume || currentSize[2] == 1)) break;
  }

  const auto extremaData = extrema->readback();
  const auto countData = counts->readback();
  const auto exactCount = [](float value) -> std::size_t {
    if (
      !std::isfinite(value) || value < 0.0f || value > static_cast<float>(maxTileSamples) || std::floor(value) != value)
    {
      throw std::runtime_error("Quality reduction produced an invalid tile count");
    }
    return static_cast<std::size_t>(value);
  };
  ReducedDirectionQuality summary;
  for (std::size_t i = 0; i < countData.size(); ++i) {
    const auto requested = exactCount(countData[i].x);
    const auto evaluated = exactCount(countData[i].y);
    const auto outside = exactCount(countData[i].z);
    const auto nonFinite = exactCount(countData[i].w);
    if (
      evaluated > requested || outside != requested - evaluated || nonFinite > outside ||
      requested > sourceDomain.sampleCount() - summary.requested)
    {
      throw std::runtime_error("Quality reduction produced inconsistent counts");
    }
    summary.requested += requested;
    summary.evaluated += evaluated;
    summary.outside += outside;
    summary.nonFinite += nonFinite;
    if (evaluated == 0) continue;
    const auto value = extremaData[i];
    if (
      !std::isfinite(value.x) || !std::isfinite(value.y) || !std::isfinite(value.z) || !std::isfinite(value.w) ||
      value.x > value.y || value.z < 0.0f || value.w < 0.0f)
    {
      throw std::runtime_error("Quality reduction produced invalid extrema");
    }
    summary.minDeterminant = std::min(summary.minDeterminant, static_cast<double>(value.x));
    summary.maxDeterminant = std::max(summary.maxDeterminant, static_cast<double>(value.y));
    summary.maxResidualMm = std::max(summary.maxResidualMm, static_cast<double>(value.z));
    summary.maxResidualVoxels = std::max(summary.maxResidualVoxels, static_cast<double>(value.w));
  }
  return summary;
}

::deformation::DirectionQuality FieldPassRunner::analyzeDirection(
  const FieldTexture& forward,
  const FieldTexture& inverse)
{
  compatible(forward, inverse);
  m_impl->reserve(forward, 2);
  FieldTexture diagnostic(forward.domain(), m_impl->budget);
  quality(forward, inverse, diagnostic);
  const auto geometry = reduceQuality(diagnostic);

  {
    detail::NumericalState state;
    auto& shader = forward.domain().dimension() == SpatialDimension::Plane ? *m_impl->stretch2D : *m_impl->stretch3D;
    m_impl->prepare(shader, diagnostic);
    forward.texture().bind(0);
    m_impl->draw(diagnostic, shader.handle());
  }
  const auto stretch = reduceQuality(diagnostic);
  if (geometry.requested != stretch.requested || stretch.evaluated < geometry.evaluated) {
    throw std::runtime_error("Quality and stretch coverage disagree");
  }
  ::deformation::DirectionQuality result;
  result.requested = geometry.requested;
  result.evaluated = geometry.evaluated;
  result.outside = geometry.outside;
  result.finite = geometry.nonFinite == 0 && stretch.nonFinite == 0;
  result.minDeterminant = geometry.minDeterminant;
  result.maxDeterminant = geometry.maxDeterminant;
  result.minSingularValue = stretch.minDeterminant;
  result.maxSingularValue = stretch.maxResidualMm;
  result.maxResidualMm = geometry.maxResidualMm;
  result.maxResidualVoxels = geometry.maxResidualVoxels;
  return result;
}

::deformation::QualityReport FieldPassRunner::sampledReport(const FieldPair& pair)
{
  requirePair(pair);
  ::deformation::QualityReport report;
  report.forward = analyzeDirection(*pair.forward, *pair.inverse);
  report.inverse = analyzeDirection(*pair.inverse, *pair.forward);
  return report;
}

PairCellVerification FieldPassRunner::verifyCells(
  const FieldPair& pair,
  const ::deformation::QualityPolicy& policy,
  unsigned maxDepth,
  std::size_t maxSamples)
{
  requirePair(pair);
  PairCellVerification report;
  report.forward =
    ::deformation::verifyFieldCells(pair.forward->domain(), pair.forward->readback(), policy, maxDepth, maxSamples);
  report.inverse =
    ::deformation::verifyFieldCells(pair.inverse->domain(), pair.inverse->readback(), policy, maxDepth, maxSamples);
  return report;
}

::deformation::RefinementEvidence
FieldPassRunner::compareRefinement(const FieldPair& candidate, const FieldPair& refined, std::size_t maxPoints)
{
  requirePair(candidate);
  requirePair(refined);
  const auto candidateForward = candidate.forward->readback();
  const auto candidateInverse = candidate.inverse->readback();
  const auto refinedForward = refined.forward->readback();
  const auto refinedInverse = refined.inverse->readback();
  return ::deformation::compareRefinement(
    {{candidate.forward->domain(), candidateForward}, {candidate.inverse->domain(), candidateInverse}},
    {{refined.forward->domain(), refinedForward}, {refined.inverse->domain(), refinedInverse}},
    maxPoints);
}

::deformation::RefinementEvidence FieldPassRunner::reduceRefinement(
  const FieldPair& candidate,
  const FieldPair& refined)
{
  requirePair(candidate);
  requirePair(refined);
  compatible(*candidate.forward, *refined.forward);
  compatible(*candidate.inverse, *refined.inverse);
  m_impl->reserve(*candidate.forward, 1);
  FieldTexture diagnostic(candidate.forward->domain(), m_impl->budget);
  ::deformation::RefinementEvidence evidence;
  const auto& extent = candidate.forward->domain().validExtent();
  const int n = candidate.forward->domain().dimension() == SpatialDimension::Plane ? 2 : 3;
  std::size_t centers = 1;
  std::size_t cells = 1;
  for (int axis = 0; axis < n; ++axis) {
    const std::size_t length = extent.end[axis] - extent.begin[axis];
    centers *= length;
    cells *= length - 1;
  }
  evidence.requested = 2 * (centers + cells);
  const auto run = [&](const FieldTexture& coarse, const FieldTexture& fine) {
    for (int mode = 0; mode < 2; ++mode) {
      {
        detail::NumericalState state;
        auto& shader =
          coarse.domain().dimension() == SpatialDimension::Plane ? *m_impl->refinement2D : *m_impl->refinement3D;
        m_impl->prepare(shader, diagnostic);
        coarse.texture().bind(0);
        fine.texture().bind(1);
        glUniform1i(glGetUniformLocation(shader.handle(), "u_mode"), mode);
        m_impl->draw(diagnostic, shader.handle());
      }
      const auto reduced = reduceQuality(diagnostic, false);
      evidence.unavailable += reduced.outside;
      evidence.maxErrorMm = std::max(evidence.maxErrorMm, reduced.maxResidualMm + reduced.maxResidualVoxels);
    }
  };
  run(*candidate.forward, *refined.forward);
  run(*candidate.inverse, *refined.inverse);
  return evidence;
}

::deformation::ProtectionEvidence FieldPassRunner::measureProtectedCores(
  const FieldPair& increment,
  std::span<const ::deformation::ProtectedRegion> regions,
  double targetErrorMm,
  unsigned maxDepth,
  std::size_t maxCells)
{
  requirePair(increment);
  if (regions.empty())
    return ::deformation::measureProtectedCores(
      {{increment.forward->domain(), {}}, {increment.inverse->domain(), {}}},
      regions,
      targetErrorMm,
      maxDepth,
      maxCells);
  const auto forward = increment.forward->readback();
  const auto inverse = increment.inverse->readback();
  return ::deformation::measureProtectedCores(
    {{increment.forward->domain(), forward}, {increment.inverse->domain(), inverse}},
    regions,
    targetErrorMm,
    maxDepth,
    maxCells);
}

ProtectedCorePrecheck FieldPassRunner::precheckProtectedCores(
  const FieldPair& increment,
  std::span<const ::deformation::ProtectedRegion> regions)
{
  requirePair(increment);
  if (regions.empty()) return {true, true};
  if (regions.size() > 32) return {};
  const auto& domain = increment.forward->domain();
  const auto& extent = domain.validExtent();
  const int n = domain.dimension() == SpatialDimension::Plane ? 2 : 3;
  std::vector<glm::vec4> protection;
  protection.reserve(regions.size());
  for (int axis = 0; axis < n; ++axis) {
    if (extent.end[axis] - extent.begin[axis] < 2 || domain.size()[axis] > 8192) return {};
  }
  for (const auto& region : regions) {
    if (!std::isfinite(region.coreRadiusMm) || region.coreRadiusMm < 0.0) return {};
    glm::dvec3 center;
    try {
      center = domain.physicalToIndex(region.centerMm);
    }
    catch (const ::deformation::ContractError&) {
      return {};
    }
    for (int axis = 0; axis < n; ++axis) {
      const double radiusIndex = region.coreRadiusMm / domain.spacing()[axis];
      if (center[axis] - radiusIndex < extent.begin[axis] || center[axis] + radiusIndex > extent.end[axis] - 1) {
        return {};
      }
    }
    protection.emplace_back(narrow(center), narrow(region.coreRadiusMm));
  }
  m_impl->reserve(*increment.forward, 1);
  FieldTexture diagnostic(domain, m_impl->budget);
  ProtectedCorePrecheck result{true, true};
  for (const auto* field : {increment.forward.get(), increment.inverse.get()}) {
    {
      detail::NumericalState state;
      auto& shader = domain.dimension() == SpatialDimension::Plane ? *m_impl->protected2D : *m_impl->protected3D;
      m_impl->prepare(shader, diagnostic);
      field->texture().bind(0);
      const GLuint p = shader.handle();
      glUniform1i(glGetUniformLocation(p, "u_protectionCount"), static_cast<GLint>(protection.size()));
      glUniform4fv(
        glGetUniformLocation(p, "u_protection[0]"),
        static_cast<GLsizei>(protection.size()),
        glm::value_ptr(protection[0]));
      m_impl->draw(diagnostic, p);
    }
    const auto reduced = reduceQuality(diagnostic, false);
    result.checked = result.checked && reduced.outside == 0 && reduced.nonFinite == 0 && reduced.maxDeterminant >= 1.0;
    result.exactlyZero = result.exactlyZero && reduced.maxResidualMm == 0.0;
  }
  return result;
}

CandidateResult FieldPassRunner::acceptVelocity(
  const FieldTexture& velocity,
  const FieldPair* previous,
  std::span<const ::deformation::ProtectedRegion> regions,
  const ::deformation::QualityPolicy& policy,
  unsigned firstSquarings,
  unsigned maxAttempts,
  const Cancel& cancel)
{
  if (firstSquarings >= 20 || maxAttempts == 0 || maxAttempts > 20) {
    throw std::invalid_argument("Candidate attempt limits are invalid");
  }
  if (previous) {
    requirePair(*previous);
    compatible(*previous->forward, velocity);
    compatible(*previous->inverse, velocity);
  }
  CandidateResult result;
  const auto stopped = [&cancel] {
    return cancel && cancel();
  };
  for (unsigned step = 0; step < maxAttempts && firstSquarings + step < 20; ++step) {
    if (stopped()) {
      result.canceled = true;
      break;
    }
    ++result.attempts;
    auto increment = exponential(velocity, firstSquarings + step, cancel);
    if (!increment.forward) {
      result.canceled = true;
      break;
    }
    auto refinedIncrement = exponential(velocity, firstSquarings + step + 1, cancel);
    if (!refinedIncrement.forward) {
      result.canceled = true;
      break;
    }
    auto candidate = previous ? accumulate(*previous, increment, cancel) : std::move(increment);
    if (!candidate.forward) {
      result.canceled = true;
      break;
    }
    auto refined = previous ? accumulate(*previous, refinedIncrement, cancel) : std::move(refinedIncrement);
    if (!refined.forward) {
      result.canceled = true;
      break;
    }
    if (stopped()) {
      result.canceled = true;
      break;
    }
    result.report = sampledReport(candidate);
    if (stopped()) {
      result.canceled = true;
      break;
    }
    // Assess sampled hard failures before allocating CPU verification work.
    const auto sampled = ::deformation::assessCandidate(result.report, policy);
    if (sampled.decision == ::deformation::CandidateDecision::Reject) {
      result.assessment = sampled;
      break;
    }
    result.cells = verifyCells(candidate, policy);
    if (stopped()) {
      result.canceled = true;
      break;
    }
    if (result.cells.forward.folded || result.cells.inverse.folded) {
      result.assessment = {::deformation::CandidateDecision::Reject, ::deformation::QualityReason::Folding};
      break;
    }
    if (result.cells.forward.invalid || result.cells.inverse.invalid) {
      result.assessment = {::deformation::CandidateDecision::Reject, ::deformation::QualityReason::NonFinite};
      break;
    }
    result.report.cellsVerified = result.cells.complete();
    if (!result.report.cellsVerified) {
      result.assessment = {::deformation::CandidateDecision::Refine, ::deformation::QualityReason::MissingEvidence};
      break;
    }
    const auto& incrementPair = previous ? increment : candidate;
    const auto protectionPrecheck = precheckProtectedCores(incrementPair, regions);
    if (protectionPrecheck.checked && protectionPrecheck.exactlyZero) {
      result.protection.checked = true;
    }
    else {
      result.protection = measureProtectedCores(incrementPair, regions, policy.maxProtectionErrorMm);
    }
    if (stopped()) {
      result.canceled = true;
      break;
    }
    result.report.protectionChecked = result.protection.checked;
    result.report.maxProtectionErrorMm = result.protection.maxErrorMm;
    if (!result.protection.checked) {
      result.assessment = {::deformation::CandidateDecision::Refine, ::deformation::QualityReason::MissingEvidence};
      break;
    }
    result.refinement = reduceRefinement(candidate, refined);
    if (stopped()) {
      result.canceled = true;
      break;
    }
    result.report.convergenceChecked = result.refinement.checked();
    result.report.maxConvergenceErrorMm = result.refinement.maxErrorMm;
    result.assessment = ::deformation::assessCandidate(result.report, policy);
    if (result.assessment.decision == ::deformation::CandidateDecision::Accept) {
      if (stopped()) {
        result.canceled = true;
        break;
      }
      result.pair = std::move(candidate);
      break;
    }
    if (
      result.assessment.decision != ::deformation::CandidateDecision::Refine ||
      (result.assessment.reason != ::deformation::QualityReason::InverseConsistency &&
       result.assessment.reason != ::deformation::QualityReason::Convergence))
      break;
  }
  return result;
}
} // namespace rendering::deformation
