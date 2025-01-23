#include "rendering_old/drawables/slides/SlideStackArrow.h"
#include "rendering_old/drawables/BasicMesh.h"
#include "rendering_old/drawables/Transformation.h"

#include "rendering/utility/containers/VertexAttributeInfo.h"
#include "rendering/utility/containers/VertexIndicesInfo.h"
#include "rendering/utility/gl/GLBufferObject.h"
#include "rendering_old/records/MeshGpuRecord.h"

#include "logic/camera/CameraHelpers.h"
#include "logic/camera/MathUtility.h"

#include "common/Exception.hpp"

#include <glm/gtc/matrix_inverse.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/component_wise.hpp>
#include <glm/gtx/transform.hpp>

#include <algorithm>
#include <sstream>

SlideStackArrow::SlideStackArrow(
  std::string name,
  ShaderProgramActivatorType shaderProgramActivator,
  UniformsProviderType uniformsProvider,
  GetterType<float> slideStackHeightProvider,
  std::weak_ptr<MeshGpuRecord> coneMeshGpuRecord,
  std::weak_ptr<MeshGpuRecord> cylinderMeshGpuRecord,
  std::weak_ptr<MeshGpuRecord> sphereMeshGpuRecord,
  bool isFixedRadius
)
  : DrawableBase(std::move(name), DrawableType::SlideStackArrow)
  ,

  m_slideStackHeightProvider(slideStackHeightProvider)
  , m_cylinderRadius(2.0f)
  , m_isFixedRadius(isFixedRadius)
  ,

  m_coneTx(std::make_shared<Transformation>("stackArrowConeTx", glm::mat4{1.0f}))
  , m_cylinderTx(std::make_shared<Transformation>("stackArrowCylinderTx", glm::mat4{1.0f}))
  , m_sphereTx(std::make_shared<Transformation>("stackArrowSphereTx", glm::mat4{1.0f}))
  ,

  m_cone(std::make_shared<BasicMesh>(
    "stackArrowConeMesh", shaderProgramActivator, uniformsProvider, coneMeshGpuRecord
  ))
  ,

  m_cylinder(std::make_shared<BasicMesh>(
    "stackArrowCylinderMesh", shaderProgramActivator, uniformsProvider, cylinderMeshGpuRecord
  ))
  ,

  m_sphere(std::make_shared<BasicMesh>(
    "stackArrowSphereMesh", shaderProgramActivator, uniformsProvider, sphereMeshGpuRecord
  ))
{
  setupChildren();
}

void SlideStackArrow::setSlideStackHeightProvider(GetterType<float> provider)
{
  m_slideStackHeightProvider = provider;
}

void SlideStackArrow::setRadius(float radius)
{
  if (0.0f < radius)
  {
    m_cylinderRadius = radius;
  }
}

void SlideStackArrow::setupChildren()
{
  static const glm::vec3 sk_white{1.0f, 1.0f, 1.0f};

  addChild(m_coneTx);
  addChild(m_cylinderTx);
  addChild(m_sphereTx);

  m_coneTx->addChild(m_cone);
  m_cylinderTx->addChild(m_cylinder);
  m_sphereTx->addChild(m_sphere);

  // Disable backface culling, there is no need to see inside of the arrow

  m_cone->setAdsLightFactors(0.5f, 0.5f, 0.1f);
  m_cone->setUseOctantClipPlanes(false);
  m_cone->setMaterialColor(sk_white);
  m_cone->setBackfaceCull(true);

  m_cone->enableLayer(BasicMeshColorLayer::Material);
  m_cone->disableLayer(BasicMeshColorLayer::Vertex);

  m_cylinder->setAdsLightFactors(0.5f, 0.5f, 0.1f);
  m_cylinder->setUseOctantClipPlanes(false);
  m_cylinder->setMaterialColor(sk_white);
  m_cylinder->setBackfaceCull(true);

  m_cylinder->enableLayer(BasicMeshColorLayer::Material);
  m_cylinder->disableLayer(BasicMeshColorLayer::Vertex);

  m_sphere->setAdsLightFactors(0.5f, 0.5f, 0.1f);
  m_sphere->setUseOctantClipPlanes(false);
  m_sphere->setMaterialColor(sk_white);
  m_sphere->setBackfaceCull(true);

  m_sphere->enableLayer(BasicMeshColorLayer::Material);
  m_sphere->disableLayer(BasicMeshColorLayer::Vertex);

  setPickable(false);
}

void SlideStackArrow::
  doUpdate(double /*time*/, const Viewport& viewport, const camera::Camera& camera, const CoordinateFrame&)
{
  static constexpr float sk_defaultCylinderLength(50.0f);
  static constexpr float sk_defaultScaleFactorInPixels(2.0f);

  const float cylinderLength = (m_slideStackHeightProvider) ? (m_slideStackHeightProvider() + 10.0f)
                                                            : sk_defaultCylinderLength;

  const float xyFactor = (m_isFixedRadius) ? m_cylinderRadius
                                           : sk_defaultScaleFactorInPixels
                                               * glm::compMax(worldPixelSize(viewport, camera));

  glm::mat4 coneTx = glm::translate(glm::vec3{0.0f, 0.0f, cylinderLength})
                     * glm::scale(2.0f * glm::vec3{xyFactor, xyFactor, 2.0f * xyFactor});

  glm::mat4 cylinderTx = glm::scale(glm::vec3{xyFactor, xyFactor, cylinderLength});

  glm::mat4 sphereTx = glm::scale(glm::vec3{2.0f * xyFactor});

  m_coneTx->setMatrix(std::move(coneTx));
  m_cylinderTx->setMatrix(std::move(cylinderTx));
  m_sphereTx->setMatrix(std::move(sphereTx));
}
