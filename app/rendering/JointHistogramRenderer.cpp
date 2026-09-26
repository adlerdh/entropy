#include "rendering/JointHistogramRenderer.h"

#include "common/Exception.hpp"
#include "common/Viewport.h"
#include "rendering/JointHistogramBatching.h"
#include "rendering/JointHistogramShaderUniforms.h"
#include "rendering/gl/OpenGLStateGuard.h"

#include <cmrc/cmrc.hpp>

#include <glm/gtc/type_ptr.hpp>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <format>
#include <limits>
#include <new>
#include <string>
#include <utility>
#include <vector>

CMRC_DECLARE(shaders);

namespace rendering
{
namespace
{
constexpr int kBackgroundTileSize = 16;

// These buffer/transform-feedback states are outside the ordinary framebuffer state guard.
struct SampleStateGuard
{
  GLint arrayBuffer = 0;
  GLint feedbackBuffer = 0;
  GLint indexedFeedbackBuffer = 0;
  GLint64 feedbackStart = 0;
  GLint64 feedbackSize = 0;
  GLboolean discard = glIsEnabled(GL_RASTERIZER_DISCARD);
  GLboolean pointSize = glIsEnabled(GL_PROGRAM_POINT_SIZE);

  SampleStateGuard()
  {
    glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &arrayBuffer);
    glGetIntegerv(GL_TRANSFORM_FEEDBACK_BUFFER_BINDING, &feedbackBuffer);
    glGetIntegeri_v(GL_TRANSFORM_FEEDBACK_BUFFER_BINDING, 0, &indexedFeedbackBuffer);
    glGetInteger64i_v(GL_TRANSFORM_FEEDBACK_BUFFER_START, 0, &feedbackStart);
    glGetInteger64i_v(GL_TRANSFORM_FEEDBACK_BUFFER_SIZE, 0, &feedbackSize);
  }

  ~SampleStateGuard()
  {
    glBindBuffer(GL_ARRAY_BUFFER, static_cast<GLuint>(arrayBuffer));
    if (indexedFeedbackBuffer != 0 && feedbackSize > 0) {
      glBindBufferRange(
        GL_TRANSFORM_FEEDBACK_BUFFER,
        0,
        static_cast<GLuint>(indexedFeedbackBuffer),
        static_cast<GLintptr>(feedbackStart),
        static_cast<GLsizeiptr>(feedbackSize));
    }
    else {
      glBindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER, 0, static_cast<GLuint>(indexedFeedbackBuffer));
    }
    glBindBuffer(GL_TRANSFORM_FEEDBACK_BUFFER, static_cast<GLuint>(feedbackBuffer));
    discard ? glEnable(GL_RASTERIZER_DISCARD) : glDisable(GL_RASTERIZER_DISCARD);
    pointSize ? glEnable(GL_PROGRAM_POINT_SIZE) : glDisable(GL_PROGRAM_POINT_SIZE);
  }
};

std::string shaderSource(const std::string& name)
{
  const cmrc::file file = cmrc::shaders::get_filesystem().open("rendering/shaders/" + name);
  return std::string(file.begin(), file.end());
}

std::unique_ptr<GLShaderProgram> makeProgram(
  const std::string& name,
  const std::string& vertexSource,
  const std::string& fragmentSource,
  Uniforms vertexUniforms,
  Uniforms fragmentUniforms)
{
  GLShader vertex(name + " vertex", ShaderType::Vertex, vertexSource.c_str());
  GLShader fragment(name + " fragment", ShaderType::Fragment, fragmentSource.c_str());
  vertex.setRegisteredUniforms(std::move(vertexUniforms));
  fragment.setRegisteredUniforms(std::move(fragmentUniforms));
  auto program = std::make_unique<GLShaderProgram>(name);
  if (!program->attachShader(vertex) || !program->attachShader(fragment) || !program->link()) {
    throwDebug("Failed to create joint-histogram shader program " + name);
  }
  return program;
}

GLint location(const GLShaderProgram& program, const std::string& name)
{
  return glGetUniformLocation(program.handle(), name.c_str());
}

void setMatrix(const GLShaderProgram& program, const std::string& name, const glm::mat4& value)
{
  glUniformMatrix4fv(location(program, name), 1, GL_FALSE, glm::value_ptr(value));
}

