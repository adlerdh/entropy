#include "rendering_old/drawables/overlays/CameraLabel.h"

#include "common/Exception.hpp"

#include "rendering/utility/UnderlyingEnumType.h"
#include "rendering/utility/containers/VertexAttributeInfo.h"
#include "rendering/utility/containers/VertexIndicesInfo.h"
#include "rendering/utility/gl/GLDrawTypes.h"
#include "rendering/utility/gl/GLShaderProgram.h"
#include "rendering/utility/gl/GLTexture.h"
#include "rendering_old/ShaderNames.h"
#include "rendering_old/records/MeshGpuRecord.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_inverse.hpp>
#include <glm/gtc/matrix_transform.hpp>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/transform.hpp>

namespace
{

static const glm::mat4 sk_ident{1.0f};

static const glm::vec3 sk_white{1.0f, 1.0f, 1.0f};

static constexpr float sk_nearDist = 0.1f;
static constexpr float sk_farDist = 2.0f;

/**
 * @brief Compute the left, posterior, and superior directions of the subject in Camera space.
 *
 * @param[in] camera_O_world_rotation Rotation matrix from World to Camera space
 * @param[in] world_O_subject_rotation Rotation matrix from Subject to World space
 *
 * @return Left, posterior, and superior directions of the Subject in Camera space
 */
glm::mat3 computeSubjectAxesInCamera(
  const glm::mat3& camera_O_world_rotation, const glm::mat3& world_O_subject_rotation
)
{
  return glm::inverseTranspose(camera_O_world_rotation * world_O_subject_rotation);
}

} // namespace

const Uniforms::SamplerIndexType CameraLabel::TexSamplerIndex{0};

CameraLabel::CameraLabel(
  std::string name,
  ShaderProgramActivatorType shaderProgramActivator,
  UniformsProviderType uniformsProvider,
  GetterType<std::optional<glm::mat4> > subjectToWorldProvider,
  std::array<std::weak_ptr<GLTexture>, 6> letterTextures
)
  : DrawableBase(std::move(name), DrawableType::CameraLabel)
  ,

  m_shaderProgramActivator(shaderProgramActivator)
  , m_uniformsProvider(uniformsProvider)
  , m_subjectToWorldProvider(subjectToWorldProvider)
  ,

  m_vao()
  , m_vaoParams(nullptr)
  , m_meshGpuRecord(nullptr)
  , m_uniforms()
  ,

  m_labels()
  , m_clip_O_camera(glm::ortho(-1.0f, 1.0f, -1.0f, 1.0f, sk_nearDist, sk_farDist))
{
  m_renderId = static_cast<uint32_t>(underlyingType(m_type) << 12) | (numCreated() % 4096);

  if (m_uniformsProvider)
  {
    m_uniforms = m_uniformsProvider(SimpleProgram::name);
  }
  else
  {
    throw_debug("Unable to access UniformsProvider");
  }

  for (uint32_t i = 0; i < m_labels.size(); ++i)
  {
    m_labels[i].m_texture = letterTextures[i];
    m_labels[i].m_world_O_model = sk_ident;
    m_labels[i].m_solidColor = sk_white;
    m_labels[i].m_visible = false;
  }

  initBuffer();
  initVao();

  setMasterOpacityMultiplier(0.5f);
  setPickable(false);
}

CameraLabel::~CameraLabel() = default;

DrawableOpacity CameraLabel::opacityFlag() const
{
  return DrawableOpacity{OpacityFlag::Unknown, OpacityFlag::Unknown};
}

void CameraLabel::setSubjectToWorldProvider(GetterType<std::optional<glm::mat4> > provider)
{
  m_subjectToWorldProvider = provider;
}

