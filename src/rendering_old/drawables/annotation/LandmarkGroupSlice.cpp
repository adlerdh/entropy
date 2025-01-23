#include "rendering/drawables/annotation/LandmarkGroupSlice.h"

#include "common/HZeeException.hpp"
#include "logic/annotation/PlanarPolygon.h"
#include "logic/camera/CameraHelpers.h"

#include "rendering/common/MeshPolygonOffset.h"
#include "rendering/drawables/BasicMesh.h"
#include "rendering/utility/CreateGLObjects.h"
#include "rendering/utility/UnderlyingEnumType.h"
#include "rendering/utility/math/MathUtility.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_inverse.hpp>
#include <glm/gtc/packing.hpp>

#include <algorithm> // sort
#include <array>
#include <numeric> // iota
#include <vector>

namespace
{

// Starting offset into OpenGL buffers
static constexpr GLintptr sk_bufferOffset = static_cast<GLintptr>(0);

// Z normal vector packed into uint32
static const uint32_t sk_zNormal = glm::packSnorm3x10_1x2(glm::vec4{0.0f, 0.0f, 1.0f, 0.0f});

glm::vec4 computeAnnotationPlane(
  const glm::mat4& annot_O_world, const glm::mat4& world_O_camera, const glm::mat4& world_O_frame
)
{
  const glm::mat4 annot_O_camera = annot_O_world * world_O_camera;
  const glm::mat4 annot_O_crosshairsFrame = annot_O_world * world_O_frame;

  const glm::vec4 p = annot_O_crosshairsFrame[3];
  const glm::vec3 planePosition = glm::vec3{p / p.w};
  const glm::vec3 planeNormal = glm::vec3{glm::inverseTranspose(annot_O_camera)[2]};

  return math::makePlane(glm::normalize(planeNormal), planePosition);
}

/**
 * @brief Convenience function to append an intersection point between and line segment and plane
 * to an existing vector of points.
 *
 * @param[in] start Start point of line segment
 * @param[in] end End point of line segment
 * @param[in] plane Plane equation in form (a, b, c, d), where ax + by + cz + d = 0
 * @param[in,out] intersections Vector of intersections to append to
 *
 * @return True iff an intersection exists and was appended to the vector
 */
bool addIntersection(
  const glm::vec3& start,
  const glm::vec3& end,
  const glm::vec4& plane,
  std::vector<glm::vec3>& intersections
)
{
  float t;
  if (math::lineSegmentPlaneIntersection(start, end, plane, t))
  {
    intersections.emplace_back(start + t * (end - start));
    return true;
  }

  return false;
}

std::vector<glm::vec3> computeAnnotationIntersections(
  const glm::vec4& annotPlane,
  const std::array<glm::vec3, 3>& bottomFace,
  const std::array<glm::vec3, 3>& topFace
)
{
  std::vector<glm::vec3> intersections;

  do
  {
    // Test intersections with sides edges:
    addIntersection(bottomFace[0], topFace[0], annotPlane, intersections);
    addIntersection(bottomFace[1], topFace[1], annotPlane, intersections);
    addIntersection(bottomFace[2], topFace[2], annotPlane, intersections);

    if (3 == intersections.size())
    {
      // If three points along the side faces are intersected,
      // then there will be no more intersections.
      break;
    }

    // Test intersections with bottom face edges:
    addIntersection(bottomFace[0], bottomFace[1], annotPlane, intersections);
    addIntersection(bottomFace[0], bottomFace[2], annotPlane, intersections);

    if (5 == intersections.size())
    {
      // If five points are intersected, then there will be no more:
      break;
    }

    addIntersection(bottomFace[1], bottomFace[2], annotPlane, intersections);
    if (5 == intersections.size())
    {
      break;
    }

    // Test intersections with top face edges:
    addIntersection(topFace[0], topFace[1], annotPlane, intersections);
    if (5 == intersections.size())
    {
      break;
    }

    addIntersection(topFace[0], topFace[2], annotPlane, intersections);
    if (5 == intersections.size())
    {
      break;
    }

    addIntersection(topFace[1], topFace[2], annotPlane, intersections);
  } while (false);

  return intersections;
}

std::unique_ptr<MeshGpuRecord> reallocateMeshGpuRecord(size_t triangleCount)
{
  // Every triangle in the annotation polygon forms a triangular prism in 3D that potentially
  // intersects the view plane at 5 points:
  const size_t vertexCount = 5 * triangleCount;
  const size_t normalBufferBytes = vertexCount * sizeof(uint32_t);

  // One (identical) normal vector per vertex:
  const std::vector<uint32_t> normalBuffer(vertexCount, sk_zNormal);

  // Create three triangles to represent the pentagon of intersection between each
  // triangle prism and the view plane. The triangles are indexed independently with 9 indices.
  // (Note: We could use a triangle fan or strip to reduce the index count.)
  const size_t indexCount = 9 * triangleCount;
  const size_t indexBufferBytes = indexCount * sizeof(uint32_t);

  std::vector<uint32_t> indexBuffer;

  uint32_t lastIndex = 0;

  for (uint32_t triangle = 0; triangle < triangleCount; ++triangle)
  {
    // First triangle
    indexBuffer.push_back(lastIndex + 0);
    indexBuffer.push_back(lastIndex + 1);
    indexBuffer.push_back(lastIndex + 2);

    // Second triangle
    indexBuffer.push_back(lastIndex + 0);
    indexBuffer.push_back(lastIndex + 2);
    indexBuffer.push_back(lastIndex + 3);

    // Third triangle
    indexBuffer.push_back(lastIndex + 0);
    indexBuffer.push_back(lastIndex + 3);
    indexBuffer.push_back(lastIndex + 4);

    lastIndex += 5;
  }

  auto record = gpuhelper::createMeshGpuRecord(
    vertexCount, indexCount, PrimitiveMode::Triangles, BufferUsagePattern::DynamicDraw
  );

  record->indicesObject().write(sk_bufferOffset, indexBufferBytes, indexBuffer.data());

  if (record->normalsObject())
  {
    record->normalsObject()->write(sk_bufferOffset, normalBufferBytes, normalBuffer.data());
  }
  else
  {
    return nullptr;
  }

  return record;
}

} // namespace

