#include "rendering_old/assemblies/MeshAssembly.h"
#include "rendering_old/ShaderNames.h"
#include "rendering_old/common/MeshColorLayer.h"
#include "rendering_old/drawables/DynamicTransformation.h"
#include "rendering_old/drawables/TexturedMesh.h"
#include "rendering_old/drawables/Transformation.h"

#include "common/Exception.hpp"

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/string_cast.hpp>

#include <iostream>
#include <sstream>
#include <tuple>

namespace
{

static const glm::mat4 sk_ident{1.0f};

// Default material:
static const glm::vec3 sk_defaultMaterialColor(0.5f);
static constexpr float sk_defaultMaterialAlpha = 1.0f;

static constexpr bool sk_defaultVisibility = true;

} // namespace

MeshAssembly::MeshAssembly(
  ShaderProgramActivatorType shaderProgramActivator,
  UniformsProviderType uniformsProvider,
  std::weak_ptr<BlankTextures> blankTextures
)
  : m_shaderActivator(shaderProgramActivator)
  , m_uniformsProvider(uniformsProvider)
  , m_blankTextures(blankTextures)
  ,

  m_meshSubjectToWorldQuerier(nullptr)
  ,

  m_image3dRecord()
  , m_parcelRecord()
  , m_imageColorMapRecord()
  , m_labelTableRecord()
  ,

  m_rootTx2d(nullptr)
  , m_rootTx3d(nullptr)
  , m_meshes()
  , m_properties()
{
}

void MeshAssembly::initialize()
{
  std::ostringstream ss;
  ss << "MeshAssemblyRoot2d_#" << numCreated() << std::ends;
  m_rootTx2d = std::make_shared<Transformation>(ss.str(), sk_ident);

  ss.str(std::string());
  ss << "MeshAssemblyRoot3d_#" << numCreated() << std::ends;
  m_rootTx3d = std::make_shared<Transformation>(ss.str(), sk_ident);
}

std::weak_ptr<DrawableBase> MeshAssembly::getRoot(const SceneType& type)
{
  switch (type)
  {
  case SceneType::ReferenceImage2d:
  case SceneType::SlideStack2d:
  case SceneType::Registration_Image2d:
  case SceneType::Registration_Slide2d:
  {
    return std::static_pointer_cast<DrawableBase>(m_rootTx2d);
  }
  case SceneType::ReferenceImage3d:
  case SceneType::SlideStack3d:
  {
    return std::static_pointer_cast<DrawableBase>(m_rootTx3d);
  }
  default:
  case SceneType::None:
  {
    return {};
  }
  }

  return {};
}

