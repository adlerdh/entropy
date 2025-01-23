#include "rendering_old/drawables/Crosshairs.h"
#include "rendering_old/drawables/BasicMesh.h"
#include "rendering_old/drawables/Transformation.h"

#include "rendering/utility/UnderlyingEnumType.h"
#include "rendering/utility/containers/VertexAttributeInfo.h"
#include "rendering/utility/containers/VertexIndicesInfo.h"
#include "rendering/utility/gl/GLBufferObject.h"
#include "rendering_old/common/MeshPolygonOffset.h"
#include "rendering_old/records/MeshGpuRecord.h"

#include "logic/camera/CameraHelpers.h"
#include "logic/camera/MathUtility.h"

#include "common/Exception.hpp"

#include <glm/gtc/matrix_inverse.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/component_wise.hpp>
#include <glm/gtx/intersect.hpp> // PUT INTO HELPER
#include <glm/gtx/transform.hpp>

#include <algorithm>
#include <iostream>
#include <sstream>

namespace
{

static const glm::mat4 IDENTITY{1.0f};
static const float sk_defaultLength = 100.0f;

bool intersectDoubleSidedRayWithPlane(
  const glm::vec3& orig,
  const glm::vec3& dir,
  const glm::vec3& planeOrig,
  const glm::vec3& planeNormal,
  float& intersectionDistance
)
{
  static constexpr float EPS = std::numeric_limits<float>::epsilon();

  const float d = std::abs(glm::dot(dir, planeNormal));

  if (d > EPS)
  {
    intersectionDistance = std::abs(glm::dot(planeOrig - orig, planeNormal)) / d;
    return true;
  }

  return false;
}

std::array<float, 3> distancesFromCrosshairToFrustumPlanes(
  const camera::Camera& camera, const CoordinateFrame& crosshairs
)
{
  static const glm::vec3 sk_ndcLeftPlanePos{-1.0f, 0.0f, -1.0f};
  static const glm::vec3 sk_ndcRightPlanePos{1.0f, 0.0f, -1.0f};
  static const glm::vec3 sk_ndcBottomPlanePos{0.0f, -1.0f, -1.0f};
  static const glm::vec3 sk_ndcTopPlanePos{0.0f, 1.0f, -1.0f};

  const glm::vec3 leftPlanePos = camera::world_T_ndc(camera, sk_ndcLeftPlanePos);
  const glm::vec3 rightPlanePos = camera::world_T_ndc(camera, sk_ndcRightPlanePos);
  const glm::vec3 bottomPlanePos = camera::world_T_ndc(camera, sk_ndcBottomPlanePos);
  const glm::vec3 topPlanePos = camera::world_T_ndc(camera, sk_ndcTopPlanePos);

  const glm::vec3 lrPlaneNormal = camera::worldDirection(camera, Directions::View::Right);
  const glm::vec3 btPlaneNormal = camera::worldDirection(camera, Directions::View::Up);

  const glm::vec3 rayPos = crosshairs.worldOrigin();

  const glm::mat3 world_T_frame_invT = glm::inverseTranspose(glm::mat3{crosshairs.world_T_frame()});

  // Largest distances from the crosshair's x, y, and z axes
  std::array<float, 3> distances;

  for (uint32_t i = 0; i < 3; ++i)
  {
    const glm::vec3 rayDir{world_T_frame_invT[static_cast<int>(i)]};

    float d;

    float horizDist = 0.0f;
    float vertDist = 0.0f;

    bool hitXPlane = false;
    bool hitYPlane = false;

    if (intersectDoubleSidedRayWithPlane(rayPos, rayDir, leftPlanePos, lrPlaneNormal, d))
    {
      hitXPlane = true;
      horizDist = std::max(horizDist, d);
    }

    if (intersectDoubleSidedRayWithPlane(rayPos, rayDir, rightPlanePos, -lrPlaneNormal, d))
    {
      hitXPlane = true;
      horizDist = std::max(horizDist, d);
    }

    if (intersectDoubleSidedRayWithPlane(rayPos, rayDir, bottomPlanePos, btPlaneNormal, d))
    {
      hitYPlane = true;
      vertDist = std::max(vertDist, d);
    }

    if (intersectDoubleSidedRayWithPlane(rayPos, rayDir, topPlanePos, -btPlaneNormal, d))
    {
      hitYPlane = true;
      vertDist = std::max(vertDist, d);
    }

    if (hitXPlane && hitYPlane)
    {
      distances[i] = std::min(horizDist, vertDist);
    }
    else if (hitXPlane)
    {
      distances[i] = horizDist;
    }
    else if (hitYPlane)
    {
      distances[i] = vertDist;
    }
  }

  return distances;
}

} // anonymous namespace