void setVector2(const GLShaderProgram& program, const std::string& name, const glm::vec2& value)
{
  glUniform2fv(location(program, name), 1, glm::value_ptr(value));
}

bool sameMatrix(const glm::mat4& a, const glm::mat4& b)
{
  return std::equal(glm::value_ptr(a), glm::value_ptr(a) + 16, glm::value_ptr(b));
}

bool finiteMatrix(const glm::mat4& value)
{
  return std::all_of(glm::value_ptr(value), glm::value_ptr(value) + 16, [](const float element) {
    return std::isfinite(element);
  });
}

bool finiteVector(const glm::vec2& value)
{
  return std::isfinite(value.x) && std::isfinite(value.y);
}

bool validPlanarLayout(const PlanarTextureLayout& layout)
{
  return layout.dimension != TextureDimension::Texture2D ||
         (layout.axes.x >= 0 && layout.axes.x < 3 && layout.axes.y >= 0 && layout.axes.y < 3 &&
          layout.axes.x != layout.axes.y);
}

bool validInputs(const JointHistogramRenderer::Inputs& inputs)
{
  if (
    inputs.bins < 2 || inputs.bins > 1024 || inputs.sourceTextureIds[0] == 0u || inputs.sourceTextureIds[1] == 0u ||
    !finiteMatrix(inputs.world_T_fixedTexture) || !finiteVector(inputs.metric.m_slopeIntercept) ||
    !finiteVector(inputs.metric.m_cmapSlopeIntercept))
  {
    return false;
  }
  for (std::size_t i = 0; i < 2; ++i) {
    const auto& deformation = inputs.deformations[i];
    if (
      inputs.textureComponents[i] < 0 || inputs.textureComponents[i] > 3 || !validPlanarLayout(inputs.layouts[i]) ||
      !finiteMatrix(inputs.texture_T_world[i]) || !finiteVector(inputs.normalized_T_texture[i]) ||
      (deformation.enabled && (!finiteMatrix(deformation.texture_T_world) ||
                               !std::isfinite(deformation.native_T_texture) || !std::isfinite(deformation.strength))))
    {
      return false;
    }
  }
  return true;
}

std::optional<std::uint64_t> voxelCount(const glm::uvec3 dimensions)
{
  if (dimensions.x == 0u || dimensions.y == 0u || dimensions.z == 0u) {
    return std::nullopt;
  }
  std::uint64_t count = dimensions.x;
  if (dimensions.y > std::numeric_limits<std::uint64_t>::max() / count) {
    return std::nullopt;
  }
  count *= dimensions.y;
  if (dimensions.z > std::numeric_limits<std::uint64_t>::max() / count) {
    return std::nullopt;
  }
  return count * dimensions.z;
}

bool sameFixedSamples(const JointHistogramRenderer::Inputs& a, const JointHistogramRenderer::Inputs& b)
{
  return a.fixedDimensions == b.fixedDimensions && a.sourceTextureIds[0] == b.sourceTextureIds[0] &&
         a.sourceTextureRevisions[0] == b.sourceTextureRevisions[0] &&
         a.layouts[0].dimension == b.layouts[0].dimension && a.layouts[0].axes == b.layouts[0].axes &&
         a.normalized_T_texture[0] == b.normalized_T_texture[0] && a.textureComponents[0] == b.textureComponents[0] &&
         a.linearInterpolation[0] == b.linearInterpolation[0];
}

bool allocateSampleBuffer(GLenum target, GLsizeiptr bytes, const void* data)
{
  glBufferData(target, bytes, data, GL_STATIC_DRAW);
  const GLenum error = glGetError();
  if (error == GL_OUT_OF_MEMORY) {
    return false;
  }
  if (error != GL_NO_ERROR) {
    throwDebug("Failed to allocate joint-histogram sample buffer");
  }
  return true;
}

