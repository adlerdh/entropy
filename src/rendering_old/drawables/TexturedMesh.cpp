#include "rendering_old/drawables/TexturedMesh.h"
#include "rendering/utility/CreateGLObjects.h"
#include "rendering/utility/UnderlyingEnumType.h"
#include "rendering/utility/gl/GLShaderProgram.h"
#include "rendering/utility/gl/GLTexture.h"
#include "rendering_old/ShaderNames.h"
#include "rendering_old/records/MeshGpuRecord.h"
#include "rendering_old/utility/containers/BlankTextures.h"

#include "logic/camera/CameraHelpers.h"
#include "logic/camera/MathUtility.h"

#include "common/Exception.hpp"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_inverse.hpp>

#include <iostream>
#include <utility>

namespace
{

static constexpr uint32_t sk_imageComp = 0;

static const glm::mat4 sk_ident{1.0f};

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

TexturedMesh::TexturedMesh(
  std::string name,
  ShaderProgramActivatorType shaderProgramActivator,
  UniformsProviderType uniformsProvider,
  std::weak_ptr<BlankTextures> blankTextures,
  GetterType<MeshGpuRecord*> meshGpuRecordProvider
)
  : DrawableBase(std::move(name), DrawableType::TexturedMesh)
  ,

  m_shaderProgramActivator(shaderProgramActivator)
  , m_uniformsProvider(uniformsProvider)
  , m_blankTextures(blankTextures)
  ,

  m_vao()
  , m_vaoParams(nullptr)
  , m_meshGpuRecordProvider(meshGpuRecordProvider)
  ,

  m_texture2d()
  , m_image3dRecord()
  , m_parcelRecord()
  , m_imageColorMapRecord()
  , m_labelsRecord()
  ,

  m_layerPermutation{
    0, // Material
    1, // Vertex
    2, // Image2D
    3, // Image3D
    4  // Label3D
  }
  ,

  m_layerOpacities{
    1.0f, // Material
    0.0f, // Vertex
    0.0f, // Image2D
    0.0f, // Image3D
    0.0f  // Label3D
  }
  ,

  m_layerOpacityMultipliers{1.0f, 0.0f, 0.0f, 0.0f, 0.0f}
  , m_finalLayerOpacities{1.0f, 0.0f, 0.0f, 0.0f, 0.0f}
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

  m_autoHidingMode(false)
  , m_image2dThresholdMode(false)
  , m_image3dThresholdMode(false)
  , m_image2dThresholdActive(false)
  , m_image3dThresholdActive(false)
  ,

  m_xrayMode(false)
  , m_xrayPower(3.0f)
  ,

  m_texture2dThresholds{0.0f, 1.0f}
  ,

  //      m_labelTexCoords_O_view( 1.0f ),