LandmarkSlice::LandmarkSlice(
  std::string name,
  ShaderProgramActivatorType shaderProgramActivator,
  UniformsProviderType uniformsProvider,
  ValueGetterType<boost::optional<glm::mat4> > annotToWorldTxProvider,
  std::weak_ptr<SlideAnnotationRecord> slideAnnotationRecord
)
  : DrawableBase(std::move(name), DrawableType::LandmarkSlice)
  ,

  m_shaderActivator(shaderProgramActivator)
  , m_uniformsProvider(uniformsProvider)
  ,

  m_annotToWorldTxProvider(annotToWorldTxProvider)
  , m_slideAnnotationRecord(slideAnnotationRecord)
  ,

  m_meshGpuRecord(nullptr)
  , m_mesh(nullptr)
  , m_currentAnnotationUid(boost::none)
{
  m_renderId = static_cast<uint32_t>(underlyingType(m_type) << 12) | (numCreated() % 0x1000);

  updateMeshGpuRecord();

  setupChildren();
}

bool LandmarkSlice::isMeshGpuRecordCurrent() const
{
  auto annot = m_slideAnnotationRecord.lock();
  if (!annot || !annot->cpuRecord() || !annot->cpuRecord()->polygon())
  {
    throw_debug("Null slide annotation record");
  }

  const PlanarPolygon* polygon = annot->cpuRecord()->polygon();
  return (m_currentAnnotationUid && polygon->equals(*m_currentAnnotationUid));
}

void LandmarkSlice::updateMeshGpuRecord()
{
  if (isMeshGpuRecordCurrent())
  {
    return;
  }

  // The current GPU record known by this object is not current for the annotation.

  auto annot = m_slideAnnotationRecord.lock();
  if (!annot || !annot->cpuRecord() || !annot->cpuRecord()->polygon())
  {
    throw_debug("Null slide annotation record");
  }

  PlanarPolygon* polygon = annot->cpuRecord()->polygon();

  // Save the new UID and reallocate a new GPU record:
  m_currentAnnotationUid = polygon->getCurrentUid();

  m_meshGpuRecord = reallocateMeshGpuRecord(polygon->numTriangles());
  if (!m_meshGpuRecord)
  {
    throw_debug("Null mesh GPU record");
  }

  if (m_mesh)
  {
    m_mesh->setMeshGpuRecord(m_meshGpuRecord);
  }
  else
  {
    m_mesh = std::make_shared<
      BasicMesh>("annotSlice", m_shaderActivator, m_uniformsProvider, m_meshGpuRecord);
  }
}

void LandmarkSlice::setupChildren()
{
  if (!m_mesh)
  {
    throw_debug("Null mesh");
  }

  // Only ambient lighting
  m_mesh->setAdsLightFactors(1.0f, 0.0f, 0.0f);
  m_mesh->setUseOctantClipPlanes(false);

  m_mesh->setEnablePolygonOffset(true);

  m_mesh->setPolygonOffsetValues(PolygonOffset::annotations.first, PolygonOffset::annotations.second);

  // No backface culling, so that we can see slices from front and back sides
  m_mesh->setBackfaceCull(false);

  // Annotations are only colored by material:
  m_mesh->enableLayer(BasicMeshColorLayer::Material);
  m_mesh->disableLayer(BasicMeshColorLayer::Vertex);

  m_mesh->setLayerOpacityMultiplier(BasicMeshColorLayer::Material, 1.0f);

  addChild(m_mesh);
}

bool LandmarkSlice::isOpaque() const
{
  if (m_mesh)
  {
    return m_mesh->isOpaque();
  }
  return DrawableBase::isOpaque();
}

