#include "rendering_old/assemblies/LandmarkAssembly.h"

#include "rendering_old/ShaderNames.h"
#include "rendering_old/drawables/Transformation.h"
#include "rendering_old/drawables/annotation/LandmarkGroup3d.h"
#include "rendering_old/records/MeshGpuRecord.h"

#include "common/Exception.hpp"

#include <glm/glm.hpp>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/string_cast.hpp>

#include <iostream>
#include <sstream>

namespace
{

static const glm::mat4 sk_ident{1.0f};

} // namespace

LandmarkAssembly::LandmarkAssembly(
  ShaderProgramActivatorType shaderProgramActivator,
  UniformsProviderType uniformsProvider,
  GetterType<std::unique_ptr<MeshGpuRecord> > meshGpuRecordProvider,
  LmGroupToWorldTxQuerierType landmarkGroupToWorldTxQuerier,
  QuerierType<DrawableScaling, uuids::uuid> landarkScalingQuerier
)
  : m_shaderActivator(shaderProgramActivator)
  , m_uniformsProvider(uniformsProvider)
  ,

  m_meshGpuRecordProvider(meshGpuRecordProvider)
  , m_landmarkGroupToWorldTxQuerier(landmarkGroupToWorldTxQuerier)
  , m_landmarkScalingQuerier(landarkScalingQuerier)
  ,

  m_meshGpuRecord(nullptr)
  ,

  m_rootFor2dViews(nullptr)
  , m_rootFor3dViews(nullptr)
  ,

  m_lmDrawables()
  , m_lmProperties()
{
}

void LandmarkAssembly::initialize()
{
  if (m_meshGpuRecordProvider)
  {
    // Convert unique pointer of record to shared pointer:
    m_meshGpuRecord = m_meshGpuRecordProvider();
  }
  else
  {
    throw_debug("Unable to obtain mesh GPU record");
  }

  std::ostringstream ss;
  ss << "LandmarkAssembly_Root2d_#" << numCreated() << std::ends;
  m_rootFor2dViews = std::make_shared<Transformation>(ss.str(), sk_ident);

  ss.str(std::string());
  ss << "LandmarkAssembly_Root3d_#" << numCreated() << std::ends;
  m_rootFor3dViews = std::make_shared<Transformation>(ss.str(), sk_ident);
}

std::weak_ptr<DrawableBase> LandmarkAssembly::getRoot(const SceneType& type)
{
  switch (type)
  {
  case SceneType::ReferenceImage2d:
  case SceneType::SlideStack2d:
  case SceneType::Registration_Image2d:
  case SceneType::Registration_Slide2d:
  {
    return std::static_pointer_cast<DrawableBase>(m_rootFor2dViews);
  }
  case SceneType::ReferenceImage3d:
  case SceneType::SlideStack3d:
  {
    return std::static_pointer_cast<DrawableBase>(m_rootFor3dViews);
  }
  default:
  case SceneType::None:
  {
    return {};
  }
  }

  return {};
}

void LandmarkAssembly::setLandmarkGroupToWorldTxQuerier(LmGroupToWorldTxQuerierType querier)
{
  m_landmarkGroupToWorldTxQuerier = querier;
}

void LandmarkAssembly::setLandmarkGroupScalingQuerier(
  QuerierType<DrawableScaling, uuids::uuid> querier
)
{
  m_landmarkScalingQuerier = querier;
}