  m_enablePolygonOffset(false)
  , m_polygonOffsetFactor(0.0f)
  , m_polygonOffsetUnits(0.0f)
{
  setRenderId((static_cast<uint32_t>(underlyingType(m_type)) << 12) | (numCreated() % 4096));

  if (m_uniformsProvider)
  {
    m_stdUniforms = m_uniformsProvider(MeshProgram::name);
    m_peelUniforms = m_uniformsProvider(MeshDDPPeelProgram::name);
    m_initUniforms = m_uniformsProvider(DDPInitProgram::name);
  }
  else
  {
    throw_debug("Unable to access UniformsProvider");
  }

  initVao();
}

bool TexturedMesh::isOpaque() const
{
  if (m_autoHidingMode || m_xrayMode || (m_image2dThresholdMode && m_image2dThresholdActive) || (m_image3dThresholdMode && m_image3dThresholdActive))
  {
    // Since fragment opacity is modulated when these modes are active,
    // there is no guarantee that the fragment is opaque
    return false;
  }

  if ( m_overallOpacity * // combined opacity of all layers
         getAccumulatedRenderingData().m_masterOpacityMultiplier // master opacity multiplier
         < 1.0f )
  {
    return false;
  }

  // If the mesh is textured with a partially transparent image
  // (e.g. a histology slide with some pixel alpha < 1),
  // then it may NOT be true that the mesh is opaque.
  // We'll deal with this later. For now, return that the mesh is opaque.
  return true;
}

DrawableOpacity TexturedMesh::opacityFlag() const
{
  return DrawableOpacity{OpacityFlag::Unknown, OpacityFlag::Unknown};
}

void TexturedMesh::setTexture2d(std::weak_ptr<GLTexture> texture)
{
  m_texture2d = texture;
}

void TexturedMesh::setTexture2dThresholds(glm::vec2 thresholds)
{
  m_texture2dThresholds = std::move(thresholds);
}

void TexturedMesh::setImage3dRecord(std::weak_ptr<ImageRecord> imageRecord)
{
  m_image3dRecord = imageRecord;

  auto rec = m_image3dRecord.lock();

  if (rec && rec->cpuData())
  {
    const auto& H = rec->cpuData()->header();
    const auto& type = H.memoryComponentType();

    if (ComponentType::Long == type || ComponentType::ULong == type || ComponentType::Float64 == type)
    {
      // Component types Int64, UInt64, and Double64 are not supported
      std::ostringstream ss;
      ss << "Invalid component type " << H.memoryComponentTypeAsString() << " for image "
         << H.fileName() << std::ends;
      throw_debug(ss.str());
    }
  }
}

void TexturedMesh::setParcellationRecord(std::weak_ptr<ParcellationRecord> record)
{
  m_parcelRecord = record;
}

void TexturedMesh::setImageColorMapRecord(std::weak_ptr<ImageColorMapRecord> record)
{
  m_imageColorMapRecord = record;
}

void TexturedMesh::setLabelTableRecord(std::weak_ptr<LabelTableRecord> record)
{
  m_labelsRecord = record;
}

std::weak_ptr<ImageRecord> TexturedMesh::image3dRecord()
{
  return m_image3dRecord;
}

std::weak_ptr<ParcellationRecord> TexturedMesh::parcelRecord()
{
  return m_parcelRecord;
}

void TexturedMesh::setLayerPermutation(
  const std::array<TexturedMeshColorLayer, static_cast<size_t>(TexturedMeshColorLayer::NumLayers)>& perm
)
{
  for (size_t order = 0; order < static_cast<size_t>(TexturedMeshColorLayer::NumLayers); ++order)
  {
    m_layerPermutation[order] = underlyingType(perm.at(order));
  }
}

void TexturedMesh::setLayerOpacityMultiplier(TexturedMeshColorLayer layer, float m)
{
  if (0.0f <= m && m <= 1.0f)
  {
    m_layerOpacityMultipliers[underlyingType(layer)] = m;
    updateLayerOpacities();
  }
}

float TexturedMesh::getLayerOpacityMultiplier(TexturedMeshColorLayer layer) const
{
  return m_layerOpacityMultipliers[underlyingType(layer)];
}

void TexturedMesh::setLayerOpacity(TexturedMeshColorLayer layer, float a)
{
  if (0.0f <= a && a <= 1.0f)
  {
    m_layerOpacities[underlyingType(layer)] = a;
    updateLayerOpacities();
  }
}

float TexturedMesh::getLayerOpacity(TexturedMeshColorLayer layer) const
{
  return m_layerOpacities[underlyingType(layer)];
}

void TexturedMesh::enableLayer(TexturedMeshColorLayer layer)
{
  // Enabled layer is fully opaque:
  m_layerOpacities[underlyingType(layer)] = 1.0f;
  updateLayerOpacities();
}

void TexturedMesh::disableLayer(TexturedMeshColorLayer layer)
{
  // Disabled layer is fully transparent:
  m_layerOpacities[underlyingType(layer)] = 0.0f;
  updateLayerOpacities();
}

void TexturedMesh::setMaterialColor(const glm::vec3& color)
{
  m_materialColor = color;
}

glm::vec3 TexturedMesh::getMaterialColor() const
{
  return m_materialColor;
}

void TexturedMesh::setMaterialShininess(float s)
{
  if (0.0f <= s)
  {
    m_materialShininess = s;
  }
}

float TexturedMesh::getMaterialShininess() const
{
  return m_materialShininess;
}

void TexturedMesh::setBackfaceCull(bool set)
{
  m_backfaceCull = set;
}

void TexturedMesh::setUseAutoHidingMode(bool set)
{
  m_autoHidingMode = set;
}

void TexturedMesh::setUseImage2dThresholdMode(bool set)
{
  m_image2dThresholdMode = set;
}

void TexturedMesh::setUseImage3dThresholdMode(bool set)
{
  m_image3dThresholdMode = set;
}

void TexturedMesh::setImage2dThresholdsActive(bool set)
{
  m_image2dThresholdActive = set;
}

void TexturedMesh::setImage3dThresholdsActive(bool set)
{
  m_image3dThresholdActive = set;
}

void TexturedMesh::setUseXrayMode(bool set)
{
  m_xrayMode = set;
}

void TexturedMesh::setXrayPower(float p)
{
  m_xrayPower = p;
}

bool TexturedMesh::getBackfaceCull() const
{
  return m_backfaceCull;
}

void TexturedMesh::setEnablePolygonOffset(bool enable)
{
  m_enablePolygonOffset = enable;
}

void TexturedMesh::setPolygonOffsetValues(float factor, float units)
{
  m_polygonOffsetFactor = factor;
  m_polygonOffsetUnits = units;
}

void TexturedMesh::setUseOctantClipPlanes(bool set)
{
  m_useOctantClipPlanes = set;
}

void TexturedMesh::setAmbientLightFactor(float f)
{
  if (0.0f <= f && f <= 1.0f)
  {
    m_ambientLightFactor = f;
  }
}

void TexturedMesh::setDiffuseLightFactor(float f)
{
  if (0.0f <= f && f <= 1.0f)
  {
    m_diffuseLightFactor = f;
  }
}

void TexturedMesh::setSpecularLightFactor(float f)
{
  if (0.0f <= f && f <= 1.0f)
  {
    m_specularLightFactor = f;
  }
}

void TexturedMesh::setAdsLightFactors(float a, float d, float s)
{
  setAmbientLightFactor(a);
  setDiffuseLightFactor(d);
  setSpecularLightFactor(s);
}

//void Mesh::addClippingPlane() {}

void TexturedMesh::initVao()
{
  static constexpr GLuint sk_positionIndex = 0;
  static constexpr GLuint sk_normalIndex = 1;
  static constexpr GLuint sk_texCoords2DIndex = 2;
  static constexpr GLuint sk_colorIndex = 3;

  if (!m_meshGpuRecordProvider)
  {
    throw_debug("Null mesh GPU record");
  }

  auto meshGpuRecord = m_meshGpuRecordProvider();
  if (!meshGpuRecord)
  {
    std::ostringstream ss;
    ss << "Null VAO parameters in " << m_name << std::ends;
    throw_debug(ss.str());
  }

  const auto& positionsInfo = meshGpuRecord->positionsInfo();
  const auto& normalsInfo = meshGpuRecord->normalsInfo();
  const auto& indicesInfo = meshGpuRecord->indicesInfo();
  const auto& texCoordsInfo = meshGpuRecord->texCoordsInfo();
  const auto& colorsInfo = meshGpuRecord->colorsInfo();

  auto& positionsObject = meshGpuRecord->positionsObject();
  auto& normalsObject = meshGpuRecord->normalsObject();
  auto& indicesObject = meshGpuRecord->indicesObject();
  auto& texCoordsObject = meshGpuRecord->texCoordsObject();
  auto& colorsObject = meshGpuRecord->colorsObject();

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

    if (texCoordsObject && texCoordsInfo)
    {
      texCoordsObject->bind();
      m_vao.setAttributeBuffer(sk_texCoords2DIndex, *texCoordsInfo);
      m_vao.enableVertexAttribute(sk_texCoords2DIndex);
    }
    else
    {
      // static const glm::vec2 sk_defaultTexCoord{ 0.0f, 0.0f };
      m_vao.disableVertexAttribute(sk_texCoords2DIndex);
      // m_vao.setGenericAttribute2f( k_texCoordsIndex, sk_defaultTexCoord );
    }

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

void TexturedMesh::doSetupState()
{
  /// @todo This function should store state prior to setting it
  //    glEnable( GL_CLIP_DISTANCE0 );
  //    glPolygonMode( GL_FRONT_AND_BACK, GL_LINE );

  //    glHint( GL_FRAGMENT_SHADER_DERIVATIVE_HINT, GL_NICEST );
}

void TexturedMesh::doTeardownState()
{
  /// @todo This function should restore stored state
  //    glDisable( GL_CULL_FACE );
  //    glDisable( GL_CLIP_DISTANCE0 );
}

/// @note Need to set uniforms every render, in case another Mesh has set them
void TexturedMesh::doRender(const RenderStage& stage)
{
  static const Uniforms::SamplerIndexType sk_tex2DUnit{2};
  static const Uniforms::SamplerIndexType sk_image3DUnit{3};
  static const Uniforms::SamplerIndexType sk_label3DUnit{4};
  static const Uniforms::SamplerIndexType sk_labelColorMapTexUnit{5};
  static const Uniforms::SamplerIndexType sk_imageColorMapTexUnit{6};

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
    shaderProgram = m_shaderProgramActivator(MeshProgram::name);
    uniforms = &m_stdUniforms;
    break;
  }
  case RenderStage::DepthPeel:
  {
    shaderProgram = m_shaderProgramActivator(MeshDDPPeelProgram::name);
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
    using namespace MeshDDPPeelProgram;

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

    uniforms->setValue(
      frag::masterOpacityMultiplier, getAccumulatedRenderingData().m_masterOpacityMultiplier
    );
    uniforms->setValue(frag::autoHidingMode, m_autoHidingMode);
    uniforms->setValue(frag::image3DThresholdMode, m_image3dThresholdMode);
    uniforms->setValue(frag::xrayMode, m_xrayMode);
    uniforms->setValue(frag::xrayPower, m_xrayPower);

    //        uniforms->setValue( frag::labelTexCoords_O_view, m_labelTexCoords_O_view );

    uniforms->setValue(frag::layerOpacities, m_finalLayerOpacities);
    uniforms->setValue(frag::layerPermutation, m_layerPermutation);

    if (RenderStage::DepthPeel == stage)
    {
      uniforms->setValue(frag::depthBlenderTex, DepthBlenderTexSamplerIndex);
      uniforms->setValue(frag::frontBlenderTex, FrontBlenderTexSamplerIndex);
    }

    uniforms->setValue(frag::tex2D, sk_tex2DUnit);
    uniforms->setValue(frag::imageTex3D, sk_image3DUnit);
    uniforms->setValue(frag::labelTex3D, sk_label3DUnit);
    uniforms->setValue(frag::labelColormapTexture, sk_labelColorMapTexUnit);

    if (auto texture = m_texture2d.lock())
    {
      /// @todo: include sampler object in GL texture
      texture->bind(sk_tex2DUnit.index);
      texture->bindSampler(sk_tex2DUnit.index);

      uniforms->setValue(frag::image2dThresholds, m_texture2dThresholds);
    }
    else
    {
      if (auto blankTextures = m_blankTextures.lock())
      {
        blankTextures->bindImageTexture2D(sk_tex2DUnit.index);
      }
    }

    std::optional<glm::mat4> imageSubject_O_world;

    if (auto imageRecord = m_image3dRecord.lock())
    {
      auto cpuRecord = imageRecord->cpuData();
      auto gpuRecord = imageRecord->gpuData();

      if (cpuRecord && gpuRecord)
      {
        if (auto texture = gpuRecord->texture().lock())
        {
          /// @todo: include sampler object in GL texture
          texture->bind(sk_image3DUnit.index);
          texture->bindSampler(sk_image3DUnit.index);

          imageSubject_O_world = cpuRecord->transformations().subject_T_worldDef();

          uniforms->setValue(
            vert::imageTexCoords_O_world, cpuRecord->transformations().texture_T_worldDef()
          );
        }
      }
    }
    else
    {
      if (auto blankTextures = m_blankTextures.lock())
      {
        uniforms->setValue(vert::imageTexCoords_O_world, sk_ident);
        blankTextures->bindImageTexture3D(sk_image3DUnit.index);
      }
    }

    if (auto parcelRecord = m_parcelRecord.lock())
    {
      auto gpuRecord = parcelRecord->gpuData();
      auto cpuRecord = parcelRecord->cpuData();

      if (gpuRecord && cpuRecord)
      {
        if (auto texture = gpuRecord->texture().lock())
        {
          texture->bind(sk_label3DUnit.index);

          glm::mat4 parcelTexture_O_world(1.0f);

          if (imageSubject_O_world)
          {
            // If there is an image defined, then use its subject_O_world transformation
            // for the parcellation as well
            parcelTexture_O_world = cpuRecord->transformations().texture_T_subject()
                                    * (*imageSubject_O_world);
          }
          else
          {
            parcelTexture_O_world = cpuRecord->transformations().texture_T_worldDef();
          }

          uniforms->setValue(vert::labelTexCoords_O_world, parcelTexture_O_world);
        }
      }
    }
    else
    {
      if (auto blankTextures = m_blankTextures.lock())
      {
        uniforms->setValue(vert::labelTexCoords_O_world, sk_ident);
        blankTextures->bindLabelTexture3D(static_cast<uint32_t>(sk_label3DUnit.index));
      }
    }

    if (auto cmapRecord = m_imageColorMapRecord.lock())
    {
      if (auto colorMapTexture = cmapRecord->gpuData())
      {
        if (colorMapTexture->size().x > 0)
        {
          colorMapTexture->bind(sk_imageColorMapTexUnit.index);

          const float N = static_cast<float>(colorMapTexture->size().x);
          uniforms->setValue(frag::cmapSlope, (N - 1.0f) / N);
          uniforms->setValue(frag::cmapIntercept, 0.5f / N);
        }
        else
        {
          std::cerr << "Color map texture has zero size" << std::endl;
        }
      }
    }

    if (auto labelTableRecord = m_labelsRecord.lock())
    {
      if (auto colorTableTexture = labelTableRecord->gpuData())
      {
        colorTableTexture->bind(sk_labelColorMapTexUnit.index);
      }
    }

    if (auto imageRecord = m_image3dRecord.lock())
    {
      const auto& imageSettings = imageRecord->cpuData()->settings();

      /// @note Used to be called slopeIntercept()
      const auto si = imageSettings.slopeIntercept_normalized_T_native(sk_imageComp);
      uniforms->setValue(frag::slope, static_cast<float>(si.first));
      uniforms->setValue(frag::intercept, static_cast<float>(si.second));

      /// @note These used to be called thresholdLowNormalized, which is why slope/intercept
      /// are now used to convert to normalized units
      uniforms->setValue(
        frag::thresholds,
        glm::vec2{
          si.first * imageSettings.thresholds(sk_imageComp).first + si.second,
          si.first * imageSettings.thresholds(sk_imageComp).second + si.second
        }
      );
    }

    shaderProgram->applyUniforms(*uniforms);
  }

  m_vao.bind();
  {
    m_vao.drawElements(*m_vaoParams);
  }
  m_vao.release();

  if (auto texture = m_texture2d.lock())
  {
    // Note: unbind here wrecks render when mesh has transparency.
    // texture->unbind();
    texture->unbindSampler(sk_tex2DUnit.index);
  }

  if (auto imageRecord = m_image3dRecord.lock())
  {
    if (auto gpuRecord = imageRecord->gpuData())
    {
      if (auto texture = gpuRecord->texture().lock())
      {
        // Note: unbind here wrecks render when mesh has transparency.
        // texture->unbind();
        texture->unbindSampler(sk_image3DUnit.index);
      }
    }
  }

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

void TexturedMesh::doUpdate(
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

  static constexpr size_t sk_comp = 0;

  auto image3dRecord = m_image3dRecord.lock();
  if (image3dRecord && image3dRecord->cpuData())
  {
    const auto& S = image3dRecord->cpuData()->settings();
    setLayerOpacity(TexturedMeshColorLayer::Image3D, static_cast<float>(S.opacity(sk_comp)));
    setImage3dThresholdsActive(S.thresholdsActive(0));
  }
  else
  {
    setLayerOpacity(TexturedMeshColorLayer::Image3D, 0.0f);
  }

  auto label3dRecord = m_parcelRecord.lock();
  if (label3dRecord && label3dRecord->cpuData())
  {
    const auto S = label3dRecord->cpuData()->settings();
    setLayerOpacity(TexturedMeshColorLayer::Parcellation3D, static_cast<float>(S.opacity(sk_comp)));

    // Transformation from view to 3D parcellation coordinates
    //        m_labelTexCoords_O_view =
    //                label3dRecord->cpuData()->transformations().texture_O_world() *
    //                camera::world_O_clip( camera ) *
    //                camera::get_ndc_O_view( viewport );
  }
  else
  {
    setLayerOpacity(TexturedMeshColorLayer::Parcellation3D, 0.0f);

    //        m_labelTexCoords_O_view = glm::mat4{ 1.0f };
  }

  updateLayerOpacities();
}

void TexturedMesh::updateLayerOpacities()
{
  m_finalLayerOpacities = multiplyArrays(m_layerOpacities, m_layerOpacityMultipliers);
  m_overallOpacity = math::computeOverallOpacity(m_finalLayerOpacities);
}
