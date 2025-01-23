#include "rendering_old/drawables/annotation/LandmarkGroup3d.h"

#include "common/Exception.hpp"
#include "logic/camera/CameraHelpers.h"

#include "rendering/utility/UnderlyingEnumType.h"
#include "rendering_old/common/MeshPolygonOffset.h"
#include "rendering_old/drawables/BasicMesh.h"
#include "rendering_old/drawables/Transformation.h"
#include "rendering_old/records/MeshGpuRecord.h"

#include <glm/glm.hpp>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/component_wise.hpp>
#include <glm/gtx/string_cast.hpp>
#include <glm/gtx/transform.hpp>

#include <sstream>
#include <unordered_set>

namespace
{

static const glm::vec3 sk_white{1.0f, 1.0f, 1.0f};
static const glm::mat4 sk_ident{1.0f};

glm::vec3 applyMatrix(const glm::mat4& M, const glm::vec3& a)
{
  glm::vec4 b = M * glm::vec4{a, 1.0f};
  return glm::vec3{b / b.w};
}

} // namespace

LandmarkGroup3d::Landmark::Landmark(LandmarkGroup3d& lmGroup)
  : m_tx(std::make_shared<Transformation>("lmTx", sk_ident))
  ,

  m_mesh(std::make_shared<BasicMesh>(
    "landmarkMesh", lmGroup.m_shaderActivator, lmGroup.m_uniformsProvider, lmGroup.m_meshGpuRecord
  ))
{
  m_tx->addChild(m_mesh);

  m_mesh->setAdsLightFactors(0.5f, 0.5f, 0.1f);
  m_mesh->setUseOctantClipPlanes(false);
  m_mesh->setMaterialColor(sk_white);

  m_mesh->enableLayer(BasicMeshColorLayer::Material);
  m_mesh->disableLayer(BasicMeshColorLayer::Vertex);

  m_mesh->setLayerOpacityMultiplier(BasicMeshColorLayer::Material, 1.0f);
  //    m_mesh->setMasterOpacityMultiplier( 1.0f );

  // Allow going inside of mesh
  m_mesh->setBackfaceCull(false);

  m_mesh->setPickable(true);

  // Polygon offset used so that the landmarks are always in front of image slices and slides.
  m_mesh->setEnablePolygonOffset(true);
  m_mesh->setPolygonOffsetValues(PolygonOffset::landmarks.first, PolygonOffset::landmarks.second);
}

LandmarkGroup3d::LandmarkGroup3d(
  std::string name,
  ShaderProgramActivatorType shaderActivator,
  UniformsProviderType uniformsProvider,
  std::weak_ptr<LandmarkGroupRecord> landmarkGroupRecord,
  std::weak_ptr<MeshGpuRecord> meshGpuRecord,
  GetterType<std::optional<DrawableScaling> > scalingProvider,
  GetterType<std::optional<std::pair<glm::mat4, glm::mat4> > > landmarkToWorldTxProvider
)
  : DrawableBase(std::move(name), DrawableType::Landmark)
  ,

  m_shaderActivator(shaderActivator)
  , m_uniformsProvider(uniformsProvider)
  ,

  m_landmarkGroupRecord(landmarkGroupRecord)
  , m_meshGpuRecord(meshGpuRecord)
  ,

  m_scalingProvider(scalingProvider)
  , m_landmarkToWorldTxProvider(landmarkToWorldTxProvider)
  ,

  m_landmarks()
{
  setRenderId((static_cast<uint32_t>(underlyingType(m_type)) << 12) | (numCreated() % 4096));
}

void LandmarkGroup3d::setScalingInfoProvider(GetterType<std::optional<DrawableScaling> > provider)
{
  m_scalingProvider = provider;
}

void LandmarkGroup3d::setLandmarkToWorldTxProvider(
  GetterType<std::optional<std::pair<glm::mat4, glm::mat4> > > provider
)
{
  m_landmarkToWorldTxProvider = provider;
}

void LandmarkGroup3d::
  doUpdate(double, const Viewport& viewport, const camera::Camera& camera, const CoordinateFrame&)
{
  if (!m_scalingProvider || !m_landmarkToWorldTxProvider)
  {
    setVisible(false);
    return;
  }

  const auto world_O_landmark = m_landmarkToWorldTxProvider();
  const auto scaling = m_scalingProvider();

  if (!world_O_landmark || !scaling)
  {
    setVisible(false);
    return;
  }

  auto lmGroup = m_landmarkGroupRecord.lock();

  if (!lmGroup || !lmGroup->cpuData())
  {
    setVisible(false);
    return;
  }

  setVisible(true);

  // Get the rotation component of the world_O_landmark transformation:
  const glm::mat4 rotation{glm::mat3{world_O_landmark->second}};

  auto* cpuRecord = lmGroup->cpuData();

  // Keep track of landmarks that are in the group
  std::unordered_set<uuids::uuid> landmarksInLandmarkGroup;

  // Loop over all points in the landmark group
  for (const auto& point : cpuRecord->getPoints().getPoints())
  {
    landmarksInLandmarkGroup.insert(point.uid());

    // If the point is not yet a child of this group, then emplace it
    auto ret = m_landmarks.emplace(std::make_pair(point.uid(), Landmark(*this)));

    Landmark& landmark = ret.first->second;

    if (!landmark.m_mesh || !landmark.m_tx)
    {
      // Error!!!
      continue;
    }

    if (ret.second)
    {
      addChild(landmark.m_tx);
    }

    // Disable backface culling if the mesh is not opaque
    //        if ( masterOpacityMultiplier() < 1.0f )
    //        {
    //            landmark.m_mesh->setBackfaceCull( false );
    //        }
    //        else
    //        {
    //            landmark.m_mesh->setBackfaceCull( true );
    //        }

    landmark.m_mesh->setMaterialColor(cpuRecord->getColor());

    landmark.m_mesh->setMasterOpacityMultiplier(cpuRecord->getOpacity());

    /// @todo Deal with layering?
    //        cpuRecord->getLayer();

    // Transform landmark to World space:
    const glm::vec3 worldPos = applyMatrix(world_O_landmark->first, point.getPosition());

    // Size of one pixel in World space
    const float worldPixelSize = glm::compMax(
      worldPixelSizeAtWorldPosition(viewport, camera, worldPos)
    );

    glm::vec3 scaleFactors{1.0f};

    for (uint32_t i = 0; i < 3; ++i)
    {
      switch ((*scaling)[i].m_scalingMode)
      {
      case ScalingMode::FixedInPhysicalWorld:
      {
        scaleFactors[int(i)] = (*scaling)[i].m_scale;
        break;
      }
      case ScalingMode::FixedInViewPixels:
      {
        scaleFactors[int(i)] = (*scaling)[i].m_scale * worldPixelSize;
        break;
      }
      }
    }

    landmark.m_tx->setMatrix(glm::translate(worldPos) * rotation * glm::scale(scaleFactors));
  }

  // Delete child landmarks that are not in the group any more:
  for (const auto& landmark : m_landmarks)
  {
    if (!landmarksInLandmarkGroup.count(landmark.first))
    {
      // Note: this is safe because rehashing is forbidden
      removeChild(*landmark.second.m_tx);
      m_landmarks.erase(landmark.first);
    }
  }
}