void LandmarkSlice::doUpdate(
  double, const Viewport&, const camera::Camera& camera, const CoordinateFrame& crosshairs
)
{
  if (!m_mesh || !m_annotToWorldTxProvider)
  {
    setVisible(false);
    return;
  }

  auto annotRecord = m_slideAnnotationRecord.lock();
  if (!annotRecord || !annotRecord->cpuRecord() || !annotRecord->cpuRecord()->polygon())
  {
    setVisible(false);
    return;
  }

  auto world_O_annot = m_annotToWorldTxProvider();
  if (!world_O_annot)
  {
    setVisible(false);
    return;
  }

  updateMeshGpuRecord();

  setVisible(true);

  auto* annot = annotRecord->cpuRecord();

  // Set color and opacity of mesh
  m_mesh->setMaterialColor(annot->getColor());
  m_mesh->setMasterOpacityMultiplier(annot->getOpacity());

  // Compute the intersections between the view plane and the annotation.

  const glm::mat4 annot_O_world = glm::inverse(*world_O_annot);

  // View plane mapped to annotation (slide) space:
  const glm::vec4 annotPlane
    = computeAnnotationPlane(annot_O_world, camera.world_O_camera(), crosshairs.world_O_frame());

  std::vector<glm::vec3> positions;
  std::vector<uint32_t> indices;

  PlanarPolygon* polygon = annot->polygon();

  for (uint32_t i = 0; i < polygon->numTriangles(); ++i)
  {
    const auto triangle = polygon->getTriangle(i);

    // Vertices of bottom face (z = 0) and bottom face (z = 1) for the this triangular pyramid:
    const std::array<glm::vec3, 3> bottomFace{
      glm::vec3{polygon->getVertex(std::get<0>(triangle)), 0.0f},
      glm::vec3{polygon->getVertex(std::get<1>(triangle)), 0.0f},
      glm::vec3{polygon->getVertex(std::get<2>(triangle)), 0.0f}
    };

    const std::array<glm::vec3, 3> topFace{
      glm::vec3{bottomFace[0].x, bottomFace[0].y, 1.0f},
      glm::vec3{bottomFace[1].x, bottomFace[1].y, 1.0f},
      glm::vec3{bottomFace[2].x, bottomFace[2].y, 1.0f}
    };

    // Intersections of triangular pyramid and the view plane, in annotation space:
    std::vector<glm::vec3> annotIntersections
      = computeAnnotationIntersections(annotPlane, bottomFace, topFace);

    if (0 == annotIntersections.size())
    {
      // Plane did not intersect the prism.
      // Add 5 equal dummy vertices as intersections:
      positions.insert(std::begin(positions), 5, bottomFace[0]);
    }
    else if (1 == annotIntersections.size())
    {
      // Plane intersected the prism at one point: add it 5 times.
      positions.insert(std::begin(positions), 5, annotIntersections[0]);
    }
    else if (2 == annotIntersections.size())
    {
      // Plane intersected the prism at two points (an edge).
      positions.insert(std::begin(positions), 4, annotIntersections[0]);
      positions.push_back(annotIntersections[1]);
    }
    else if (3 == annotIntersections.size())
    {
      // Plane intersected the prism in a triangle.
      positions.push_back(annotIntersections[0]);
      positions.push_back(annotIntersections[0]);
      positions.push_back(annotIntersections[0]);
      positions.push_back(annotIntersections[1]);
      positions.push_back(annotIntersections[2]);
    }
    else if (4 == annotIntersections.size())
    {
      // Plane intersected the prism in a convex quadrilateral.

      // Project the intersection points to their plane:
      const auto projectedIntersections = math::project3dPointsToPlane(annotIntersections);

      // Reorder the points so that there is no crossing:
      const auto reordering = math::sortCounterclockwise(projectedIntersections);

      // Duplicate first point:
      positions.push_back(annotIntersections[reordering[0]]);

      for (uint32_t ii : reordering)
      {
        positions.push_back(annotIntersections[ii]);
      }
    }
    else if (5 == annotIntersections.size())
    {
      // Plane intersected the prism in a convex pentagon.

      // Project the intersection points to their plane:
      const auto projectedIntersections = math::project3dPointsToPlane(annotIntersections);

      for (uint32_t ii : math::sortCounterclockwise(projectedIntersections))
      {
        positions.push_back(annotIntersections[ii]);
      }
    }
  }

  // Offset annotation vertices towards viewer according to their layer depth.
  // Increase all layers by an additional 4 offset, to make sure that there is no
  // z-fighting with slides.
  math::applyLayeringOffsetsToModelPositions(camera, annot_O_world, annot->getLayer() + 4, positions);

  const size_t vertexCount = positions.size();
  const size_t positionBufferBytes = vertexCount * sizeof(glm::vec3);

  // Set new positions in mesh
  auto& positionsObject = m_meshGpuRecord->positionsObject();
  positionsObject.write(sk_bufferOffset, positionBufferBytes, positions.data());
}