bool sameScatterInputs(const JointHistogramRenderer::Inputs& a, const JointHistogramRenderer::Inputs& b)
{
  if (
    a.bins != b.bins || a.fixedDimensions.x != b.fixedDimensions.x || a.fixedDimensions.y != b.fixedDimensions.y ||
    a.fixedDimensions.z != b.fixedDimensions.z || a.sourceTextureIds != b.sourceTextureIds ||
    a.sourceTextureRevisions != b.sourceTextureRevisions || !sameMatrix(a.world_T_fixedTexture, b.world_T_fixedTexture))
  {
    return false;
  }
  for (std::size_t i = 0; i < 2; ++i) {
    const auto& da = a.deformations[i];
    const auto& db = b.deformations[i];
    if (
      a.layouts[i].dimension != b.layouts[i].dimension || a.layouts[i].axes != b.layouts[i].axes ||
      !sameMatrix(a.texture_T_world[i], b.texture_T_world[i]) || !sameMatrix(da.texture_T_world, db.texture_T_world) ||
      a.normalized_T_texture[i].x != b.normalized_T_texture[i].x ||
      a.normalized_T_texture[i].y != b.normalized_T_texture[i].y || da.native_T_texture != db.native_T_texture ||
      a.textureComponents[i] != b.textureComponents[i] || da.strength != db.strength || da.enabled != db.enabled ||
      da.interleaved != db.interleaved || a.linearInterpolation[i] != b.linearInterpolation[i])
    {
      return false;
    }
  }
  return true;
}
} // namespace

JointHistogramRenderer::JointHistogramRenderer()
  : m_framebuffer("Joint intensity histogram"), m_counts(tex::Target::Texture2D)
{
}

JointHistogramRenderer::~JointHistogramRenderer()
{
  glDeleteBuffers(1, &m_positions);
  glDeleteBuffers(1, &m_fixedValues);
  glDeleteTextures(1, &m_fixedValuesTexture);
  glDeleteTextures(2, m_reductionTextures.data());
  glDeleteFramebuffers(1, &m_reductionFramebuffer);
  for (const auto& timing : m_timings) {
    if (timing.query != 0) {
      glDeleteQueries(1, &timing.query);
    }
  }
}

void JointHistogramRenderer::initialize()
{
  if (m_displayProgram) {
    return;
  }
  const std::string scatterVertex = shaderSource("JointHistogramScatter.vs");
  const std::string scatterFragment = shaderSource("JointHistogramScatter.fs");
  for (int variant = 0; variant < 4; ++variant) {
    std::string source = scatterVertex;
    std::string defines;
    if ((variant & 1) != 0) {
      defines += "#define FIXED_IMAGE_2D\n";
    }
    if ((variant & 2) != 0) {
      defines += "#define MOVING_IMAGE_2D\n";
    }
    source.insert(source.find('\n') + 1, defines);
    m_scatterPrograms[variant] = makeProgram(
      std::format("Joint histogram scatter {}", variant),
      source,
      scatterFragment,
      jointHistogramScatterUniforms(),
      Uniforms{});
  }
  m_displayProgram = makeProgram(
    "Joint histogram display",
    shaderSource("AsciiPost.vs"),
    shaderSource("JointHistogramDisplay.fs"),
    Uniforms{},
    jointHistogramDisplayUniforms());
  m_vao.generate();
  m_counts.generate();
  m_counts.setMinificationFilter(tex::MinificationFilter::Nearest);
  m_counts.setMagnificationFilter(tex::MagnificationFilter::Nearest);
  m_counts.setWrapMode(tex::WrapMode::ClampToEdge);
  m_framebuffer.generate();
  glGenBuffers(1, &m_positions);
  glGenBuffers(1, &m_fixedValues);
  glGenTextures(1, &m_fixedValuesTexture);
  glGenTextures(2, m_reductionTextures.data());
  glGenFramebuffers(1, &m_reductionFramebuffer);
  glActiveTexture(GL_TEXTURE9);
  for (int i = 0; i < 2; ++i) {
    const int size = i == 0 ? 4 : 1;
    glBindTexture(GL_TEXTURE_2D, m_reductionTextures[i]);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R32F, size, size, 0, GL_RED, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  }
  glBindFramebuffer(GL_FRAMEBUFFER, m_reductionFramebuffer);
  for (const GLuint texture : m_reductionTextures) {
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture, 0);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
      throwDebug("Failed to create joint-histogram reduction framebuffer");
    }
  }
  m_reduceProgram = makeProgram(
    "Joint histogram background reduction",
    shaderSource("AsciiPost.vs"),
    shaderSource("JointHistogramReduce.fs"),
    Uniforms{},
    jointHistogramReductionUniforms());
  for (auto& timing : m_timings) {
    glGenQueries(1, &timing.query);
  }
}

