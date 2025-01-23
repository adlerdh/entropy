#include "rendering_old/drawables/slides/SlideBox.h"

#include "rendering/utility/UnderlyingEnumType.h"
#include "rendering_old/ShaderNames.h"
#include "rendering_old/common/MeshColorLayer.h"
#include "rendering_old/drawables/TexturedMesh.h"
#include "rendering_old/drawables/Transformation.h"
#include "rendering_old/records/MeshGpuRecord.h"

#include "common/Exception.hpp"

#include "slideio/SlideHelper.h"

#include <glm/glm.hpp>

namespace
{

static const glm::vec3 sk_black(0.0f, 0.0f, 0.0f);

//static const glm::vec3 sk_activeSlideHighlightColor( 0.0f, 0.64f, 1.0f );
static const float sk_activeSlideHighlightOpacity = 0.15f;

} // namespace

SlideBox::SlideBox(
  std::string name,
  ShaderProgramActivatorType shaderProgramActivator,
  UniformsProviderType uniformsProvider,
  std::weak_ptr<BlankTextures> blankTextures,
  std::weak_ptr<MeshGpuRecord> boxMeshGpuRecord,
  std::weak_ptr<SlideRecord> slideRecord,
  QuerierType<bool, uuids::uuid> activeSlideQuerier,
  GetterType<float> image3dLayerOpacityProvider
)
  : DrawableBase(name, DrawableType::Slide)
  ,

  m_activeSlideQuerier(activeSlideQuerier)
  , m_image3dLayerOpacityProvider(image3dLayerOpacityProvider)
  ,

  m_boxMeshGpuRecord(boxMeshGpuRecord)
  , m_slideRecord(slideRecord)
  , m_stack_O_slide_tx(std::make_shared<Transformation>(name))
  ,

  m_boxMesh(std::make_shared<TexturedMesh>(
    name + "_boxMesh",
    shaderProgramActivator,
    uniformsProvider,
    blankTextures,
    [this]() -> MeshGpuRecord*
    {
      if (auto gpuRecord = m_boxMeshGpuRecord.lock())
      {
        return gpuRecord.get();
      }
      return nullptr;
    }
  ))
{
  m_renderId = static_cast<uint32_t>(underlyingType(m_type) << 12) | (numCreated() % 0x1000);

  setupChildren();
}

bool SlideBox::isOpaque() const
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

  if (m_boxMesh)
  {
    return m_boxMesh->isOpaque();
  }
  return DrawableBase::isOpaque();
}

DrawableOpacity SlideBox::opacityFlag() const
{
  return DrawableOpacity{OpacityFlag::Unknown, OpacityFlag::Unknown};
}

void SlideBox::setImage3dRecord(std::weak_ptr<ImageRecord> record)
{
  if (m_boxMesh)
  {
    m_boxMesh->setImage3dRecord(record);
  }
}

void SlideBox::setParcellationRecord(std::weak_ptr<ParcellationRecord> record)
{
  if (m_boxMesh)
  {
    m_boxMesh->setParcellationRecord(record);
  }
}

void SlideBox::setImageColorMapRecord(std::weak_ptr<ImageColorMapRecord> record)
{
  if (m_boxMesh)
  {
    m_boxMesh->setImageColorMapRecord(record);
  }
}

void SlideBox::setLabelTableRecord(std::weak_ptr<LabelTableRecord> record)
{
  if (m_boxMesh)
  {
    m_boxMesh->setLabelTableRecord(record);
  }
}

void SlideBox::setUseIntensityThresolding(bool set)
{
  m_boxMesh->setUseImage2dThresholdMode(set);
}

