#include "rendering_old/drawables/slides/SlideSlice.h"

#include "rendering/utility/UnderlyingEnumType.h"
#include "rendering/utility/gl/GLDrawTypes.h"
#include "rendering_old/ShaderNames.h"
#include "rendering_old/common/MeshPolygonOffset.h"
#include "rendering_old/drawables/Line.h"
#include "rendering_old/drawables/TexturedMesh.h"
#include "rendering_old/drawables/Transformation.h"
#include "rendering_old/records/MeshGpuRecord.h"

#include "common/Exception.hpp"

#include "logic/camera/CameraHelpers.h"
#include "logic/camera/MathUtility.h"

#include "slideio/SlideHelper.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_inverse.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/packing.hpp>
#include <glm/gtc/type_ptr.hpp>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/norm.hpp>
#include <glm/gtx/string_cast.hpp>
#include <glm/gtx/transform.hpp>

#include <algorithm>
#include <array>

namespace
{

static const glm::vec3 sk_black(0.0f, 0.0f, 0.0f);

/// Default color to highlight the slide when it is active
//static const glm::vec3 sk_activeSlideHighlightColor( 0.0f, 0.64f, 1.0f );

/// Default opacity to apply to the slide's highlight
static const float sk_activeSlideHighlightOpacity = 0.15f;

/// Number of vertices in a slide-plane (i.e. slice) intersection polygon
/// (which is a hexagon; the additional vertex set as the center hub)
static constexpr int sk_numVerts = 7;

} // namespace

SlideSlice::SlideSlice(
  std::string name,
  ShaderProgramActivatorType shaderProgramActivator,
  UniformsProviderType uniformsProvider,
  std::weak_ptr<BlankTextures> blankTextures,
  std::weak_ptr<MeshGpuRecord> sliceMeshGpuRecord,
  std::weak_ptr<SlideRecord> slideRecord,
  QuerierType<bool, uuids::uuid> activeSlideQuerier,
  GetterType<float> image3dLayerOpacityProvider
)
  : DrawableBase(std::move(name), DrawableType::SlideSlice)
  ,

  m_activeSlideQuerier(activeSlideQuerier)
  , m_image3dLayerOpacityProvider(image3dLayerOpacityProvider)
  ,

  m_sliceMeshGpuRecord(sliceMeshGpuRecord)
  , m_slideRecord(slideRecord)
  ,

  m_stack_O_slide_tx(std::make_shared<Transformation>(name, glm::mat4{1.0f}))
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

  m_modelPlaneNormal(1.0f, 0.0f, 0.0f)
  , m_clip_O_camera(1.0f)
  , m_camera_O_world(1.0f)
  , m_worldCameraPos(0.0f, 0.0f, 0.0f)
  , m_showOutline(false)
  , m_sliceIntersector()
{
  /// @note could move << 13 then have 2^13 for numcreated
  m_renderId = static_cast<uint32_t>(underlyingType(m_type) << 12) | (numCreated() % 0x1000);

  m_sliceIntersector.setAlignmentMethod(intersection::AlignmentMethod::CameraZ);
  m_sliceIntersector.setPositioningMethod(intersection::PositioningMethod::FrameOrigin);

  setupChildren();
}

bool SlideSlice::isOpaque() const
{
  //    auto slideRecord = m_slideRecord.lock();
  //    if ( slideRecord && slideRecord->cpuData() )
  //    {
  //        if ( slideRecord->cpuData()->header().hasTransparency() )
  //        {
  //            // If the slide has transparency, then it's not opaque
  //            return false;
  //        }
  //    }

  if (m_sliceMesh && m_sliceOutline)
  {
    return (m_sliceMesh->isOpaque() && m_sliceOutline->isOpaque());
  }
  return DrawableBase::isOpaque();
}

DrawableOpacity SlideSlice::opacityFlag() const
{
  return DrawableOpacity{OpacityFlag::Unknown, OpacityFlag::Unknown};
}