void JointHistogramRenderer::ensureTexture(int bins)
{
  if (m_allocatedBins == bins) {
    return;
  }
  m_counts.setSize(glm::uvec3{
    static_cast<unsigned>(std::max(bins, kBackgroundTileSize)),
    static_cast<unsigned>(bins + kBackgroundTileSize),
    1u});
  m_counts.setData(
    0,
    tex::SizedInternalFormat::R32F,
    tex::BufferPixelFormat::Red,
    tex::BufferPixelDataType::Float32,
    nullptr);
  m_framebuffer.bind(fbo::TargetType::Draw);
  m_framebuffer.attach2DTexture(fbo::TargetType::Draw, fbo::AttachmentType::Color, m_counts, 0);
  m_allocatedBins = bins;
}

bool JointHistogramRenderer::prepareFixedSamples(const Inputs& inputs, const std::uint64_t voxelCount)
{
  // Keep memory bounded for very large images; their existing procedural shader remains available.
  const std::uint64_t planeSize = std::uint64_t{inputs.fixedDimensions.x} * inputs.fixedDimensions.y;
  GLint maxBufferSize = 0;
  glGetIntegerv(GL_MAX_TEXTURE_BUFFER_SIZE, &maxBufferSize);
  if (
    maxBufferSize <= 0 || inputs.deformations[0].enabled || inputs.deformations[1].enabled ||
    voxelCount > 134'217'728u || planeSize > 4'194'304u || voxelCount > static_cast<std::uint64_t>(maxBufferSize) ||
    inputs.fixedDimensions.z > kJointHistogramMaxVoxelsPerBatch)
  {
    return false;
  }
  if (m_fixedSampleInputs && sameFixedSamples(*m_fixedSampleInputs, inputs)) {
    return true;
  }
  if (m_failedFixedSampleInputs && sameFixedSamples(*m_failedFixedSampleInputs, inputs)) {
    return false;
  }
  const auto allocationFailed = [this, &inputs]() {
    m_fixedSampleInputs.reset();
    m_failedFixedSampleInputs = inputs;
    // Release any old or partially allocated cache before falling back to procedural sampling.
    glBindBuffer(GL_ARRAY_BUFFER, m_positions);
    glBufferData(GL_ARRAY_BUFFER, 0, nullptr, GL_STATIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, m_fixedValues);
    glBufferData(GL_ARRAY_BUFFER, 0, nullptr, GL_STATIC_DRAW);
    return false;
  };

  const int variant = inputs.layouts[0].dimension == TextureDimension::Texture2D ? 1 : 0;
  if (!m_fixedSamplePrograms[variant]) {
    std::string source = shaderSource("JointHistogramFixedSamples.vs");
    if (variant != 0) {
      source.insert(source.find('\n') + 1, "#define FIXED_IMAGE_2D\n");
    }
    GLShader shader("Joint histogram fixed samples", ShaderType::Vertex, source.c_str());
    shader.setRegisteredUniforms(jointHistogramFixedSampleUniforms());
    auto program = std::make_unique<GLShaderProgram>("Joint histogram fixed samples");
    if (!program->attachShader(shader)) {
      throwDebug("Failed to attach fixed-sample shader");
    }
    const char* varying = "v_fixedValue";
    glTransformFeedbackVaryings(program->handle(), 1, &varying, GL_INTERLEAVED_ATTRIBS);
    if (!program->link()) {
      throwDebug("Failed to link fixed-sample shader");
    }
    m_fixedSamplePrograms[variant] = std::move(program);
  }

  m_vao.bind();
  glBindBuffer(GL_ARRAY_BUFFER, m_positions);
  if (
    !m_fixedSampleInputs || m_fixedSampleInputs->fixedDimensions.x != inputs.fixedDimensions.x ||
    m_fixedSampleInputs->fixedDimensions.y != inputs.fixedDimensions.y)
  {
    std::vector<glm::vec2> positions;
    try {
      positions.resize(planeSize);
    }
    catch (const std::bad_alloc&) {
      return allocationFailed();
    }
    for (unsigned y = 0; y < inputs.fixedDimensions.y; ++y) {
      for (unsigned x = 0; x < inputs.fixedDimensions.x; ++x) {
        positions[std::uint64_t{y} * inputs.fixedDimensions.x + x] = glm::vec2{
          (static_cast<float>(x) + 0.5f) / inputs.fixedDimensions.x,
          (static_cast<float>(y) + 0.5f) / inputs.fixedDimensions.y};
      }
    }
    if (!allocateSampleBuffer(
          GL_ARRAY_BUFFER,
          static_cast<GLsizeiptr>(planeSize * sizeof(glm::vec2)),
          positions.data()))
    {
      return allocationFailed();
    }
  }
  glEnableVertexAttribArray(0);
  glDisableVertexAttribArray(1);
  glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(glm::vec2), nullptr);
  glBindBuffer(GL_TRANSFORM_FEEDBACK_BUFFER, m_fixedValues);
  if (!allocateSampleBuffer(GL_TRANSFORM_FEEDBACK_BUFFER, static_cast<GLsizeiptr>(voxelCount * sizeof(float)), nullptr))
  {
    return allocationFailed();
  }
  glBindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER, 0, m_fixedValues);
  auto& program = *m_fixedSamplePrograms[variant];
  program.use();
  program.setSamplerUniform("u_fixedImage", 0);
  program.setUniform("u_fixedDepth", static_cast<int>(inputs.fixedDimensions.z));
  if (variant != 0) {
    program.setUniform("u_fixedAxes", inputs.layouts[0].axes);
  }
  program.setUniform("u_normalization", inputs.normalized_T_texture[0]);
  program.setUniform("u_textureComponent", inputs.textureComponents[0]);
  glEnable(GL_RASTERIZER_DISCARD);
  glBeginTransformFeedback(GL_POINTS);
  glDrawArraysInstanced(GL_POINTS, 0, static_cast<GLsizei>(planeSize), static_cast<GLsizei>(inputs.fixedDimensions.z));
  glEndTransformFeedback();
  glDisable(GL_RASTERIZER_DISCARD);
  glBindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER, 0, 0);
  glActiveTexture(GL_TEXTURE10);
  glBindTexture(GL_TEXTURE_BUFFER, m_fixedValuesTexture);
  glTexBuffer(GL_TEXTURE_BUFFER, GL_R32F, m_fixedValues);
  m_fixedSampleInputs = inputs;
  return true;
}

