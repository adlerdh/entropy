#include "rendering_old/drawables/annotation/AnnotationExtrusion.h"

#include "common/Exception.hpp"

#include "logic/camera/CameraHelpers.h"
#include "logic/camera/MathUtility.h"
#include "logic_old/annotation/PlanarPolygon.h"

#include "rendering/utility/UnderlyingEnumType.h"
#include "rendering_old/common/MeshPolygonOffset.h"
#include "rendering_old/drawables/BasicMesh.h"
#include "rendering_old/drawables/Transformation.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_inverse.hpp>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/transform.hpp>

#include <algorithm> // used for std::transform
#include <sstream>

namespace
{

glm::vec3 applyMatrix(const glm::mat4& M, const glm::vec3& a)
{
  glm::vec4 b = M * glm::vec4{a, 1.0f};
  return glm::vec3{b / b.w};
}

} // namespace

AnnotationExtrusion::AnnotationExtrusion(
  std::string name,
  ShaderProgramActivatorType shaderActivator,
  UniformsProviderType uniformsProvider,
  GetterType<std::optional<glm::mat4> > annotToWorldTxProvider,
  GetterType<std::optional<float> > thicknessProvider,
  std::weak_ptr<SlideAnnotationRecord> slideAnnotationRecord
)
  : DrawableBase(std::move(name), DrawableType::AnnotationSlice)
  ,

  m_shaderActivator(shaderActivator)
  , m_uniformsProvider(uniformsProvider)
  ,

  m_annotToWorldTxProvider(annotToWorldTxProvider)
  , m_thicknessProvider(thicknessProvider)
  , m_slideAnnotationRecord(slideAnnotationRecord)
  ,

  m_meshGpuRecord(nullptr)
  , m_mesh(nullptr)
  , m_scaleTx(nullptr)
{
  m_renderId = static_cast<uint32_t>(underlyingType(m_type) << 12) | (numCreated() % 0x1000);

  setupChildren();
}

void AnnotationExtrusion::setupChildren()
{
  auto annotRecord = m_slideAnnotationRecord.lock();
  if (!annotRecord || !annotRecord->gpuData())
  {
    throw_debug("Null slide annotation or GPU record");
  }

  std::ostringstream ss;
  ss << m_name << "_AnnotationExtrusionMesh" << std::ends;

  m_mesh = std::make_shared<BasicMesh>(
    ss.str(), m_shaderActivator, m_uniformsProvider, annotRecord->gpuData()->getMeshGpuRecord()
  );

  m_mesh->setAdsLightFactors(0.5f, 0.5f, 0.05f);
  m_mesh->setUseOctantClipPlanes(false);

  m_mesh->setEnablePolygonOffset(true);

  m_mesh->setPolygonOffsetValues(PolygonOffset::annotations.first, PolygonOffset::annotations.second);

  // Enable culling so that we can't see inside the annotations
  /// @todo Don't do this if opacity < 1.0
  m_mesh->setBackfaceCull(true);

  // Annotations are only colored by material:
  m_mesh->enableLayer(BasicMeshColorLayer::Material);
  m_mesh->disableLayer(BasicMeshColorLayer::Vertex);

  m_mesh->setLayerOpacityMultiplier(BasicMeshColorLayer::Material, 1.0f);

  m_scaleTx = std::make_shared<Transformation>("annotScaleTx");
  m_scaleTx->addChild(m_mesh);

  addChild(m_scaleTx);
}

bool AnnotationExtrusion::isOpaque() const
{
  if (m_mesh)
  {
    return m_mesh->isOpaque();
  }
  return false;
}

void AnnotationExtrusion::
  doUpdate(double, const Viewport&, const camera::Camera& camera, const CoordinateFrame&)
{
  if (!m_mesh || !m_thicknessProvider || !m_annotToWorldTxProvider)
  {
    setVisible(false);
    return;
  }

  auto worldThickness = m_thicknessProvider();
  auto world_O_annot = m_annotToWorldTxProvider();

  if (!worldThickness || !world_O_annot)
  {
    setVisible(false);
    return;
  }

  auto annotRecord = m_slideAnnotationRecord.lock();
  if (!annotRecord || !annotRecord->cpuData() || !annotRecord->cpuData()->polygon())
  {
    setVisible(false);
    return;
  }

  auto* annot = annotRecord->cpuData();

  // Axis-aligned bounding square of the polygon (2D coordinates)
  const auto aabSquare = annot->polygon()->getAABBox();

  if (!aabSquare)
  {
    setVisible(false);
    return;
  }

  setVisible(true);

  // Set color and opacity of mesh
  m_mesh->setMaterialColor(annot->getColor());
  m_mesh->setMasterOpacityMultiplier(annot->getOpacity());

  // AABB of the annotation uses z = 0 for the bottom face and z = 1 for the top face,
  // since the annotation is defined in normalied Slide-space coordinates.
  const AABB<float> annotAABBox{glm::vec3{aabSquare->first, 0}, glm::vec3{aabSquare->second, 1}};

  // Corners of the AABB in annotation space:
  const std::array<glm::vec3, 8> annotAABBoxCorners = math::makeAABBoxCorners(annotAABBox);

  // Vector of World-space offets:
  std::vector<float> worldOffsets;

  // Compute depth offset for each AABB corner in World units:
  std::transform(
    std::begin(annotAABBoxCorners),
    std::end(annotAABBoxCorners),
    std::back_inserter(worldOffsets),
    [&camera, &world_O_annot](const glm::vec3& annotCorner)
    {
      return camera::computeSmallestWorldDepthOffset(
        camera, applyMatrix(*world_O_annot, glm::vec4{annotCorner, 1.0f})
      );
    }
  );

  // Use the maximum offset for layering:
  const float maxWorldOffset = *std::max_element(std::begin(worldOffsets), std::end(worldOffsets));

  // Divide by slide thickness to get offset in Annotation mesh coordinates:
  const float annotOffset = maxWorldOffset / *worldThickness;

  // Desired displacement of bottom and top face is proprotional to the annotation layer
  // and the offset. Increase all layers by an additional 2 offset, to make sure that there is no
  // z-fighting with slides.
  const float displacement = static_cast<float>(annot->getLayer() + 2) * annotOffset;

  // Scale the mesh along its z axis by an additional factor of 2x, because the scaling is
  // applied about the center of the mesh. So, the top and bottom faces only move by half this amount.
  const float zScale = 2.0f * displacement;
  const glm::vec3 scale{1.0f, 1.0f, 1.0f + zScale};

  // Center point of annotation, about which to apply the scale:
  const glm::vec3 center{0.0f, 0.0f, 0.5f};

  m_scaleTx->setMatrix(glm::translate(center) * glm::scale(scale) * glm::translate(-center));
}