void SlideSlice::setImage3dRecord(std::weak_ptr<ImageRecord> record)
{
  if (m_sliceMesh)
  {
    m_sliceMesh->setImage3dRecord(record);
  }
}

void SlideSlice::setParcellationRecord(std::weak_ptr<ParcellationRecord> record)
{
  if (m_sliceMesh)
  {
    m_sliceMesh->setParcellationRecord(record);
  }
}

void SlideSlice::setImageColorMapRecord(std::weak_ptr<ImageColorMapRecord> record)
{
  if (m_sliceMesh)
  {
    m_sliceMesh->setImageColorMapRecord(record);
  }
}

void SlideSlice::setLabelTableRecord(std::weak_ptr<LabelTableRecord> record)
{
  if (m_sliceMesh)
  {
    m_sliceMesh->setLabelTableRecord(record);
  }
}

void SlideSlice::setUseIntensityThresolding(bool set)
{
  m_sliceMesh->setUseImage2dThresholdMode(set);
}

void SlideSlice::setPositioningMethod(
  const intersection::PositioningMethod& method, const std::optional<glm::vec3>& p
)
{
  m_sliceIntersector.setPositioningMethod(method, p);
}

void SlideSlice::setAlignmentMethod(
  const intersection::AlignmentMethod& method, const std::optional<glm::vec3>& worldNormal
)
{
  m_sliceIntersector.setAlignmentMethod(method, worldNormal);
}

void SlideSlice::setShowOutline(const bool show)
{
  m_showOutline = show;
}

void SlideSlice::setupChildren()
{
  if (!m_stack_O_slide_tx || !m_sliceMesh || !m_sliceOutline)
  {
    throw_debug("Null child drawable");
  }

  addChild(m_stack_O_slide_tx);
  m_stack_O_slide_tx->addChild(m_sliceMesh);

  if (m_showOutline)
  {
    m_stack_O_slide_tx->addChild(m_sliceOutline);
  }
  else
  {
    if (auto s = m_sliceOutline)
    {
      m_stack_O_slide_tx->removeChild(*s);
    }
  }

  // Use no lighting on slide slices, with only ambient contribution equal to the texture value.
  m_sliceMesh->setAdsLightFactors(1.0f, 0.0f, 0.0f);
  m_sliceMesh->setUseOctantClipPlanes(false);

  // Disable backface culling, since slides may be drawn with normals backwards.
  m_sliceMesh->setBackfaceCull(false);

  // Define the ordering of layers for the slide box mesh. Layer "Image2D" is the slide image;
  // layers "Image3D" and "Parcellation3D" are from the 3D reference image; and layer "Material"
  // is for highlightin the slide.
  std::array<TexturedMeshColorLayer, static_cast<size_t>(TexturedMeshColorLayer::NumLayers)>
    layerPerm;

  layerPerm[0] = TexturedMeshColorLayer::Vertex; // bottom layer
  layerPerm[1] = TexturedMeshColorLayer::Image2D;
  layerPerm[2] = TexturedMeshColorLayer::Image3D;
  layerPerm[3] = TexturedMeshColorLayer::Parcellation3D;
  layerPerm[4] = TexturedMeshColorLayer::Material; // top layer

  m_sliceMesh->setLayerPermutation(layerPerm);

  // Set default layer opacities. Slides never use the vertex coloring layer.
  m_sliceMesh->disableLayer(TexturedMeshColorLayer::Vertex);
  m_sliceMesh->enableLayer(TexturedMeshColorLayer::Image2D);
  m_sliceMesh->enableLayer(TexturedMeshColorLayer::Image3D);
  m_sliceMesh->enableLayer(TexturedMeshColorLayer::Parcellation3D);
  m_sliceMesh->enableLayer(TexturedMeshColorLayer::Material);

  // By default, only display the "Image2D" (slide texture) layer.
  m_sliceMesh->setLayerOpacityMultiplier(TexturedMeshColorLayer::Image2D, 1.0f);
  m_sliceMesh->setLayerOpacityMultiplier(TexturedMeshColorLayer::Image3D, 0.0f);
  m_sliceMesh->setLayerOpacityMultiplier(TexturedMeshColorLayer::Parcellation3D, 0.0f);
  m_sliceMesh->setLayerOpacityMultiplier(TexturedMeshColorLayer::Material, 0.0f);

  m_sliceMesh->setMaterialColor(sk_black);

  // Polygon offset enabled so that slide slices are rendered nearer the viewer
  // than other objects without polygon offset defined. This substantially eliminates
  // Z-fighting with image slices and other meshes.
  m_sliceMesh->setEnablePolygonOffset(true);
  m_sliceMesh
    ->setPolygonOffsetValues(PolygonOffset::slideSlices.first, PolygonOffset::slideSlices.second);
}

