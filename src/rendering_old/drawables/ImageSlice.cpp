#include "rendering_old/drawables/ImageSlice.h"
#include "rendering_old/drawables/Line.h"
#include "rendering_old/drawables/TexturedMesh.h"

#include "rendering/utility/UnderlyingEnumType.h"
#include "rendering/utility/gl/GLDrawTypes.h"
#include "rendering_old/ShaderNames.h"
#include "rendering_old/common/MeshPolygonOffset.h"
#include "rendering_old/records/MeshGpuRecord.h"

#include "logic/camera/CameraHelpers.h"
#include "logic/camera/MathUtility.h"

#include "common/Exception.hpp"

#include <glm/gtc/matrix_inverse.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/packing.hpp>
#include <glm/gtc/type_ptr.hpp>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/component_wise.hpp>
#include <glm/gtx/norm.hpp>
#include <glm/gtx/string_cast.hpp>
#include <glm/gtx/transform.hpp>

#include <array>
#include <cmath>
#include <sstream>
#include <unordered_map>
#include <utility>

namespace
{

static const glm::vec3 sk_black(0.0f, 0.0f, 0.0f);

// Default light factors for 2D image slices:
// Pure ambient contribution, so that lighting does not depend on view direction.
static constexpr float sk_ambientFactor2D = 1.0f;
static constexpr float sk_diffuseFactor2D = 0.0f;
static constexpr float sk_specularFactor2D = 0.0f;

// Default light factors for 3D image slices:
static constexpr float sk_ambientFactor3D = 0.20f;
static constexpr float sk_diffuseFactor3D = 0.55f;
static constexpr float sk_specularFactor3D = 0.25f;

// Defaut specular shininss exponent
static constexpr float sk_shininess = 15.0f;

static constexpr int sk_numVerts = 7;

} // namespace

ImageSlice::ImageSlice(
  std::string name,
  ShaderProgramActivatorType shaderProgramActivator,
  UniformsProviderType uniformsProvider,
  std::weak_ptr<BlankTextures> blankTextures,
  std::weak_ptr<MeshGpuRecord> sliceMeshGpuRecord
)
  : DrawableBase(std::move(name), DrawableType::ImageSlice)
  ,

  m_image3dRecord()
  , m_parcelRecord()
  ,

  m_sliceMeshGpuRecord(sliceMeshGpuRecord)
  ,

  m_sliceMesh(std::make_shared<TexturedMesh>(
    m_name + "_sliceMesh",
    shaderProgramActivator,
    uniformsProvider,
    blankTextures,
    [this]() -> MeshGpuRecord*
    {
      if (auto gpuRecord = m_sliceMeshGpuRecord.lock())
      {
        return gpuRecord.get();
      }
      return nullptr;
    }
  ))
  ,

  m_sliceOutline(std::make_shared<Line>(
    m_name + "_sliceOutline", shaderProgramActivator, uniformsProvider, PrimitiveMode::LineLoop
  ))
  ,

  m_sliceIntersector()
  ,

  m_modelPlaneNormal(1.0f, 0.0f, 0.0f)
  , m_clip_O_camera(1.0f)
  , m_camera_O_world(1.0f)
  , m_cameraIsOrthographic(true)
  ,

  m_worldCameraPos(0.0f, 0.0f, 0.0f)
  , m_worldCameraDir(0.0f, 0.0f, 1.0f)
  ,

  m_showOutline(true)
{
  // could move << 13 then have 2^13 for numcreated
  m_renderId = static_cast<uint32_t>(underlyingType(m_type) << 12) | (numCreated() % 0x1000);

  setAlignmentMethod(intersection::AlignmentMethod::CameraZ);
  setPositioningMethod(intersection::PositioningMethod::FrameOrigin);

  setupChildren();
}