void MeshAssembly::addMesh(const uuids::uuid& meshUid, std::weak_ptr<MeshRecord> meshRecord)
{
  const auto& itr = m_meshes.find(meshUid);
  if (std::end(m_meshes) != itr)
  {
    // Meshes for this UID already exist in the Assembly
    return;
  }

  // No meshes with this UID exists, so create them:
  MeshDrawables M;

  M.m_meshRecord = meshRecord;

  auto subjectToWorldTxProvider = [this, meshUid]() -> std::optional<glm::mat4>
  {
    if (m_meshSubjectToWorldQuerier)
    {
      return m_meshSubjectToWorldQuerier(meshUid);
    }
    return std::nullopt;
  };

  auto meshGpuRecordProvider = [meshRecord]() -> MeshGpuRecord*
  {
    if (auto record = meshRecord.lock())
    {
      return record->gpuData();
    }
    return nullptr;
  };

  std::ostringstream ss;

  ss << "Mesh2dTx@" << meshUid << std::ends;
  M.m_world_O_subject_for2d
    = std::make_shared<DynamicTransformation>(ss.str(), subjectToWorldTxProvider);

  ss.str(std::string());
  ss << "Mesh2d@" << meshUid << std::ends;

  M.m_meshFor2d = std::make_shared<TexturedMesh>(
    ss.str(), m_shaderActivator, m_uniformsProvider, m_blankTextures, meshGpuRecordProvider
  );

  M.m_world_O_subject_for2d->addChild(M.m_meshFor2d);

  ss.str(std::string());
  ss << "Mesh3dTx@" << meshUid << std::ends;

  M.m_world_O_subject_for3d
    = std::make_shared<DynamicTransformation>(ss.str(), subjectToWorldTxProvider);

  ss.str(std::string());
  ss << "Mesh3d@" << meshUid << std::ends;

  M.m_meshFor3d = std::make_shared<TexturedMesh>(
    ss.str(), m_shaderActivator, m_uniformsProvider, m_blankTextures, meshGpuRecordProvider
  );

  M.m_world_O_subject_for3d->addChild(M.m_meshFor3d);

  // Initialize with default color:
  M.m_meshFor2d->setMaterialColor(sk_defaultMaterialColor);
  M.m_meshFor3d->setMaterialColor(sk_defaultMaterialColor);

  M.m_meshFor2d->enableLayer(TexturedMeshColorLayer::Material);
  M.m_meshFor2d->enableLayer(TexturedMeshColorLayer::Image3D);
  M.m_meshFor2d->enableLayer(TexturedMeshColorLayer::Parcellation3D);
  M.m_meshFor2d->disableLayer(TexturedMeshColorLayer::Vertex);
  M.m_meshFor2d->disableLayer(TexturedMeshColorLayer::Image2D);

  M.m_meshFor2d->setLayerOpacityMultiplier(TexturedMeshColorLayer::Material, 1.0f);
  M.m_meshFor2d->setLayerOpacityMultiplier(TexturedMeshColorLayer::Image3D, 0.0f);
  M.m_meshFor2d->setLayerOpacityMultiplier(TexturedMeshColorLayer::Parcellation3D, 0.0f);

  M.m_meshFor3d->enableLayer(TexturedMeshColorLayer::Material);
  M.m_meshFor3d->enableLayer(TexturedMeshColorLayer::Image3D);
  M.m_meshFor3d->enableLayer(TexturedMeshColorLayer::Parcellation3D);
  M.m_meshFor3d->disableLayer(TexturedMeshColorLayer::Vertex);
  M.m_meshFor3d->disableLayer(TexturedMeshColorLayer::Image2D);

  M.m_meshFor3d->setLayerOpacityMultiplier(TexturedMeshColorLayer::Material, 1.0f);
  M.m_meshFor3d->setLayerOpacityMultiplier(TexturedMeshColorLayer::Image3D, 0.0f);
  M.m_meshFor3d->setLayerOpacityMultiplier(TexturedMeshColorLayer::Parcellation3D, 0.0f);

  // Add to Assembly roots:
  m_rootTx2d->addChild(M.m_world_O_subject_for2d);
  m_rootTx3d->addChild(M.m_world_O_subject_for3d);

  // Store Drawables:
  m_meshes.emplace(meshUid, M);

  setImage3dRecord(m_image3dRecord);
  setParcellationRecord(m_parcelRecord);
  setImageColorMapRecord(m_imageColorMapRecord);
  setLabelTableRecord(m_labelTableRecord);

  updateMeshRenderingProperties();
  updateMeshColors();
}

void MeshAssembly::removeMesh(const uuids::uuid& meshUid)
{
  const auto& itr = m_meshes.find(meshUid);

  if (std::end(m_meshes) != itr)
  {
    if (auto tx = itr->second.m_world_O_subject_for2d)
    {
      m_rootTx2d->removeChild(*tx);
    }

    if (auto tx = itr->second.m_world_O_subject_for3d)
    {
      m_rootTx3d->removeChild(*tx);
    }

    m_meshes.erase(meshUid);
  }
  else
  {
    std::cerr << "Error: mesh with UID " << meshUid << " not found in collection." << std::endl;
    return;
  }
}

void MeshAssembly::clearMeshes()
{
  for (auto& mesh : m_meshes)
  {
    if (auto tx = mesh.second.m_world_O_subject_for2d)
    {
      m_rootTx2d->removeChild(*tx);
    }

    if (auto tx = mesh.second.m_world_O_subject_for3d)
    {
      m_rootTx3d->removeChild(*tx);
    }
  }

  m_meshes.clear();
}

void MeshAssembly::setImage3dRecord(std::weak_ptr<ImageRecord> record)
{
  m_image3dRecord = record;

  for (auto& m : m_meshes)
  {
    if (m.second.m_meshFor2d)
    {
      m.second.m_meshFor2d->setImage3dRecord(record);
    }

    if (m.second.m_meshFor3d)
    {
      m.second.m_meshFor3d->setImage3dRecord(record);
    }
  }

  // Image record affects mesh colors:
  updateMeshColors();
}

