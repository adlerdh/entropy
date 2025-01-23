#include "rendering_old/drawables/Line.h"

#include "rendering/utility/UnderlyingEnumType.h"
#include "rendering/utility/gl/GLDrawTypes.h"
#include "rendering/utility/gl/GLShaderProgram.h"
#include "rendering_old/ShaderNames.h"

#include "common/Exception.hpp"

#include "logic/camera/MathUtility.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_inverse.hpp>

#include <algorithm>
#include <iostream>
#include <limits>
#include <numeric>

Line::Line(
  const std::string& name,
  ShaderProgramActivatorType shaderProgramActivator,
  UniformsProviderType uniformsProvider,
  const PrimitiveMode& primitiveMode
)
  : DrawableBase(name, DrawableType::Line)
  ,

  m_shaderProgramActivator(shaderProgramActivator)
  , m_uniformsProvider(uniformsProvider)
  ,

  m_vao()
  , m_vaoParams(nullptr)
  ,

  m_positionBuffer(BufferType::VertexArray, BufferUsagePattern::DynamicDraw)
  , m_indexBuffer(BufferType::Index, BufferUsagePattern::StaticDraw)
  ,

  // Create vertex position and info objects, initialized with zero vertices
  m_numVertices(0)
  ,

  m_positionInfo(
    BufferComponentType::Float, BufferNormalizeValues::False, 3, sizeof(glm::vec3), 0, m_numVertices
  )
  ,

  m_indexInfo(IndexType::UInt32, primitiveMode, m_numVertices, 0)
  ,

  m_clip_O_camera(1.0f)
  , m_camera_O_world(1.0f)
  ,

  m_solidColor(1.0f, 1.0f, 1.0f)
{
  m_renderId = static_cast<uint32_t>(underlyingType(m_type) << 12) | (numCreated() % 4096);

  if (m_uniformsProvider)
  {
    m_stdUniforms = m_uniformsProvider(FlatProgram::name);
    m_peelUniforms = m_uniformsProvider(FlatPeelProgram::name);
    m_initUniforms = m_uniformsProvider(DDPInitProgram::name);
  }
  else
  {
    throw_debug("Unable to access UniformsProvider");
  }

  if (!(PrimitiveMode::Lines == primitiveMode || PrimitiveMode::LineLoop == primitiveMode
        || PrimitiveMode::LineStrip == primitiveMode
        || PrimitiveMode::LinesAdjacency == primitiveMode
        || PrimitiveMode::LineStripAdjacency == primitiveMode))
  {
    throw_debug("Invalid primitive mode supplied to Line");
  }

  initBuffers();
  initVaos();
}

void Line::initBuffers()
{
  m_positionBuffer.generate();
  m_indexBuffer.generate();
}

void Line::initVaos()
{
  static constexpr GLuint k_positionsIndex = 0;

  m_vao.generate();
  m_vao.bind();
  {
    /// Bind EBO so that it is part of the VAO state
    m_indexBuffer.bind();

    m_positionBuffer.bind();
    m_vao.setAttributeBuffer(k_positionsIndex, m_positionInfo);
    m_vao.enableVertexAttribute(k_positionsIndex);
  }
  m_vao.release();

  m_vaoParams = std::make_unique<GLVertexArrayObject::IndexedDrawParams>(m_indexInfo);
}

DrawableOpacity Line::opacityFlag() const
{
  return DrawableOpacity{OpacityFlag::Unknown, OpacityFlag::Unknown};
}