Crosshairs::Crosshairs(
  std::string name,
  ShaderProgramActivatorType shaderProgramActivator,
  UniformsProviderType uniformsProvider,
  std::weak_ptr<MeshGpuRecord> meshGpuRecord,
  bool isFixedDiameter
)
  : DrawableBase(std::move(name), DrawableType::Crosshairs)
  ,

  m_crosshairLength(sk_defaultLength)
  , m_isFixedDiameter(isFixedDiameter)
  ,

  m_txs{
    std::make_shared<Transformation>("crosshairTxX", IDENTITY),
    std::make_shared<Transformation>("crosshairTxY", IDENTITY),
    std::make_shared<Transformation>("crosshairTxZ", IDENTITY)
  }
  ,

  m_crosshairs{
    std::make_shared<
      BasicMesh>("crosshairMeshX", shaderProgramActivator, uniformsProvider, meshGpuRecord),
    std::make_shared<
      BasicMesh>("crosshairMeshY", shaderProgramActivator, uniformsProvider, meshGpuRecord),
    std::make_shared<
      BasicMesh>("crosshairMeshZ", shaderProgramActivator, uniformsProvider, meshGpuRecord)
  }
{
  m_renderId = static_cast<uint32_t>(underlyingType(m_type) << 12) | (numCreated() % 0x1000);

  setupChildren();
}

void Crosshairs::setupChildren()
{
  const auto offset = PolygonOffset::crosshairs;

  for (uint32_t i = 0; i < 3; ++i)
  {
    addChild(m_txs[i]);

    m_txs[i]->addChild(m_crosshairs[i]);

    m_crosshairs[i]->setAdsLightFactors(0.5f, 0.5f, 0.1f);
    m_crosshairs[i]->setUseOctantClipPlanes(false);

    // Polygon offset enabled so that crosshairs are nearer the viewer than
    // other objects without polygon offset defined.
    m_crosshairs[i]->setEnablePolygonOffset(true);

    m_crosshairs[i]->setPolygonOffsetValues(offset.first, offset.second);

    // No need to see inside of crosshairs:
    m_crosshairs[i]->setBackfaceCull(true);

    // Crosshairs are only colored by material:
    m_crosshairs[i]->enableLayer(BasicMeshColorLayer::Material);
    m_crosshairs[i]->disableLayer(BasicMeshColorLayer::Vertex);

    m_crosshairs[i]->setLayerOpacityMultiplier(BasicMeshColorLayer::Material, 1.0f);
  }

  setPickable(false);
}

void Crosshairs::setLength(float length)
{
  if (0.0f < length)
  {
    m_crosshairLength = length;
  }
}

DrawableOpacity Crosshairs::opacityFlag() const
{
  return DrawableOpacity{OpacityFlag::Unknown, OpacityFlag::Unknown};
}

void Crosshairs::doUpdate(
  double /*time*/,
  const Viewport& viewport,
  const camera::Camera& camera,
  const CoordinateFrame& crosshairs
)
{
  static const glm::mat4 sk_xRotTx = glm::rotate(-glm::half_pi<float>(), glm::vec3{1.0f, 0.0f, 0.0f});

  static const glm::mat4 sk_yRotTx = glm::rotate(glm::half_pi<float>(), glm::vec3{0.0f, 1.0f, 0.0f});

  const float xyFactor = (m_isFixedDiameter)
                           ? m_crosshairLength / 5.0f
                           : 2.0f * glm::compMax(worldPixelSize(viewport, camera));

  std::array<float, 3> lengths;

  if (camera.isOrthographic())
  {
    lengths = distancesFromCrosshairToFrustumPlanes(camera, crosshairs);
  }
  else
  {
    lengths = {m_crosshairLength, m_crosshairLength, m_crosshairLength};
  }

  const glm::mat4 xScaleTx = glm::scale(glm::vec3{xyFactor, xyFactor, lengths[0]});
  const glm::mat4 yScaleTx = glm::scale(glm::vec3{xyFactor, xyFactor, lengths[1]});
  const glm::mat4 zScaleTx = glm::scale(glm::vec3{xyFactor, xyFactor, lengths[2]});

  const glm::mat4& world_O_frame = crosshairs.world_T_frame();

  m_txs[0]->setMatrix(world_O_frame * sk_yRotTx * xScaleTx);
  m_txs[1]->setMatrix(world_O_frame * sk_xRotTx * yScaleTx);
  m_txs[2]->setMatrix(world_O_frame * zScaleTx);

  const glm::mat3 world_O_frame_invT = glm::inverseTranspose(glm::mat3{world_O_frame});

  m_crosshairs[0]->setMaterialColor(math::convertVecToRGB(world_O_frame_invT[0]));
  m_crosshairs[1]->setMaterialColor(math::convertVecToRGB(world_O_frame_invT[1]));
  m_crosshairs[2]->setMaterialColor(math::convertVecToRGB(world_O_frame_invT[2]));
}