void MeshAssembly::setParcellationRecord(std::weak_ptr<ParcellationRecord> record)
{
  m_parcelRecord = record;

  for (auto& m : m_meshes)
  {
    if (m.second.m_meshFor2d)
    {
      m.second.m_meshFor2d->setParcellationRecord(record);
    }

    if (m.second.m_meshFor3d)
    {
      m.second.m_meshFor3d->setParcellationRecord(record);
    }
  }
}

void MeshAssembly::setImageColorMapRecord(std::weak_ptr<ImageColorMapRecord> record)
{
  m_imageColorMapRecord = record;

  for (auto& m : m_meshes)
  {
    if (m.second.m_meshFor2d)
    {
      m.second.m_meshFor2d->setImageColorMapRecord(record);
    }

    if (m.second.m_meshFor3d)
    {
      m.second.m_meshFor3d->setImageColorMapRecord(record);
    }
  }

  // Image color map affects mesh colors:
  updateMeshColors();
}

void MeshAssembly::setLabelTableRecord(std::weak_ptr<LabelTableRecord> record)
{
  m_labelTableRecord = record;

  for (auto& m : m_meshes)
  {
    if (m.second.m_meshFor2d)
    {
      m.second.m_meshFor2d->setLabelTableRecord(record);
    }

    if (m.second.m_meshFor3d)
    {
      m.second.m_meshFor3d->setLabelTableRecord(record);
    }
  }

  // Label table affects mesh colors:
  updateMeshColors();
}

void MeshAssembly::setMeshSubjectToWorldTxQuerier(
  QuerierType<std::optional<glm::mat4>, uuids::uuid> querier
)
{
  m_meshSubjectToWorldQuerier = querier;

  auto subjectToWorldTxProvider = [this](const uuids::uuid& meshUid) -> std::optional<glm::mat4>
  {
    if (m_meshSubjectToWorldQuerier)
    {
      return m_meshSubjectToWorldQuerier(meshUid);
    }
    return std::nullopt;
  };

  // Propagate new querier to all stored meshes:
  for (auto& mesh : m_meshes)
  {
    if (auto tx = mesh.second.m_world_O_subject_for2d)
    {
      tx->setMatrixProvider(std::bind(subjectToWorldTxProvider, mesh.first));
    }

    if (auto tx = mesh.second.m_world_O_subject_for3d)
    {
      tx->setMatrixProvider(std::bind(subjectToWorldTxProvider, mesh.first));
    }
  }
}

void MeshAssembly::setMasterOpacityMultiplier(float multiplier)
{
  m_properties.m_masterOpacityMultiplier = multiplier;
  updateMeshRenderingProperties();
}

void MeshAssembly::setUseOctantClipPlanes(bool use)
{
  m_properties.m_useOctantClipPlanes = use;
  updateMeshRenderingProperties();
}

void MeshAssembly::setShowIn2dViews(bool visible)
{
  m_properties.m_visibleIn2dViews = visible;
  updateMeshRenderingProperties();
}

void MeshAssembly::setShowIn3dViews(bool visible)
{
  m_properties.m_visibleIn3dViews = visible;
  updateMeshRenderingProperties();
}

void MeshAssembly::setUseXrayMode(bool set)
{
  m_properties.m_useXrayMode = set;
  updateMeshRenderingProperties();
}

void MeshAssembly::setXrayPower(float power)
{
  if (power > 0.0f)
  {
    m_properties.m_xrayPower = power;
  }

  updateMeshRenderingProperties();
}

void MeshAssembly::setPickable(bool pickable)
{
  m_properties.m_pickable = pickable;
  updateMeshRenderingProperties();
}

const MeshAssemblyRenderingProperties& MeshAssembly::getRenderingProperties() const
{
  return m_properties;
}