void ImageSlice::setupChildren()
{
  if (!m_sliceMesh || !m_sliceOutline)
  {
    throw_debug("Null child drawable");
  }

  addChild(m_sliceMesh);
  addChild(m_sliceOutline);

  m_sliceMesh->setAdsLightFactors(sk_ambientFactor3D, sk_diffuseFactor3D, sk_specularFactor3D);
  m_sliceMesh->setMaterialShininess(sk_shininess);
  m_sliceMesh->setUseOctantClipPlanes(false);

  // Slices may be drawn with normals backwards; do not cull triangles with backwards-facing normals
  m_sliceMesh->setBackfaceCull(false);

  // Image slices have no material, vertex, or 2D image coloring:
  m_sliceMesh->disableLayer(TexturedMeshColorLayer::Material);
  m_sliceMesh->disableLayer(TexturedMeshColorLayer::Vertex);
  m_sliceMesh->disableLayer(TexturedMeshColorLayer::Image2D);
  m_sliceMesh->enableLayer(TexturedMeshColorLayer::Image3D);
  m_sliceMesh->enableLayer(TexturedMeshColorLayer::Parcellation3D);

  // Set default layer opacities:
  m_sliceMesh->setLayerOpacityMultiplier(TexturedMeshColorLayer::Image3D, 1.0f);
  m_sliceMesh->setLayerOpacityMultiplier(TexturedMeshColorLayer::Parcellation3D, 1.0f);

  m_sliceMesh->setMaterialColor(sk_black);

  // Polygon offset used so that the image slices are always deeper in the scene
  // than other mesh objects without polygon offset defined.
  m_sliceMesh->setEnablePolygonOffset(true);

  m_sliceMesh
    ->setPolygonOffsetValues(PolygonOffset::imageSlices.first, PolygonOffset::imageSlices.second);
}

bool ImageSlice::isOpaque() const
{
  if (m_sliceMesh && m_sliceOutline)
  {
    return (m_sliceMesh->isOpaque() && m_sliceOutline->isOpaque());
  }

  return DrawableBase::isOpaque();
}

DrawableOpacity ImageSlice::opacityFlag() const
{
  return DrawableOpacity{OpacityFlag::Unknown, OpacityFlag::Unknown};
}

void ImageSlice::setImage3dRecord(std::weak_ptr<ImageRecord> imageRecord)
{
  m_image3dRecord = imageRecord;

  if (m_sliceMesh)
  {
    m_sliceMesh->setImage3dRecord(imageRecord);
  }
}

void ImageSlice::setParcellationRecord(std::weak_ptr<ParcellationRecord> labelsRecord)
{
  m_parcelRecord = labelsRecord;

  if (m_sliceMesh)
  {
    m_sliceMesh->setParcellationRecord(labelsRecord);
  }
}

void ImageSlice::setImageColorMapRecord(std::weak_ptr<ImageColorMapRecord> mapRecord)
{
  if (m_sliceMesh)
  {
    m_sliceMesh->setImageColorMapRecord(mapRecord);
  }
}

void ImageSlice::setLabelTableRecord(std::weak_ptr<LabelTableRecord> tableRecord)
{
  if (m_sliceMesh)
  {
    m_sliceMesh->setLabelTableRecord(tableRecord);
  }
}

void ImageSlice::setPositioningMethod(
  const intersection::PositioningMethod& method, const std::optional<glm::vec3>& p
)
{
  m_sliceIntersector.setPositioningMethod(method, p);
}

void ImageSlice::setAlignmentMethod(
  const intersection::AlignmentMethod& method, const std::optional<glm::vec3>& worldNormal
)
{
  m_sliceIntersector.setAlignmentMethod(method, worldNormal);
}

void ImageSlice::setShowOutline(bool show)
{
  m_showOutline = show;
}

void ImageSlice::setShowParcellation(bool show)
{
  if (m_sliceMesh)
  {
    m_sliceMesh
      ->setLayerOpacityMultiplier(TexturedMeshColorLayer::Parcellation3D, (show) ? 1.0f : 0.0f);
  }
}

void ImageSlice::setUseAutoHiding(bool set)
{
  if (m_sliceMesh)
  {
    m_sliceMesh->setUseAutoHidingMode(set);
  }
}

void ImageSlice::setUseIntensityThresolding(bool set)
{
  if (m_sliceMesh)
  {
    m_sliceMesh->setUseImage3dThresholdMode(set);
  }
}