void SlideBox::setupChildren()
{
  if (!m_stack_O_slide_tx || !m_boxMesh)
  {
    throw_debug("Null child drawable");
  }

  addChild(m_stack_O_slide_tx);
  m_stack_O_slide_tx->addChild(m_boxMesh);

  m_boxMesh->setAdsLightFactors(0.30f, 0.55f, 0.15f);
  m_boxMesh->setPickable(true);
  m_boxMesh->setUseOctantClipPlanes(false);

  // Enable backface culling, so that we do not see back faces or "inside" of slides,
  // even if they are partially transparent.
  m_boxMesh->setBackfaceCull(true);

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

  m_boxMesh->setLayerPermutation(layerPerm);

  // Slides never use the vertex coloring layer.
  m_boxMesh->disableLayer(TexturedMeshColorLayer::Vertex);
  m_boxMesh->enableLayer(TexturedMeshColorLayer::Image2D);
  m_boxMesh->enableLayer(TexturedMeshColorLayer::Image3D);
  m_boxMesh->enableLayer(TexturedMeshColorLayer::Parcellation3D);
  m_boxMesh->enableLayer(TexturedMeshColorLayer::Material);

  // By default, only display the "Image2D" (slide texture) layer.
  m_boxMesh->setLayerOpacityMultiplier(TexturedMeshColorLayer::Image2D, 1.0f);
  m_boxMesh->setLayerOpacityMultiplier(TexturedMeshColorLayer::Image3D, 0.0f);
  m_boxMesh->setLayerOpacityMultiplier(TexturedMeshColorLayer::Parcellation3D, 0.0f);
  m_boxMesh->setLayerOpacityMultiplier(TexturedMeshColorLayer::Material, 0.0f);

  m_boxMesh->setMaterialColor(sk_black);
}

void SlideBox::doUpdate(double, const Viewport&, const camera::Camera&, const CoordinateFrame&)
{
  auto record = m_slideRecord.lock();

  if (!record || !record->cpuData() || !record->gpuData() || !m_stack_O_slide_tx)
  {
    std::cerr << "Null slide record during update of " << m_name << std::endl;
    setVisible(false);
    return;
  }

  m_stack_O_slide_tx->setMatrix(slideio::stack_O_slide(*(record->cpuData())));
  m_boxMesh->setTexture2d(record->gpuData()->texture());

  const auto& slideProps = record->cpuData()->properties();

  const std::pair<uint8_t, uint8_t> thresh = slideProps.intensityThresholds();
  m_boxMesh->setTexture2dThresholds({thresh.first / 255.0f, thresh.second / 255.0f});
  m_boxMesh->setImage2dThresholdsActive(slideProps.thresholdsActive());

  m_boxMesh->setVisible(slideProps.visible());
  m_boxMesh->setLayerOpacityMultiplier(TexturedMeshColorLayer::Image2D, slideProps.opacity());

  /// @todo set these:
  //    slide.m_annotVisible = props.annotVisible();
  //    slide.m_annotOpacity = props.annotOpacity();

  if (m_activeSlideQuerier(record->uid()))
  {
    // If this is the active slide, then highlight it.
    m_boxMesh
      ->setLayerOpacityMultiplier(TexturedMeshColorLayer::Material, sk_activeSlideHighlightOpacity);
    m_boxMesh->setMaterialColor(slideProps.borderColor());
  }
  else
  {
    m_boxMesh->setLayerOpacityMultiplier(TexturedMeshColorLayer::Material, 0.0f);
    m_boxMesh->setMaterialColor(sk_black);
  }

  // Set the opacity of the Image3D and Parcellation3D layers.
  if (m_image3dLayerOpacityProvider)
  {
    const float layerOp = m_image3dLayerOpacityProvider();
    m_boxMesh
      ->setLayerOpacityMultiplier(TexturedMeshColorLayer::Image3D, layerOp * slideProps.opacity());
    m_boxMesh->setLayerOpacityMultiplier(
      TexturedMeshColorLayer::Parcellation3D, layerOp * slideProps.opacity()
    );
  }
  else
  {
    m_boxMesh->setLayerOpacityMultiplier(TexturedMeshColorLayer::Image3D, 0.0f);
    m_boxMesh->setLayerOpacityMultiplier(TexturedMeshColorLayer::Parcellation3D, 0.0f);
  }

  setVisible(true);
}