void MeshAssembly::updateMeshRenderingProperties()
{
  for (auto& m : m_meshes)
  {
    if (auto mesh = m.second.m_meshFor2d)
    {
      mesh->setEnabled(m_properties.m_visibleIn2dViews);
      mesh->setMasterOpacityMultiplier(m_properties.m_masterOpacityMultiplier);
      mesh->setPickable(m_properties.m_pickable);
      mesh->setUseOctantClipPlanes(m_properties.m_useOctantClipPlanes);
      mesh->setUseXrayMode(m_properties.m_useXrayMode);
      mesh->setXrayPower(m_properties.m_xrayPower);
    }

    if (auto mesh = m.second.m_meshFor3d)
    {
      mesh->setEnabled(m_properties.m_visibleIn3dViews);
      mesh->setMasterOpacityMultiplier(m_properties.m_masterOpacityMultiplier);
      mesh->setPickable(m_properties.m_pickable);
      mesh->setUseOctantClipPlanes(m_properties.m_useOctantClipPlanes);
      mesh->setUseXrayMode(m_properties.m_useXrayMode);
      mesh->setXrayPower(m_properties.m_xrayPower);
    }
  }
}

void MeshAssembly::updateMeshColors()
{
  auto sharedImageRec = m_image3dRecord.lock();
  auto sharedCmapRec = m_imageColorMapRecord.lock();
  auto sharedLabelsRec = m_labelTableRecord.lock();

  const Image* image = (sharedImageRec) ? sharedImageRec->cpuData() : nullptr;
  const ImageColorMap* colorMap = (sharedCmapRec) ? sharedCmapRec->cpuData() : nullptr;
  const ParcellationLabelTable* labelTable = (sharedLabelsRec) ? sharedLabelsRec->cpuData()
                                                               : nullptr;

  for (auto& M : m_meshes)
  {
    auto m2d = M.second.m_meshFor2d;
    auto m3d = M.second.m_meshFor3d;

    if (!m2d || !m3d)
    {
      continue;
    }

    auto meshRecord = M.second.m_meshRecord.lock();
    if (!meshRecord || !meshRecord->cpuData())
    {
      continue;
    }

    const MeshInfo& meshInfo = meshRecord->cpuData()->meshInfo();

    // First set defaults:
    bool visible = sk_defaultVisibility;
    glm::vec3 materialColor = sk_defaultMaterialColor;
    float materialAlpha = sk_defaultMaterialAlpha;

    if (labelTable && MeshSource::Label == meshInfo.meshSource())
    {
      // Update the label mesh color according to the label index in the table:
      const uint32_t labelIndex = meshInfo.labelIndex();

      if (labelIndex < labelTable->numLabels())
      {
        visible = labelTable->getShowMesh(labelIndex);
        materialColor = labelTable->getColor(labelIndex);
        materialAlpha = labelTable->getAlpha(labelIndex);
      }
    }
    else if (image && colorMap && MeshSource::IsoSurface == meshInfo.meshSource())
    {
      /// @todo This section needs to be executed when the image, color map,
      /// or WL change.

      // Update the iso-surface mesh color according to the image color map
      // at the iso intensity value:
      const double isoValue = meshInfo.isoValue();

      // Get window-level coefficients (slope, intercept) such that
      // y = slope * isoValue + intercept
      // is normalized to [0, 1] for input image intensity 'isoValue':
      double slope = 1.0;
      double intercept = 0.0;

      /// @note this used to be slopeIntercept
      std::tie(slope, intercept) = image->settings().slopeIntercept_normalized_T_native(0);

      // Index into color map according at iso-value after window-level mapping:
      const size_t N = colorMap->numColors();
      if (N == 0)
      {
        continue;
      }

      const double y = std::min(std::max(slope * isoValue + intercept, 0.0), 1.0);
      const size_t i = std::min(static_cast<size_t>(static_cast<double>(N - 1) * y), N - 1);
      const glm::vec4 color = colorMap->color_RGBA_F32(i); // Pre-multiplied RGBA

      materialAlpha = 1.0f;
      visible = true; // Iso meshes are always enabled (for now)

      if (color.a > 0.0f)
      {
        materialColor = glm::vec3{color / color.a};
      }
    }

    m2d->setMaterialColor(materialColor);
    m2d->setLayerOpacityMultiplier(TexturedMeshColorLayer::Material, materialAlpha);
    m2d->setVisible(visible);

    m3d->setMaterialColor(materialColor);
    m3d->setLayerOpacityMultiplier(TexturedMeshColorLayer::Material, materialAlpha);
    m3d->setVisible(visible);
  }
}