void SlideSlice::doUpdate(
  double /*time*/, const Viewport&, const camera::Camera& camera, const CoordinateFrame& crosshairs
)
{
  static constexpr GLintptr sk_offset = static_cast<GLintptr>(0);
  static constexpr GLintptr sk_positionsSize = static_cast<GLintptr>(
    sk_numVerts * sizeof(PositionType)
  );
  static constexpr GLintptr sk_normalsSize = static_cast<GLintptr>(sk_numVerts * sizeof(NormalType));
  static constexpr GLintptr sk_texCoordsSize = static_cast<GLintptr>(
    sk_numVerts * sizeof(TexCoord2DType)
  );

  // Slide corners are defined in "modeling" coordinates of the slide:
  static const PositionType p000{0.0, 0.0, 0.0};
  static const PositionType p001{0.0, 0.0, 1.0};
  static const PositionType p010{0.0, 1.0, 0.0};
  static const PositionType p011{0.0, 1.0, 1.0};
  static const PositionType p100{1.0, 0.0, 0.0};
  static const PositionType p101{1.0, 0.0, 1.0};
  static const PositionType p110{1.0, 1.0, 0.0};
  static const PositionType p111{1.0, 1.0, 1.0};

  static const std::array<PositionType, 8> sk_slideCorners = {
    {p000, p001, p010, p011, p100, p101, p110, p111}
  };

  if (!m_sliceMesh)
  {
    throw_debug("Null slice mesh");
  }

  if (!m_sliceOutline)
  {
    throw_debug("Null line");
  }

  auto slideRecord = m_slideRecord.lock();
  if (!slideRecord || !slideRecord->cpuData() || !slideRecord->gpuData())
  {
    std::cerr << "Null slide record during update" << std::endl;
    setVisible(false);
    return;
  }

  auto sliceMeshGpuRecord = m_sliceMeshGpuRecord.lock();
  if (!sliceMeshGpuRecord)
  {
    throw_debug("Null mesh object record");
  }

  const glm::mat4 stack_O_slide = slideio::stack_O_slide(*(slideRecord->cpuData()));

  m_stack_O_slide_tx->setMatrix(stack_O_slide);
  m_sliceMesh->setTexture2d(slideRecord->gpuData()->texture());

  // Compute the intersections in slide space by transforming the camera and crosshairs from
  // world to slide space ("model" space is stack space for this Drawable)
  const glm::mat4 world_O_stack = getAccumulatedRenderingData().m_world_T_object;

  const glm::mat4 slide_O_world = glm::inverse(world_O_stack * stack_O_slide);
  const glm::mat4 slide_O_camera = slide_O_world * camera.world_T_camera();
  const glm::mat4 slide_O_crosshairsFrame = slide_O_world * crosshairs.world_T_frame();

  std::optional<intersection::IntersectionVertices> slideIntersectionPositions;

  std::tie(slideIntersectionPositions, m_modelPlaneNormal)
    = m_sliceIntersector
        .computePlaneIntersections(slide_O_camera, slide_O_crosshairsFrame, sk_slideCorners);

  if (!slideIntersectionPositions)
  {
    setVisible(false);
    return;
  }

  std::array<NormalType, sk_numVerts> slideNormalsBuffer;
  slideNormalsBuffer.fill(glm::packSnorm3x10_1x2(glm::vec4{m_modelPlaneNormal, 0.0f}));

  std::array<glm::vec2, sk_numVerts> texCoords;

  for (uint32_t i = 0; i < sk_numVerts; ++i)
  {
    texCoords[i] = glm::vec2{(*slideIntersectionPositions)[i] - p000};
  }

  // Offset slice positions towards the viewer. Increase offset of the slide slice layer by
  // an additional 2, to make sure that there is no z-fighting with image slices.
  std::vector<glm::vec3>
    positions(std::begin(*slideIntersectionPositions), std::end(*slideIntersectionPositions));

  math::applyLayeringOffsetsToModelPositions(camera, slide_O_world, 2, positions);

  auto& positionsObject = sliceMeshGpuRecord->positionsObject();
  auto& normalsObject = sliceMeshGpuRecord->normalsObject();
  auto& texCoordsObject = sliceMeshGpuRecord->texCoordsObject();

  if (!normalsObject || !texCoordsObject)
  {
    throw_debug("Null mesh normals and texCoords objects");
  }

  positionsObject.write(sk_offset, sk_positionsSize, positions.data());
  normalsObject->write(sk_offset, sk_normalsSize, slideNormalsBuffer.data());
  texCoordsObject->write(sk_offset, sk_texCoordsSize, texCoords.data());

  if (m_showOutline)
  {
    m_sliceOutline->setVisible(true);
    m_sliceOutline->setVertices(glm::value_ptr(positions[0]), sk_numVerts);
    m_sliceOutline->setColor(glm::vec4{math::convertVecToRGB(m_modelPlaneNormal), 1.0f});
  }
  else
  {
    m_sliceOutline->setVisible(false);
  }

  // Set properties
  const auto& slideProps = slideRecord->cpuData()->properties();

  const std::pair<uint8_t, uint8_t> thresh = slideProps.intensityThresholds();
  m_sliceMesh->setTexture2dThresholds({thresh.first / 255.0f, thresh.second / 255.0f});
  m_sliceMesh->setImage2dThresholdsActive(slideProps.thresholdsActive());

  m_sliceMesh->setVisible(slideProps.visible());
  m_sliceMesh->setLayerOpacityMultiplier(TexturedMeshColorLayer::Image2D, slideProps.opacity());

  if (m_activeSlideQuerier(slideRecord->uid()))
  {
    // If this is the active slide, then highlight it.
    m_sliceMesh
      ->setLayerOpacityMultiplier(TexturedMeshColorLayer::Material, sk_activeSlideHighlightOpacity);
    m_sliceMesh->setMaterialColor(slideProps.borderColor());
  }
  else
  {
    m_sliceMesh->setLayerOpacityMultiplier(TexturedMeshColorLayer::Material, 0.0f);
    m_sliceMesh->setMaterialColor(sk_black);
  }

  if (m_image3dLayerOpacityProvider)
  {
    // Set the opacity of the Image3D and Parcellation3D layers.
    m_sliceMesh->setLayerOpacityMultiplier(
      TexturedMeshColorLayer::Image3D, m_image3dLayerOpacityProvider() * slideProps.opacity()
    );

    m_sliceMesh->setLayerOpacityMultiplier(
      TexturedMeshColorLayer::Parcellation3D, m_image3dLayerOpacityProvider() * slideProps.opacity()
    );
  }
  else
  {
    m_sliceMesh->setLayerOpacityMultiplier(TexturedMeshColorLayer::Image3D, 0.0f);
    m_sliceMesh->setLayerOpacityMultiplier(TexturedMeshColorLayer::Parcellation3D, 0.0f);
  }

  setVisible(true);
}
