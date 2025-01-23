#include "rendering_old/drawables/BasicMesh.h"

#include "rendering/utility/CreateGLObjects.h"
#include "rendering/utility/UnderlyingEnumType.h"
#include "rendering/utility/gl/GLShaderProgram.h"
#include "rendering_old/ShaderNames.h"
#include "rendering_old/records/MeshGpuRecord.h"

#include "common/Exception.hpp"

#include "logic/camera/CameraHelpers.h"
#include "logic/camera/MathUtility.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_inverse.hpp>

#include <iostream>
#include <numeric>
#include <unordered_map>
#include <utility>

namespace
{

template<typename T, size_t N>
std::array<T, N> multiplyArrays(const std::array<T, N>& a, const std::array<T, N>& b)
{
  std::array<T, N> c;
  for (size_t i = 0; i < N; ++i)
  {
    c[i] = a[i] * b[i];
  }
  return c;
}

} // namespace

BasicMesh::BasicMesh(
  std::string name,
  ShaderProgramActivatorType shaderProgramActivator,
  UniformsProviderType uniformsProvider,
  std::weak_ptr<MeshGpuRecord> meshGpuRecord
)
  : DrawableBase(std::move(name), DrawableType::BasicMesh)
  ,

  m_shaderProgramActivator(shaderProgramActivator)
  , m_uniformsProvider(uniformsProvider)
  ,

  m_vao()
  , m_vaoParams(nullptr)
  , m_meshGpuRecord(meshGpuRecord)
  ,

  m_layerOpacities{
    1.0f, // Material
    0.0f  // Vertex
  }
  ,

  m_layerOpacityMultipliers{1.0f, 0.0f}
  , m_finalLayerOpacities{1.0f, 0.0f}
  ,

  m_overallOpacity(math::computeOverallOpacity(m_finalLayerOpacities))
  ,

  m_stdUniforms()
  , m_initUniforms()
  , m_peelUniforms()
  ,

  m_clip_O_camera(1.0f)
  , m_camera_O_world(1.0f)
  , m_cameraIsOrthographic(true)
  ,

  m_worldCameraPos(0.0f, 0.0f, 0.0f)
  , m_worldCameraDir(0.0f, 0.0f, 1.0f)
  , m_worldLightPos(0.0f, 0.0f, 0.0f)
  , m_worldLightDir(0.0f, 0.0f, 1.0f)
  ,

  m_useOctantClipPlanes(false)
  , m_worldClipPlanes{{glm::vec4{0.0f}, glm::vec4{0.0f}, glm::vec4{0.0f}}}
  ,

  m_materialColor(1.0f, 1.0f, 1.0f)
  , m_materialShininess(18.0f)
  ,

  m_ambientLightColor(1.0f, 1.0f, 1.0f)
  , m_diffuseLightColor(1.0f, 1.0f, 1.0f)
  , m_specularLightColor(1.0f, 1.0f, 1.0f)
  ,

  // The A, D, and S light factors don't need to sum to one:
  m_ambientLightFactor(0.20f)
  , m_diffuseLightFactor(0.55f)
  , m_specularLightFactor(0.25f)
  ,

  // In x-ray mode, the ambient lighting contribution is bumped up,
  // so that edges are more brightly lit
  m_xrayAmbientLightFactor(0.85f)
  , m_xrayDiffuseLightFactor(0.55f)
  , m_xraySpecularLightFactor(0.25f)
  ,

  m_wireframe(false)
  , m_backfaceCull(false)
  ,

  m_xrayMode(false)
  , m_xrayPower(3.0f)
  ,

  m_enablePolygonOffset(false)
  , m_polygonOffsetFactor(0.0f)
  , m_polygonOffsetUnits(0.0f)
{
  setRenderId((static_cast<uint32_t>(underlyingType(m_type)) << 12) | (numCreated() % 4096));

  m_stdShaderName = BasicMeshProgram::name;
  m_peelShaderName = BasicMeshDualDepthPeelProgram::name;

  if (m_uniformsProvider)
  {
    m_stdUniforms = m_uniformsProvider(m_stdShaderName);
    m_peelUniforms = m_uniformsProvider(m_peelShaderName);
    m_initUniforms = m_uniformsProvider(DDPInitProgram::name);
  }
  else
  {
    throw_debug("Unable to access UniformsProvider");
  }

  initVao();
}

bool BasicMesh::isOpaque() const
{
  if (m_xrayMode)
  {
    // Since fragment opacity is modulated when these modes are active,
    // there is no guarantee that the fragment is opaque
    return false;
  }

  if ( m_overallOpacity * // combined opacity of all layers
         masterOpacityMultiplier() // master opacity multiplier
         < 1.0f )
  {
    return false;
  }

  return true;
}