void CameraLabel::initBuffer()
{
  static constexpr int sk_numVerts = 4;
  static constexpr int sk_numPosComps = 3;
  static constexpr int sk_numTCComps = 2;

  // Model-space coordinates of the label quads
  static const std::array<float, sk_numVerts * sk_numPosComps> sk_positionsBuffer = {{
    -1.0f,
    -1.0f,
    -1.0f, // bottom left
    1.0f,
    -1.0f,
    -1.0f, // bottom right
    -1.0f,
    1.0f,
    -1.0f, // top left
    1.0f,
    1.0f,
    -1.0f // top right
  }};

  // Texture coordinates are flipped vertically
  static const std::array<float, sk_numVerts * sk_numTCComps> sk_texCoordsBuffer = {{
    0.0f,
    1.0f, // bottom left
    1.0f,
    1.0f, // bottom right
    0.0f,
    0.0f, // top left
    1.0f,
    0.0f // top right
  }};

  static const std::array<uint32_t, sk_numVerts> sk_indicesBuffer = {{0, 1, 2, 3}};

  VertexAttributeInfo positionsInfo(
    BufferComponentType::Float,
    BufferNormalizeValues::False,
    sk_numPosComps,
    sk_numPosComps * sizeof(float),
    0,
    sk_numVerts
  );

  VertexAttributeInfo texCoordsInfo(
    BufferComponentType::Float,
    BufferNormalizeValues::False,
    sk_numTCComps,
    sk_numTCComps * sizeof(float),
    0,
    sk_numVerts
  );

  VertexIndicesInfo indexInfo(IndexType::UInt32, PrimitiveMode::TriangleStrip, sk_numVerts, 0);

  GLBufferObject positionsBuffer(BufferType::VertexArray, BufferUsagePattern::StaticDraw);
  GLBufferObject texCoordsBuffer(BufferType::VertexArray, BufferUsagePattern::StaticDraw);
  GLBufferObject indicesBuffer(BufferType::Index, BufferUsagePattern::StaticDraw);

  positionsBuffer.generate();
  texCoordsBuffer.generate();
  indicesBuffer.generate();

  positionsBuffer.allocate(sk_numVerts * sk_numPosComps * sizeof(float), sk_positionsBuffer.data());
  texCoordsBuffer.allocate(sk_numVerts * sk_numTCComps * sizeof(float), sk_texCoordsBuffer.data());
  indicesBuffer.allocate(sk_numVerts * sizeof(uint32_t), sk_indicesBuffer.data());

  m_meshGpuRecord = std::make_unique<
    MeshGpuRecord>(std::move(positionsBuffer), std::move(indicesBuffer), positionsInfo, indexInfo);

  m_meshGpuRecord->setTexCoords(std::move(texCoordsBuffer), texCoordsInfo);
}

void CameraLabel::initVao()
{
  static constexpr GLuint sk_positionsIndex = 0;
  static constexpr GLuint sk_texCoordsIndex = 1;

  if (!m_meshGpuRecord)
  {
    return;
  }

  const auto& positionsInfo = m_meshGpuRecord->positionsInfo();
  const auto& texCoordsInfo = m_meshGpuRecord->texCoordsInfo();
  const auto& indicesInfo = m_meshGpuRecord->indicesInfo();

  auto& positionsObject = m_meshGpuRecord->positionsObject();
  auto& texCoordsObject = m_meshGpuRecord->texCoordsObject();
  auto& indicesObject = m_meshGpuRecord->indicesObject();

  if (!texCoordsInfo || !texCoordsObject)
  {
    throw_debug("No mesh texture data");
  }

  m_vao.generate();
  m_vao.bind();
  {
    // Bind EBO so that it is part of the VAO state
    indicesObject.bind();

    positionsObject.bind();
    m_vao.setAttributeBuffer(sk_positionsIndex, positionsInfo);
    m_vao.enableVertexAttribute(sk_positionsIndex);

    texCoordsObject->bind();
    m_vao.setAttributeBuffer(sk_texCoordsIndex, *texCoordsInfo);
    m_vao.enableVertexAttribute(sk_texCoordsIndex);
  }
  m_vao.release();

  m_vaoParams = std::make_unique<GLVertexArrayObject::IndexedDrawParams>(indicesInfo);
}

void CameraLabel::doRender(const RenderStage& stage)
{
  if (!m_shaderProgramActivator)
  {
    throw_debug("Unable to access ShaderProgramActivator");
  }

  if (!m_vaoParams)
  {
    std::ostringstream ss;
    ss << "Null VAO parameters in " << m_name << std::ends;
    throw_debug(ss.str());
  }

  if (RenderStage::Overlay != stage)
  {
    return;
  }

  if (auto program = m_shaderProgramActivator(SimpleProgram::name))
  {
    using namespace SimpleProgram;

    // These uniforms are common to all labels:
    m_uniforms.setValue(frag::tex2D, TexSamplerIndex);
    m_uniforms.setValue(vert::camera_O_world, sk_ident);
    m_uniforms.setValue(vert::clip_O_camera, m_clip_O_camera);
    m_uniforms.setValue(frag::objectId, m_renderId);
    m_uniforms.setValue(frag::opacity, masterOpacityMultiplier());

    // Render all visible labels
    for (auto& label : m_labels)
    {
      if (label.m_visible)
      {
        auto texture = label.m_texture.lock();
        if (!texture)
        {
          continue;
        }

        texture->bind(TexSamplerIndex.index);

        m_uniforms.setValue(vert::color, label.m_solidColor);
        m_uniforms.setValue(vert::world_O_model, label.m_world_O_model);
        program->applyUniforms(m_uniforms);

        m_vao.bind();
        m_vao.drawElements(*m_vaoParams);
        m_vao.release();

        texture->unbind();
      }
    }
  }
  else
  {
    throw_debug("Null Simple shader program");
  }
}