void Line::doRender(const RenderStage& stage)
{
  if (0 == m_numVertices)
  {
    return;
  }

  if (!m_shaderProgramActivator)
  {
    throw_debug("Unable to access ShaderProgramActivator");
  }

  // Offset the line towards the viewer
  glEnable(GL_POLYGON_OFFSET_LINE);
  glPolygonOffset(-1.0f, -4.0f);

  // Note: It would be nice to thicken the line with
  // glLineWidth, but this is deprecated in GL 3.3

  switch (stage)
  {
  case RenderStage::Opaque:
  case RenderStage::Overlay:
  case RenderStage::QuadResolve:
  {
    using namespace FlatProgram;

    auto program = m_shaderProgramActivator(FlatProgram::name);

    if (!program)
    {
      throw_debug("Unable to access shader program");
    }

    const glm::mat4 world_O_this = getAccumulatedRenderingData().m_world_T_object;

    m_stdUniforms.setValue(vert::world_O_model, world_O_this);
    m_stdUniforms.setValue(vert::camera_O_world, m_camera_O_world);
    m_stdUniforms.setValue(vert::clip_O_camera, m_clip_O_camera);

    m_stdUniforms.setValue(vert::color, m_solidColor);

    m_stdUniforms.setValue(frag::objectId, m_renderId);
    m_stdUniforms.setValue(frag::opacity, masterOpacityMultiplier());

    program->applyUniforms(m_stdUniforms);
    break;
  }

  case RenderStage::DepthPeel:
  {
    using namespace FlatPeelProgram;

    auto program = m_shaderProgramActivator(FlatPeelProgram::name);

    if (!program)
    {
      throw_debug("Unable to access shader program");
    }

    const glm::mat4 world_O_this = getAccumulatedRenderingData().m_world_T_object;

    m_peelUniforms.setValue(vert::world_O_model, world_O_this);
    m_peelUniforms.setValue(vert::camera_O_world, m_camera_O_world);
    m_peelUniforms.setValue(vert::clip_O_camera, m_clip_O_camera);

    m_peelUniforms.setValue(vert::color, m_solidColor);
    m_peelUniforms.setValue(frag::opacity, masterOpacityMultiplier());

    m_peelUniforms.setValue(frag::depthBlenderTex, DepthBlenderTexSamplerIndex);
    m_peelUniforms.setValue(frag::frontBlenderTex, FrontBlenderTexSamplerIndex);

    program->applyUniforms(m_peelUniforms);
    break;
  }

  case RenderStage::Initialize:
  {
    using namespace DDPInitProgram;

    auto program = m_shaderProgramActivator(DDPInitProgram::name);

    if (!program)
    {
      throw_debug("Unable to access shader program");
    }

    m_initUniforms.setValue(vert::world_O_model, getAccumulatedRenderingData().m_world_T_object);
    m_initUniforms.setValue(vert::camera_O_world, m_camera_O_world);
    m_initUniforms.setValue(vert::clip_O_camera, m_clip_O_camera);

    m_initUniforms.setValue(frag::opaqueDepthTex, OpaqueDepthTexSamplerIndex);

    program->applyUniforms(m_initUniforms);
    break;
  }
  }

  m_vao.bind();
  m_vao.drawElements(*m_vaoParams);
  m_vao.release();

  glPolygonOffset(0.0f, 0.0f);
  glDisable(GL_POLYGON_OFFSET_LINE);
}

void Line::
  doUpdate(double /*time*/, const Viewport&, const camera::Camera& camera, const CoordinateFrame&)
{
  m_clip_O_camera = camera.clip_T_camera();
  m_camera_O_world = camera.camera_T_world();
}

void Line::setColor(const glm::vec3& color)
{
  m_solidColor = color;
}

void Line::setVertices(const float* vertexBuffer, size_t numVertices)
{
  if (!vertexBuffer || 0 == numVertices)
  {
    m_numVertices = 0;
    return;
  }

  if (std::numeric_limits<GLsizei>::max() < numVertices)
  {
    /// @todo Log
    std::cerr << "Maximum number of vertices exceeded" << std::endl;
    return;
  }

  if (m_numVertices != numVertices)
  {
    m_numVertices = numVertices;
    generateBuffers(vertexBuffer);
  }
  else
  {
    fillPositionsBuffer(vertexBuffer);
  }
}

void Line::generateBuffers(const float* vertexBuffer)
{
  std::vector<uint32_t> indices(m_numVertices);
  std::iota(std::begin(indices), std::end(indices), 0);

  m_indexBuffer.allocate(m_numVertices * sizeof(uint32_t), indices.data());
  m_positionBuffer.allocate(m_numVertices * sizeof(glm::vec3), vertexBuffer);

  m_positionInfo.setVertexCount(m_numVertices); // not needed
  m_indexInfo.setIndexCount(m_numVertices);     // not needed
  m_vaoParams->setElementCount(m_numVertices);
}

void Line::fillPositionsBuffer(const float* vertexBuffer)
{
  m_positionBuffer.write(0, m_numVertices * sizeof(glm::vec3), vertexBuffer);
}