DrawableOpacity BasicMesh::opacityFlag() const
{
  return DrawableOpacity{OpacityFlag::Unknown, OpacityFlag::Unknown};
}

void BasicMesh::setMeshGpuRecord(std::weak_ptr<MeshGpuRecord> meshGpuRecord)
{
  m_meshGpuRecord = meshGpuRecord;
  initVao();
}

std::weak_ptr<MeshGpuRecord> BasicMesh::meshGpuRecord()
{
  return m_meshGpuRecord;
}

void BasicMesh::setLayerOpacityMultiplier(BasicMeshColorLayer layer, float m)
{
  if (0.0f <= m && m <= 1.0f)
  {
    m_layerOpacityMultipliers[underlyingType(layer)] = m;
    updateLayerOpacities();
  }
}

float BasicMesh::getLayerOpacityMultiplier(BasicMeshColorLayer layer) const
{
  return m_layerOpacityMultipliers[underlyingType(layer)];
}

void BasicMesh::setLayerOpacity(BasicMeshColorLayer layer, float a)
{
  if (0.0f <= a && a <= 1.0f)
  {
    m_layerOpacities[underlyingType(layer)] = a;
    updateLayerOpacities();
  }
}

float BasicMesh::getLayerOpacity(BasicMeshColorLayer layer) const
{
  return m_layerOpacities[underlyingType(layer)];
}

void BasicMesh::enableLayer(BasicMeshColorLayer layer)
{
  // Enabled layer is fully opaque:
  m_layerOpacities[underlyingType(layer)] = 1.0f;
}

void BasicMesh::disableLayer(BasicMeshColorLayer layer)
{
  // Disabled layer is fully transparent:
  m_layerOpacities[underlyingType(layer)] = 0.0f;
}

void BasicMesh::setMaterialColor(const glm::vec3& color)
{
  m_materialColor = color;
}

glm::vec3 BasicMesh::getMaterialColor() const
{
  return m_materialColor;
}

void BasicMesh::setMaterialShininess(float s)
{
  if (0.0f <= s)
  {
    m_materialShininess = s;
  }
}

float BasicMesh::getMaterialShininess() const
{
  return m_materialShininess;
}

void BasicMesh::setBackfaceCull(bool set)
{
  m_backfaceCull = set;
}

void BasicMesh::setUseXrayMode(bool set)
{
  m_xrayMode = set;
}

void BasicMesh::setXrayPower(float p)
{
  m_xrayPower = p;
}

bool BasicMesh::getBackfaceCull() const
{
  return m_backfaceCull;
}

void BasicMesh::setEnablePolygonOffset(bool enable)
{
  m_enablePolygonOffset = enable;
}

void BasicMesh::setPolygonOffsetValues(float factor, float units)
{
  m_polygonOffsetFactor = factor;
  m_polygonOffsetUnits = units;
}

void BasicMesh::setUseOctantClipPlanes(bool set)
{
  m_useOctantClipPlanes = set;
}

void BasicMesh::setAmbientLightFactor(float f)
{
  if (0.0f <= f && f <= 1.0f)
  {
    m_ambientLightFactor = f;
  }
}

void BasicMesh::setDiffuseLightFactor(float f)
{
  if (0.0f <= f && f <= 1.0f)
  {
    m_diffuseLightFactor = f;
  }
}

void BasicMesh::setSpecularLightFactor(float f)
{
  if (0.0f <= f && f <= 1.0f)
  {
    m_specularLightFactor = f;
  }
}

void BasicMesh::setAdsLightFactors(float a, float d, float s)
{
  setAmbientLightFactor(a);
  setDiffuseLightFactor(d);
  setSpecularLightFactor(s);
}