void ImageSlice::doUpdate(
  double /*time*/, const Viewport&, const camera::Camera& camera, const CoordinateFrame& crosshairs
)
{
  using PositionType = glm::vec3;
  using NormalType = uint32_t;

  static constexpr GLintptr sk_offset = static_cast<GLintptr>(0);
  static constexpr GLintptr sk_positionsSize = static_cast<GLintptr>(
    sk_numVerts * sizeof(PositionType)
  );
  static constexpr GLintptr sk_normalsSize = static_cast<GLintptr>(sk_numVerts * sizeof(NormalType));

  if (!m_sliceMesh)
  {
    throw_debug("Null slice mesh");
  }

  auto sliceMeshGpuRecord = m_sliceMeshGpuRecord.lock();
  if (!sliceMeshGpuRecord)
  {
    throw_debug("Null mesh object record");
  }

  if (!m_sliceOutline)
  {
    throw_debug("Null line");
  }

  auto image3dRecord = m_image3dRecord.lock();
  if (!image3dRecord || !image3dRecord->cpuData())
  {
    // No image to render
    setVisible(false);
    return;
  }

  m_clip_O_camera = camera.clip_T_camera();
  m_camera_O_world = camera.camera_T_world();

  m_cameraIsOrthographic = camera.isOrthographic();

  m_worldCameraPos = worldOrigin(camera);
  m_worldCameraDir = worldDirection(camera, Directions::View::Back);

  std::array<glm::vec3, 8> subjectCorners;

  const auto& H = image3dRecord->cpuData()->header();

  for (uint32_t i = 0; i < 8; ++i)
  {
    subjectCorners[i] = glm::vec3{H.subjectBBoxCorners()[i]};
  }

  // Compute the intersections in Subject space by transforming the camera and crosshairs
  // from World to Subject space
  const glm::mat4& world_O_subject = image3dRecord->cpuData()->transformations().worldDef_T_subject(
  );
  const glm::mat4 subject_O_world = glm::inverse(world_O_subject);

  /// @todo We are currently ignoring the modeling transformation of this ImageSlice, i.e.:
  /// const glm::mat4 model_O_world = glm::transpose( get_world_O_this_invTrans() );
  /// If a non-identity Transformation is parent to this ImageSlice, then this tx
  /// should not be ignored

  std::optional<intersection::IntersectionVertices> subjectIntersectionPositions;
  glm::vec3 subjectPlaneNormal;

  std::tie(subjectIntersectionPositions, subjectPlaneNormal)
    = m_sliceIntersector.computePlaneIntersections(
      subject_O_world * camera.world_T_camera(),
      subject_O_world * crosshairs.world_T_frame(),
      subjectCorners
    );

  if (!subjectIntersectionPositions)
  {
    // No slice intersection to render
    setVisible(false);
    return;
  }
  else
  {
    setVisible(true);
  }

  // Convert Subject intersection positions and normal vector to World space
  intersection::IntersectionVertices worldIntersectionPositions;

  for (uint32_t i = 0; i < SliceIntersector::s_numVertices; ++i)
  {
    const glm::vec4 subjectPos{(*subjectIntersectionPositions)[i], 1.0f};
    worldIntersectionPositions[i] = glm::vec3{world_O_subject * subjectPos};
  }

  const glm::vec4 subjectNormal{subjectPlaneNormal, 0.0f};
  const glm::vec4 worldPlaneNormal = glm::inverseTranspose(world_O_subject) * subjectNormal;

  std::array<NormalType, sk_numVerts> slideNormalsBuffer;
  slideNormalsBuffer.fill(glm::packSnorm3x10_1x2(worldPlaneNormal));

  auto& positionsObject = sliceMeshGpuRecord->positionsObject();
  auto& normalsObject = sliceMeshGpuRecord->normalsObject();

  if (!normalsObject)
  {
    throw_debug("Null mesh normals objects");
  }

  positionsObject.write(sk_offset, sk_positionsSize, worldIntersectionPositions.data());
  normalsObject->write(sk_offset, sk_normalsSize, slideNormalsBuffer.data());

  if (intersection::AlignmentMethod::CameraZ == m_sliceIntersector.alignmentMethod())
  {
    m_sliceMesh->setAdsLightFactors(sk_ambientFactor2D, sk_diffuseFactor2D, sk_specularFactor2D);
  }
  else
  {
    m_sliceMesh->setAdsLightFactors(sk_ambientFactor3D, sk_diffuseFactor3D, sk_specularFactor3D);
  }

  if (m_showOutline)
  {
    m_sliceOutline->setVisible(true);
    m_sliceOutline->setVertices(glm::value_ptr(worldIntersectionPositions[0]), sk_numVerts - 1);
    m_sliceOutline->setColor(math::convertVecToRGB(glm::vec3{worldPlaneNormal}));
  }
  else
  {
    m_sliceOutline->setVisible(false);
  }
}