void LandmarkAssembly::addLandmarkGroup(std::weak_ptr<LandmarkGroupRecord> lmGroupRecord)
{
  if (!m_rootFor2dViews || !m_rootFor3dViews)
  {
    throw_debug("Null root drawables in assembly");
  }

  // Function that provides the tranformation from modeling space of the landmark group to World space
  auto lmToWorldTxProvider = [this,
                              lmGroupRecord]() -> std::optional<std::pair<glm::mat4, glm::mat4> >
  {
    if (auto lmg = lmGroupRecord.lock())
    {
      if (lmg && m_landmarkGroupToWorldTxQuerier)
      {
        return m_landmarkGroupToWorldTxQuerier(lmg->uid());
      }
    }
    return std::nullopt;
  };

  // Function that provides the scaling information for the landmark drawables in a landmark group
  auto lmScalingProvider = [this, lmGroupRecord]() -> std::optional<DrawableScaling>
  {
    if (auto lmg = lmGroupRecord.lock())
    {
      if (lmg && m_landmarkScalingQuerier)
      {
        return m_landmarkScalingQuerier(lmg->uid());
      }
    }
    return std::nullopt;
  };

  auto lmg = lmGroupRecord.lock();
  if (!lmg)
  {
    std::cerr << "Error: Null landmark group cannot be added to assembly" << std::endl;
    return;
  }

  const auto itr = m_lmDrawables.find(lmg->uid());
  if (std::end(m_lmDrawables) != itr)
  {
    // The landmark group already exists in the assembly, so first remove it
    removeLandmarkGroup(lmg->uid());
  }

  // Create the new drawables for 2D and 3D views:

  auto lm2d = std::make_shared<LandmarkGroup3d>(
    "lm2d",
    m_shaderActivator,
    m_uniformsProvider,
    lmGroupRecord,
    m_meshGpuRecord,
    lmScalingProvider,
    lmToWorldTxProvider
  );

  auto lm3d = std::make_shared<LandmarkGroup3d>(
    "lm3d",
    m_shaderActivator,
    m_uniformsProvider,
    lmGroupRecord,
    m_meshGpuRecord,
    lmScalingProvider,
    lmToWorldTxProvider
  );

  // Save the drawables in the map
  m_lmDrawables.emplace(lmg->uid(), std::make_pair(lm2d, lm3d));

  // Add drawables to the assembly roots for 2D and 3D views
  m_rootFor2dViews->addChild(lm2d);
  m_rootFor3dViews->addChild(lm3d);

  updateRenderingProperties();
}

void LandmarkAssembly::removeLandmarkGroup(const uuids::uuid& lmGroupUid)
{
  detatchLandmarks(lmGroupUid);
  m_lmDrawables.erase(lmGroupUid);

  updateRenderingProperties();
}

void LandmarkAssembly::clearLandmarkGroups()
{
  for (auto& lmGroup : m_lmDrawables)
  {
    detatchLandmarks(lmGroup.first);
  }

  m_lmDrawables.clear();

  updateRenderingProperties();
}

void LandmarkAssembly::detatchLandmarks(const uuids::uuid& lmGroupUid)
{
  if (!m_rootFor2dViews || !m_rootFor3dViews)
  {
    throw_debug("Null root drawables in assembly");
  }

  // Remove 2D and 3D drawables from roots:
  const auto itr = m_lmDrawables.find(lmGroupUid);
  if (std::end(m_lmDrawables) != itr)
  {
    if (auto lmGroup2d = itr->second.first)
    {
      m_rootFor2dViews->removeChild(*lmGroup2d);
    }

    if (auto lmGroup3d = itr->second.second)
    {
      m_rootFor3dViews->removeChild(*lmGroup3d);
    }
  }
}

void LandmarkAssembly::setMasterOpacityMultiplier(float multiplier)
{
  m_lmProperties.m_masterOpacityMultiplier = multiplier;
  updateRenderingProperties();
}

void LandmarkAssembly::setLandmarksVisibleIn2dViews(bool visible)
{
  m_lmProperties.m_visibleIn2dViews = visible;
  updateRenderingProperties();
}

void LandmarkAssembly::setLandmarksVisibleIn3dViews(bool visible)
{
  m_lmProperties.m_visibleIn3dViews = visible;
  updateRenderingProperties();
}

void LandmarkAssembly::setLandmarksPickable(bool pickable)
{
  m_lmProperties.m_pickable = pickable;
  updateRenderingProperties();
}

const LandmarkAssemblyRenderingProperties& LandmarkAssembly::getRenderingProperties() const
{
  return m_lmProperties;
}

void LandmarkAssembly::updateRenderingProperties()
{
  const auto& P = m_lmProperties;

  for (auto& lmGroup : m_lmDrawables)
  {
    // 2D landmark
    if (auto& lm2d = lmGroup.second.first)
    {
      lm2d->setMasterOpacityMultiplier(P.m_masterOpacityMultiplier);
      lm2d->setPickable(P.m_pickable);
      lm2d->setVisible(P.m_visibleIn2dViews);
    }

    // 3D landmark
    if (auto& lm3d = lmGroup.second.second)
    {
      lm3d->setMasterOpacityMultiplier(P.m_masterOpacityMultiplier);
      lm3d->setPickable(P.m_pickable);
      lm3d->setVisible(P.m_visibleIn2dViews);
    }
  }
}