void BasicMesh::initVao()
{
  static constexpr GLuint sk_positionIndex = 0;
  static constexpr GLuint sk_normalIndex = 1;
  static constexpr GLuint sk_colorIndex = 2;

  auto gpuRec = m_meshGpuRecord.lock();
  if (!gpuRec)
  {
    std::ostringstream ss;
    ss << "Null VAO parameters in " << m_name << std::ends;
    throw_debug(ss.str());
  }

  const auto& positionsInfo = gpuRec->positionsInfo();
  const auto& normalsInfo = gpuRec->normalsInfo();
  const auto& indicesInfo = gpuRec->indicesInfo();
  const auto& colorsInfo = gpuRec->colorsInfo();

  auto& positionsObject = gpuRec->positionsObject();
  auto& normalsObject = gpuRec->normalsObject();
  auto& indicesObject = gpuRec->indicesObject();
  auto& colorsObject = gpuRec->colorsObject();

  if (!normalsObject || !normalsInfo)
  {
    throw_debug("No mesh normals");
  }

  m_vao.generate();
  m_vao.bind();
  {
    // Bind EBO so that it is part of the VAO state
    indicesObject.bind();

    // Saves binding in VAO, since GL_ARRAY_BUFFER is not part of VAO state.
    // Register position VBO with VAO and set/enable attribute pointer
    positionsObject.bind();
    m_vao.setAttributeBuffer(sk_positionIndex, positionsInfo);
    m_vao.enableVertexAttribute(sk_positionIndex);

    normalsObject->bind();
    m_vao.setAttributeBuffer(sk_normalIndex, *normalsInfo);
    m_vao.enableVertexAttribute(sk_normalIndex);

    if (colorsObject && colorsInfo)
    {
      colorsObject->bind();
      m_vao.setAttributeBuffer(sk_colorIndex, *colorsInfo);
      m_vao.enableVertexAttribute(sk_colorIndex);
    }
    else
    {
      // static const glm::vec4 sk_defaultColor{ 0.0f, 0.0f, 0.0f, 0.0f };
      m_vao.disableVertexAttribute(sk_colorIndex);
      // m_vao.setGenericAttribute4f( k_colorIndex, sk_defaultColor );
    }
  }
  m_vao.release();

  m_vaoParams = std::make_unique<GLVertexArrayObject::IndexedDrawParams>(indicesInfo);
}

void BasicMesh::doSetupState()
{
  /// @todo This function should store state prior to setting it
  //    glEnable( GL_CLIP_DISTANCE0 );
  //    glPolygonMode( GL_FRONT_AND_BACK, GL_LINE );

  //    glHint( GL_FRAGMENT_SHADER_DERIVATIVE_HINT, GL_NICEST );
}

void BasicMesh::doTeardownState()
{
  /// @todo This function should restore stored state
  //    glDisable( GL_CULL_FACE );
  //    glDisable( GL_CLIP_DISTANCE0 );
}

/// @note Need to set uniforms every render, in case another Mesh has set them
void BasicMesh::doRender(const RenderStage& stage)
{
  static const glm::vec3 sk_materialSpecular{1.0f, 1.0f, 1.0f};

  if (!m_shaderProgramActivator)
  {
    throw_debug("Unable to access ShaderProgramActivator");
  }

  GLShaderProgram* shaderProgram = nullptr;
  Uniforms* uniforms = nullptr;

  switch (stage)
  {
  case RenderStage::Initialize:
  {
    shaderProgram = m_shaderProgramActivator(DDPInitProgram::name);
    uniforms = &m_initUniforms;
    break;
  }
  case RenderStage::Opaque:
  case RenderStage::Overlay:
  case RenderStage::QuadResolve:
  {
    shaderProgram = m_shaderProgramActivator(m_stdShaderName);
    uniforms = &m_stdUniforms;
    break;
  }
  case RenderStage::DepthPeel:
  {
    shaderProgram = m_shaderProgramActivator(m_peelShaderName);
    uniforms = &m_peelUniforms;
    break;
  }
  }

  if (!shaderProgram)
  {
    throw_debug("Null shader program");
  }

  if (!uniforms)
  {
    throw_debug("Null uniforms");
  }

  if (!m_vaoParams)
  {
    std::ostringstream ss;
    ss << "Null VAO parameters in " << m_name << std::ends;
    throw_debug(ss.str());
  }

  /// @todo Put these into doSetupState?
  if (m_wireframe)
  {
    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
  }
  else
  {
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
  }

  if (m_backfaceCull)
  {
    glEnable(GL_CULL_FACE);
    glFrontFace(GL_CCW);
    glCullFace(GL_BACK);
  }

  if (m_enablePolygonOffset)
  {
    glEnable(GL_POLYGON_OFFSET_FILL);
    glPolygonOffset(m_polygonOffsetFactor, m_polygonOffsetUnits);
  }

  if (RenderStage::Initialize == stage)
  {
    using namespace DDPInitProgram;

    m_initUniforms.setValue(vert::world_O_model, getAccumulatedRenderingData().m_world_T_object);
    m_initUniforms.setValue(vert::camera_O_world, m_camera_O_world);
    m_initUniforms.setValue(vert::clip_O_camera, m_clip_O_camera);

    for (uint32_t i = 0; i < 3; ++i)
    {
      m_initUniforms.setValue(vert::worldClipPlanes[i], m_worldClipPlanes[i]);
    }

    m_initUniforms.setValue(frag::opaqueDepthTex, OpaqueDepthTexSamplerIndex);

    shaderProgram->applyUniforms(m_initUniforms);
  }
  else
  {
    using namespace BasicMeshDualDepthPeelProgram;

    const glm::mat4 world_O_this = getAccumulatedRenderingData().m_world_T_object;

    uniforms->setValue(vert::world_O_model, world_O_this);
    uniforms->setValue(vert::world_O_model_inv_trans, glm::inverseTranspose(world_O_this));
    uniforms->setValue(vert::camera_O_world, m_camera_O_world);
    uniforms->setValue(vert::clip_O_camera, m_clip_O_camera);

    for (uint32_t i = 0; i < 3; ++i)
    {
      uniforms->setValue(vert::worldClipPlanes[i], m_worldClipPlanes[i]);
    }

    uniforms->setValue(frag::material_diffuse, m_materialColor);
    uniforms->setValue(frag::material_specular, sk_materialSpecular);
    uniforms->setValue(frag::material_shininess, m_materialShininess);

    uniforms->setValue(frag::simpleLight_ambient, m_ambientLightColor);
    uniforms->setValue(frag::simpleLight_diffuse, m_diffuseLightColor);
    uniforms->setValue(frag::simpleLight_specular, m_specularLightColor);
    uniforms->setValue(frag::simpleLight_position, m_worldLightPos);
    uniforms->setValue(frag::simpleLight_direction, m_worldLightDir);

    uniforms->setValue(frag::cameraPos, m_worldCameraPos);
    uniforms->setValue(frag::cameraDir, m_worldCameraDir);
    uniforms->setValue(frag::cameraIsOrthographic, m_cameraIsOrthographic);

    uniforms->setValue(frag::objectId, m_renderId);

    uniforms->setValue(frag::masterOpacityMultiplier, masterOpacityMultiplier());
    uniforms->setValue(frag::xrayMode, m_xrayMode);
    uniforms->setValue(frag::xrayPower, m_xrayPower);

    uniforms->setValue(frag::layerOpacities, m_finalLayerOpacities);

    if (RenderStage::DepthPeel == stage)
    {
      uniforms->setValue(frag::depthBlenderTex, DepthBlenderTexSamplerIndex);
      uniforms->setValue(frag::frontBlenderTex, FrontBlenderTexSamplerIndex);
    }

    shaderProgram->applyUniforms(*uniforms);
  }

  m_vao.bind();
  {
    m_vao.drawElements(*m_vaoParams);
  }
  m_vao.release();

  // Reset default GL states:

  if (m_wireframe)
  {
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
  }

  if (m_backfaceCull)
  {
    glDisable(GL_CULL_FACE);
  }

  if (m_enablePolygonOffset)
  {
    glPolygonOffset(0.0f, 0.0f);
    glDisable(GL_POLYGON_OFFSET_FILL);
  }
}