void CameraLabel::doUpdate(
  double /*time*/,
  const Viewport& viewport,
  const camera::Camera& camera,
  const CoordinateFrame& /*crosshairs*/
)
{
  // Label width as a fraction of total view size:
  static constexpr float sk_labelSize = 0.025f;

  // Additional amount (in view pixels) by which to move in the labels:
  static constexpr float sk_pixelBorder = 7.0f;

  if (!m_subjectToWorldProvider)
  {
    setVisible(false);
    return;
  }

  const auto world_O_subject = m_subjectToWorldProvider();
  if (!world_O_subject)
  {
    setVisible(false);
    return;
  }

  setVisible(true);

  // Scale the label quad down in size and keep its aspect ratio square.
  // The scale is based on the largest view dimension.
  const float widthScale = sk_labelSize / viewport.aspectRatio();
  const float heightScale = sk_labelSize * viewport.aspectRatio();

  const glm::vec3 scaleVec = (widthScale > heightScale)
                               ? glm::vec3{widthScale, sk_labelSize, 1.0f}
                               : glm::vec3{sk_labelSize, heightScale, 1.0f};

  const glm::mat4 scaleTx = glm::scale(scaleVec);

  // The active image subject's left, posterior, and superior directions in Camera space.
  // Columns 0, 1, and 2 of the matrix correspond to left, posterior, and superior, respectively.
  const glm::mat3 axes
    = computeSubjectAxesInCamera(glm::mat3{camera.camera_T_world()}, glm::mat3{*world_O_subject});

  const glm::mat3 axesAbs{glm::abs(axes[0]), glm::abs(axes[1]), glm::abs(axes[2])};
  const glm::mat3 axesSgn{glm::sign(axes[0]), glm::sign(axes[1]), glm::sign(axes[2])};

  // Render the two sets of labels that are closest to the view plane:
  if (axesAbs[0].z > axesAbs[1].z && axesAbs[0].z > axesAbs[2].z)
  {
    m_labels[L].m_visible = false;
    m_labels[R].m_visible = false;
    m_labels[P].m_visible = true;
    m_labels[A].m_visible = true;
    m_labels[S].m_visible = true;
    m_labels[I].m_visible = true;
  }
  else if (axesAbs[1].z > axesAbs[0].z && axesAbs[1].z > axesAbs[2].z)
  {
    m_labels[L].m_visible = true;
    m_labels[R].m_visible = true;
    m_labels[P].m_visible = false;
    m_labels[A].m_visible = false;
    m_labels[S].m_visible = true;
    m_labels[I].m_visible = true;
  }
  else if (axesAbs[2].z > axesAbs[0].z && axesAbs[2].z > axesAbs[1].z)
  {
    m_labels[L].m_visible = true;
    m_labels[R].m_visible = true;
    m_labels[P].m_visible = true;
    m_labels[A].m_visible = true;
    m_labels[S].m_visible = false;
    m_labels[I].m_visible = false;
  }

  static const glm::vec3 sk_ndcMin{-1.0f, -1.0f, 0.0f};
  static const glm::vec3 sk_ndcMax{1.0f, 1.0f, 0.0f};

  const glm::vec3 invDims = glm::vec3{1.0f / viewport.width(), 1.0f / viewport.height(), 0.0f};

  const glm::vec3 ndcLabelMin = sk_ndcMin + glm::vec3{scaleVec.x, scaleVec.y, 0.0f}
                                + 2.0f * sk_pixelBorder * invDims;

  const glm::vec3 ndcLabelMax = sk_ndcMax - glm::vec3{scaleVec.x, scaleVec.y, 0.0f}
                                - 2.0f * sk_pixelBorder * invDims;

  // Compute the translation vectors for the L (0), P (1), and S (2) labels:
  glm::mat3 t;

  for (int i = 0; i < 3; ++i)
  {
    t[i] = (axesAbs[i].x > 0.0f && axesAbs[i].y / axesAbs[i].x <= 1.0f)
             ? glm::vec3{axesSgn[i].x, axesSgn[i].y * axesAbs[i].y / axesAbs[i].x, 0.0f}
             : glm::vec3{axesSgn[i].x * axesAbs[i].x / axesAbs[i].y, axesSgn[i].y, 0.0f};

    // Clamp, so that labels are not cut off:
    t[i] = glm::clamp(t[i], ndcLabelMin, ndcLabelMax);
  }

  m_labels[L].m_world_O_model = glm::translate(t[0]) * scaleTx;
  m_labels[R].m_world_O_model = glm::translate(-t[0]) * scaleTx;
  m_labels[P].m_world_O_model = glm::translate(t[1]) * scaleTx;
  m_labels[A].m_world_O_model = glm::translate(-t[1]) * scaleTx;
  m_labels[S].m_world_O_model = glm::translate(t[2]) * scaleTx;
  m_labels[I].m_world_O_model = glm::translate(-t[2]) * scaleTx;
}
