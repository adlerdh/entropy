#include "rendering_old/drawables/PointLandmarkGroup.h"
#include "rendering_old/drawables/BasicMesh.h"
#include "rendering_old/drawables/Transformation.h"

#include "rendering/utility/UnderlyingEnumType.h"
#include "rendering/utility/containers/VertexAttributeInfo.h"
#include "rendering/utility/containers/VertexIndicesInfo.h"
#include "rendering/utility/gl/GLBufferObject.h"

#include "logic/camera/CameraHelpers.h"

#include "common/Exception.hpp"

#include <glm/gtc/matrix_inverse.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/component_wise.hpp>
#include <glm/gtx/transform.hpp>

#include <algorithm>
#include <sstream>

PointLandmarkGroup::PointLandmarkGroup(
  std::string name,
  ShaderProgramActivatorType shaderProgramActivator,
  UniformsProviderType uniformsProvider,
  std::weak_ptr<MeshRecord> sphereMeshRecord,
  std::weak_ptr<LandmarkGroupRecord> landmarkGroupRecord,
  bool isFixedRadius
)
  : DrawableBase(std::move(name), DrawableType::PointLandmarkGroup)
  ,

  m_radius(0.5f)
  , m_isFixedRadius(isFixedRadius)
  , m_scaleTx(1.0f)
  ,

  m_sphere(std::make_unique<BasicMesh>(
    "landmarkSphereMesh", shaderProgramActivator, uniformsProvider, sphereMeshRecord
  ))
  ,

  m_landmarkGroupRecord(landmarkGroupRecord)
{
  setRenderId((static_cast<uint32_t>(underlyingType(m_type)) << 12) | (numCreated() % 4096));

  setupSphere();
}

PointLandmarkGroup::~PointLandmarkGroup() = default;

void PointLandmarkGroup::setRadius(float radius)
{
  if (0.0f < radius)
  {
    m_radius = radius;
  }
}

void PointLandmarkGroup::setMaterialColor(const glm::vec3& color)
{
  if (m_sphere)
  {
    m_sphere->setMaterialColor(color);
  }
}

void PointLandmarkGroup::setupSphere()
{
  static const glm::vec3 sk_white{1.0f, 1.0f, 1.0f};

  if (!m_sphere)
  {
    return;
  }

  m_sphere->setAdsLightFactors(0.5f, 0.5f, 0.1f);
  m_sphere->setUseOctantClipPlanes(false);
  m_sphere->setMaterialColor(sk_white);

  // Disable backface culling, in case the spheres are not opaque
  m_sphere->setBackfaceCull(false);

  m_sphere->enableLayer(BasicMeshColorLayer::Material);
  m_sphere->disableLayer(BasicMeshColorLayer::Vertex);

  m_sphere->setPickable(true);
}

void PointLandmarkGroup::doRender(const RenderStage& stage)
{
  if (!m_sphere)
  {
    return;
  }

  auto landmarks = m_landmarkGroupRecord.lock();
  if (!landmarks || !landmarks->cpuRecord())
  {
    return;
  }

  uint32_t i = 0;

  for (const auto& point : landmarks->cpuRecord()->getPoints())
  {
    m_sphere->setRenderId(m_sphere->getRenderId() + i);
    ++i;

    // This does nothing withou update()!!
    m_sphere->set_parent_O_this(glm::translate(point.getPosition()) * m_scaleTx);

    // Can call private member of TextueredMesh, since this is a friend
    m_sphere->doRender(stage);
  }
}

void PointLandmarkGroup::doUpdate(
  double time,
  const Viewport& viewport,
  const camera::Camera& camera,
  const CoordinateFrame& crosshairs
)
{
  const float xyFactor = (m_isFixedRadius) ? m_radius
                                           : glm::compMax(worldPixelSize(viewport, camera));

  m_scaleTx = glm::scale(glm::vec3{2.0f * xyFactor});

  m_sphere->doUpdate(time, viewport, camera, crosshairs);
}