void BasicMesh::doUpdate(
  double, const Viewport&, const camera::Camera& camera, const CoordinateFrame& crosshairs
)
{
  static const glm::vec4 sk_lightColor{1.0f, 1.0f, 1.0f, 1.0f};

  if (m_xrayMode)
  {
    m_ambientLightColor = m_xrayAmbientLightFactor * sk_lightColor;
    m_diffuseLightColor = m_xrayDiffuseLightFactor * sk_lightColor;
    m_specularLightColor = m_xraySpecularLightFactor * sk_lightColor;
  }
  else
  {
    m_ambientLightColor = m_ambientLightFactor * sk_lightColor;
    m_diffuseLightColor = m_diffuseLightFactor * sk_lightColor;
    m_specularLightColor = m_specularLightFactor * sk_lightColor;
  }

  m_clip_O_camera = camera.clip_T_camera();
  m_camera_O_world = camera.camera_T_world();

  m_cameraIsOrthographic = camera.isOrthographic();

  m_worldCameraPos = worldOrigin(camera);
  m_worldCameraDir = worldDirection(camera, Directions::View::Back);

  m_worldLightPos = m_worldCameraPos;
  m_worldLightDir = m_worldCameraDir;

  if (m_useOctantClipPlanes)
  {
    for (uint32_t i = 0; i < 3; ++i)
    {
      // Orient the plane to clip toward the camera normal direction
      const glm::vec3 worldNormalDir = crosshairs.world_T_frame()[static_cast<int>(i)] * -1.0f
                                       * glm::sign(glm::dot(m_worldCameraDir, worldNormalDir));

      m_worldClipPlanes[i].x = worldNormalDir.x;
      m_worldClipPlanes[i].y = worldNormalDir.y;
      m_worldClipPlanes[i].z = worldNormalDir.z;
      m_worldClipPlanes[i].w = -glm::dot(worldNormalDir, crosshairs.worldOrigin());
    }
  }
  else
  {
    static const glm::vec4 k_zero{0.0f};

    for (uint32_t i = 0; i < 3; ++i)
    {
      m_worldClipPlanes[i] = k_zero;
    }
  }

  updateLayerOpacities();
}

void BasicMesh::updateLayerOpacities()
{
  m_finalLayerOpacities = multiplyArrays(m_layerOpacities, m_layerOpacityMultipliers);
  m_overallOpacity = math::computeOverallOpacity(m_finalLayerOpacities);
}