bool JointHistogramRenderer::isComplete() const
{
  return !m_inputsPending && m_cachedInputs &&
         m_processedVoxels == std::uint64_t{m_cachedInputs->fixedDimensions.x} * m_cachedInputs->fixedDimensions.y *
                                m_cachedInputs->fixedDimensions.z;
}

void JointHistogramRenderer::resolveBackground(const int bins)
{
  glActiveTexture(GL_TEXTURE9);
  glDisable(GL_BLEND);
  glBindFramebuffer(GL_FRAMEBUFFER, m_reductionFramebuffer);
  m_reduceProgram->use();
  m_reduceProgram->setSamplerUniform("u_source", 9);
  m_vao.bind();
  m_counts.bind(9u);
  for (int i = 0; i < 2; ++i) {
    const int size = i == 0 ? 4 : 1;
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_reductionTextures[i], 0);
    glViewport(0, 0, size, size);
    m_reduceProgram->setUniform("u_sourceOffset", glm::ivec2{0, i == 0 ? bins : 0});
    glDrawArrays(GL_TRIANGLES, 0, 3);
    glBindTexture(GL_TEXTURE_2D, m_reductionTextures[i]);
  }
  m_counts.bind(9u);
  glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, 1, 1);
}

bool JointHistogramRenderer::render(
  const Inputs& inputs,
  const joint_histogram::Plot& plot,
  const joint_histogram::Navigation& navigation,
  const Viewport& windowViewport)
{
  if (
    !std::isfinite(plot.left) || !std::isfinite(plot.top) || !std::isfinite(plot.size) || plot.size <= 0.0f ||
    !validInputs(inputs))
  {
    return false;
  }
  const std::optional<std::uint64_t> inputVoxelCount = voxelCount(inputs.fixedDimensions);
  if (!inputVoxelCount || *inputVoxelCount > static_cast<std::uint64_t>(std::numeric_limits<GLsizei>::max())) {
    return false;
  }
  const std::uint64_t voxelCount = *inputVoxelCount;

  const OpenGLStateGuard previousState{{9u, GL_TEXTURE_2D}, {10u, GL_TEXTURE_BUFFER}};
  const SampleStateGuard sampleState;
  glDisable(GL_RASTERIZER_DISCARD);
  initialize();

  for (auto& timing : m_timings) {
    if (!timing.pending) {
      continue;
    }
    GLuint available = GL_FALSE;
    glGetQueryObjectuiv(timing.query, GL_QUERY_RESULT_AVAILABLE, &available);
    if (available) {
      GLuint64 elapsed = 0;
      glGetQueryObjectui64v(timing.query, GL_QUERY_RESULT, &elapsed);
      // Tiny images mostly measure fixed draw/reduction overhead, not the cost of sampling a volume.
      if (timing.voxels >= kJointHistogramWorkChunkVoxels) {
        m_nanosecondsPerVoxel = jointHistogramUpdateCost(
          m_nanosecondsPerVoxel,
          static_cast<double>(elapsed) / static_cast<double>(timing.voxels));
      }
      timing.pending = false;
    }
  }

  const bool inputsChanged = !m_cachedInputs || !sameScatterInputs(*m_cachedInputs, inputs);
  m_inputsPending = inputsChanged;
  auto* timing = std::find_if(m_timings.begin(), m_timings.end(), [](const Timing& t) { return !t.pending; });
  if ((inputsChanged || m_processedVoxels < voxelCount) && timing != m_timings.end()) {
    // Accept only the latest requested transform, and keep the previous preview while the GPU is busy.
    ensureTexture(inputs.bins);
    const bool fast = prepareFixedSamples(inputs, voxelCount);
    const std::uint64_t planeSize = std::uint64_t{inputs.fixedDimensions.x} * inputs.fixedDimensions.y;
    const std::uint64_t targetVoxels = jointHistogramSubmissionLimit(m_nanosecondsPerVoxel, inputsChanged);
    if (inputsChanged) {
      m_cachedInputs = inputs;
      m_inputsPending = false;
      m_completedBatches = 0;
      m_processedVoxels = 0;
      // Keep the partition fixed throughout refinement so each voxel contributes exactly once.
      m_batchVoxelLimit = std::max<std::uint64_t>(
        fast ? inputs.fixedDimensions.z : 1u,
        std::clamp(targetVoxels, kJointHistogramWorkChunkVoxels, kJointHistogramPreviewVoxels));
      m_framebuffer.bind(fbo::TargetType::Draw);
      glDisable(GL_SCISSOR_TEST);
      glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
      const GLfloat clearValue[4] = {0.0f, 0.0f, 0.0f, 0.0f};
      glClearBufferfv(GL_COLOR, 0, clearValue);
    }
    const std::uint64_t batchCount =
      fast ? jointHistogramPlaneBatchCount(planeSize, inputs.fixedDimensions.z, m_batchVoxelLimit)
           : jointHistogramBatchCount(voxelCount, m_batchVoxelLimit);
    m_framebuffer.bind(fbo::TargetType::Draw);
    glViewport(0, 0, std::max(inputs.bins, kBackgroundTileSize), inputs.bins + kBackgroundTileSize);
    glDisable(GL_SCISSOR_TEST);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_STENCIL_TEST);
    glDisable(GL_CULL_FACE);
    glDisable(GL_MULTISAMPLE);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glEnable(GL_BLEND);
    glBlendEquation(GL_FUNC_ADD);
    glBlendFunc(GL_ONE, GL_ONE);
    const int variant = (TextureDimension::Texture2D == inputs.layouts[0].dimension ? 1 : 0) |
                        (TextureDimension::Texture2D == inputs.layouts[1].dimension ? 2 : 0);
    const int movingVariant = (variant & 2) != 0 ? 1 : 0;
    if (fast && !m_fastScatterPrograms[movingVariant]) {
      std::string source = shaderSource("JointHistogramFastScatter.vs");
      if (movingVariant != 0) {
        source.insert(source.find('\n') + 1, "#define MOVING_IMAGE_2D\n");
      }
      m_fastScatterPrograms[movingVariant] = makeProgram(
        "Joint histogram cached scatter",
        source,
        shaderSource("JointHistogramScatter.fs"),
        jointHistogramFastScatterUniforms(),
        Uniforms{});
    }
    GLShaderProgram& scatter = fast ? *m_fastScatterPrograms[movingVariant] : *m_scatterPrograms[variant];
    scatter.use();
    glUniform1i(location(scatter, "u_backgroundRows"), kBackgroundTileSize);
    glUniform1i(location(scatter, "u_bins"), inputs.bins);
    glUniform1i(location(scatter, "u_movingImage"), 1);
    if (fast) {
      glActiveTexture(GL_TEXTURE10);
      glBindTexture(GL_TEXTURE_BUFFER, m_fixedValuesTexture);
      scatter.setSamplerUniform("u_fixedValues", 10);
      scatter.setUniform("u_planeSize", static_cast<int>(planeSize));
      scatter.setUniform("u_fixedDepth", static_cast<int>(inputs.fixedDimensions.z));
      scatter.setUniform("u_moving_T_fixedTexture", inputs.texture_T_world[1] * inputs.world_T_fixedTexture);
      scatter.setUniform("u_normalization", inputs.normalized_T_texture[1]);
      scatter.setUniform("u_textureComponent", inputs.textureComponents[1]);
      if (movingVariant != 0) {
        scatter.setUniform("u_movingAxes", inputs.layouts[1].axes);
      }
    }
    else {
      glUniform1i(location(scatter, "u_fixedImage"), 0);
      glUniform2i(
        location(scatter, "u_fixedSizeXY"),
        static_cast<GLint>(inputs.fixedDimensions.x),
        static_cast<GLint>(inputs.fixedDimensions.y));
      glUniform1i(location(scatter, "u_fixedDepth"), static_cast<GLint>(inputs.fixedDimensions.z));
      glUniform2i(location(scatter, "u_textureComponents"), inputs.textureComponents[0], inputs.textureComponents[1]);
      setMatrix(scatter, "u_world_T_fixedTexture", inputs.world_T_fixedTexture);

      for (int i = 0; i < 2; ++i) {
        const std::string suffix = std::format("[{}]", i);
        setMatrix(scatter, "u_tex_T_world" + suffix, inputs.texture_T_world[i]);
        setMatrix(scatter, "u_defTex_T_world" + suffix, inputs.deformations[i].texture_T_world);
        setVector2(scatter, "u_normalized_T_texture" + suffix, inputs.normalized_T_texture[i]);
        glUniform2i(location(scatter, "u_tex2DAxes" + suffix), inputs.layouts[i].axes.x, inputs.layouts[i].axes.y);
        glUniform1f(location(scatter, "u_defSlope_native_T_texture" + suffix), inputs.deformations[i].native_T_texture);
        glUniform1f(location(scatter, "u_deformationStrength" + suffix), inputs.deformations[i].strength);
        glUniform1i(location(scatter, "u_defInterleaved" + suffix), inputs.deformations[i].interleaved);
        glUniform1i(location(scatter, "u_warpEnabled" + suffix), inputs.deformations[i].enabled);
        for (int component = 0; component < 3; ++component) {
          glUniform1i(location(scatter, std::format("u_defTex{}[{}]", i, component)), 3 + 3 * i + component);
        }
      }
    }

    glEnable(GL_PROGRAM_POINT_SIZE);
    m_vao.bind();
    if (fast) {
      glEnableVertexAttribArray(0);
    }
    else {
      glDisableVertexAttribArray(0);
      glDisableVertexAttribArray(1);
    }
    const auto submissionStart = std::chrono::steady_clock::now();
    std::uint64_t submittedVoxels = 0;
    glBeginQuery(GL_TIME_ELAPSED, timing->query);
    do {
      const JointHistogramBatch batch =
        fast ? jointHistogramBatch(planeSize, m_batchVoxelLimit / inputs.fixedDimensions.z, m_completedBatches)
             : jointHistogramBatch(voxelCount, m_batchVoxelLimit, m_completedBatches);
      if (fast) {
        glBindBuffer(GL_ARRAY_BUFFER, m_positions);
        glVertexAttribPointer(
          0,
          2,
          GL_FLOAT,
          GL_FALSE,
          static_cast<GLsizei>(batch.stride * sizeof(glm::vec2)),
          reinterpret_cast<const void*>(batch.offset * sizeof(glm::vec2)));
        scatter.setUniform("u_sampleOffset", static_cast<int>(batch.offset));
        scatter.setUniform("u_sampleStride", static_cast<int>(batch.stride));
        glDrawArraysInstanced(
          GL_POINTS,
          0,
          static_cast<GLsizei>(batch.size),
          static_cast<GLsizei>(inputs.fixedDimensions.z));
      }
      else {
        glUniform1i(location(scatter, "u_batchOffset"), static_cast<GLint>(batch.offset));
        glUniform1i(location(scatter, "u_batchStride"), static_cast<GLint>(batch.stride));
        glDrawArrays(GL_POINTS, 0, static_cast<GLsizei>(batch.size));
      }
      ++m_completedBatches;
      const std::uint64_t batchVoxels = batch.size * (fast ? inputs.fixedDimensions.z : 1u);
      m_processedVoxels += batchVoxels;
      submittedVoxels += batchVoxels;
    } while (m_completedBatches < batchCount && submittedVoxels + m_batchVoxelLimit <= targetVoxels &&
             std::chrono::steady_clock::now() - submissionStart < std::chrono::milliseconds{2});
    glDisableVertexAttribArray(0);
    glDisableVertexAttribArray(1);
    resolveBackground(inputs.bins);
    glEndQuery(GL_TIME_ELAPSED);
    timing->voxels = submittedVoxels;
    timing->pending = true;
    GLVertexArrayObject::unbind();
    GLShaderProgram::stopUse();
  }

  previousState.restoreFramebufferAndViewport();
  const glm::vec2 ratio = windowViewport.devicePixelRatio();
  const GLint x = static_cast<GLint>(std::lround(windowViewport.deviceLeft() + plot.left * ratio.x));
  const GLint y = static_cast<GLint>(
    std::lround(windowViewport.deviceBottom() + (windowViewport.height() - plot.top - plot.size) * ratio.y));
  const GLsizei width = static_cast<GLsizei>(std::lround(plot.size * ratio.x));
  const GLsizei height = static_cast<GLsizei>(std::lround(plot.size * ratio.y));
  glViewport(x, y, width, height);
  glEnable(GL_SCISSOR_TEST);
  glScissor(x, y, width, height);
  glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
  glDisable(GL_BLEND);
  m_counts.bind(9u);
  m_displayProgram->use();
  glUniform1i(location(*m_displayProgram, "u_counts"), 9);
  setVector2(
    *m_displayProgram,
    "u_histogramFraction",
    {static_cast<float>(m_allocatedBins) / std::max(m_allocatedBins, kBackgroundTileSize),
     static_cast<float>(m_allocatedBins) / (m_allocatedBins + kBackgroundTileSize)});
  glUniform1i(location(*m_displayProgram, "u_colormap"), 2);
  const float referenceCount = std::max(
    1.0f,
    static_cast<float>(m_processedVoxels) * 16.0f / static_cast<float>(m_allocatedBins * m_allocatedBins));
  glUniform1f(location(*m_displayProgram, "u_referenceCount"), referenceCount);
  glUniform1i(location(*m_displayProgram, "u_logarithmicScale"), inputs.logarithmicScale);
  setVector2(*m_displayProgram, "u_visibleMinimum", navigation.visibleMinimum());
  setVector2(*m_displayProgram, "u_visibleMaximum", navigation.visibleMaximum());
  setVector2(*m_displayProgram, "u_metricSlopeIntercept", inputs.metric.m_slopeIntercept);
  setVector2(*m_displayProgram, "u_cmapSlopeIntercept", inputs.metric.m_cmapSlopeIntercept);
  glUniform1i(
    location(*m_displayProgram, "u_cmapQuantizationLevels"),
    inputs.metric.m_cmapContinuous ? 0 : std::clamp(inputs.metric.m_cmapQuantizationLevels, 2, 1024));
  m_vao.bind();
  glDrawArrays(GL_TRIANGLES, 0, 3);
  GLVertexArrayObject::unbind();
  GLShaderProgram::stopUse();
  return !isComplete();
}

} // namespace rendering
