#include "rendering/Rendering.h"

#include "common/Exception.hpp"
#include "common/Types.h"
#include "common/UuidUtility.h"

#include "image/ImageColorMap.h"
#include "image/SurfaceUtility.h"

#include "logic/app/Data.h"
#include "logic/camera/CameraHelpers.h"
#include "logic/camera/MathUtility.h"
#include "logic/states/AnnotationStateHelpers.h"
#include "logic/states/FsmList.hpp"

#include "rendering/ImageDrawing.h"
#include "rendering/TextureSetup.h"
#include "rendering/VectorDrawing.h"
#include "rendering/utility/containers/Uniforms.h"
#include "rendering/utility/gl/GLShader.h"

/**************************/
#include "rendering/renderers/DepthPeelRenderer.h"
#include "rendering/utility/CreateGLObjects.h"
//#include "rendering_old/utility/containers/BlankTextures.h"
//#include "rendering/utility/vtk/PolyDataGenerator.h"
/**************************/

#include "windowing/View.h"

#include <cmrc/cmrc.hpp>

#include <spdlog/fmt/ostr.h>
#include <spdlog/spdlog.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_inverse.hpp>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/component_wise.hpp>
#include <glm/gtx/string_cast.hpp>
#include <glm/gtx/transform.hpp>

#include <glad/glad.h>

#include <nanovg.h>
#define NANOVG_GL3_IMPLEMENTATION
#include <nanovg_gl.h>

#include <algorithm>
#include <functional>
#include <list>
#include <memory>
#include <sstream>
#include <tuple>
#include <unordered_map>
#include <utility>
#include <vector>

CMRC_DECLARE(fonts);
CMRC_DECLARE(shaders);

// These types are used when setting uniforms in the shaders
using FloatVector = std::vector<float>;
using Mat4Vector = std::vector<glm::mat4>;
using Vec2Vector = std::vector<glm::vec2>;
using Vec3Vector = std::vector<glm::vec3>;

namespace
{

static const glm::vec3 WHITE{1.0f};

static const glm::mat4 sk_identMat3{1.0f};
static const glm::mat4 sk_identMat4{1.0f};

static const glm::vec2 sk_zeroVec2{0.0f, 0.0f};
static const glm::vec3 sk_zeroVec3{0.0f, 0.0f, 0.0f};
static const glm::vec4 sk_zeroVec4{0.0f, 0.0f, 0.0f, 0.0f};

static const glm::ivec2 sk_zeroIVec2{0, 0};

static const std::string ROBOTO_LIGHT("robotoLight");

/**
 * @brief Create the Dual-Depth Peel renderer for a given view
 * @param viewUid View UID
 * @param shaderActivator Function that activates shader programs
 * @param uniformsProvider Function returning the uniforms
 * @return Unique pointer to the renderer
 */
std::unique_ptr<DepthPeelRenderer> createDdpRenderer(
  int viewUid,
  ShaderProgramActivatorType shaderActivator,
  UniformsProviderType uniformsProvider,
  GetterType<IDrawable*> rootProvider,
  GetterType<IDrawable*> overlayProvider
)
{
  std::ostringstream name;
  name << "DdpRenderer_" << viewUid << std::ends;

  auto renderer = std::make_unique<DepthPeelRenderer>(
    name.str(), shaderActivator, uniformsProvider, rootProvider, overlayProvider
  );

  // Maximum number of dual depth peeling iterations. Three iterations enables
  // 100% pixel perfect rendering of six transparent layers.
  static constexpr uint32_t sk_maxPeels = 3;
  renderer->setMaxNumberOfPeels(sk_maxPeels);

  // Override the maximum depth peel limit by using occlusion queries.
  // Using an occlusion ratio of 0.0 means that as many peels are
  // performed as necessary in order to render the scene transparency correctly.
  renderer->setOcclusionRatio(0.0f);

  return renderer;
}

} // namespace

/// @note OpenGL should have a at least a minimum of 16 texture units

const Uniforms::SamplerIndexVectorType Rendering::msk_imgTexSamplers{{0, 1}};
const Uniforms::SamplerIndexVectorType Rendering::msk_segTexSamplers{{2, 3}};
const Uniforms::SamplerIndexVectorType Rendering::msk_labelTableTexSamplers{{4, 5}};
const Uniforms::SamplerIndexVectorType Rendering::msk_imgCmapTexSamplers{{6, 7}};
const Uniforms::SamplerIndexType Rendering::msk_metricCmapTexSampler{6};

const Uniforms::SamplerIndexType Rendering::msk_imgTexSampler{0};
const Uniforms::SamplerIndexType Rendering::msk_segTexSampler{1};
const Uniforms::SamplerIndexType Rendering::msk_imgCmapTexSampler{2};
const Uniforms::SamplerIndexType Rendering::msk_labelTableTexSampler{3};
const Uniforms::SamplerIndexVectorType Rendering::msk_imgRgbaTexSamplers{{0, 5, 6, 7}};

const Uniforms::SamplerIndexType Rendering::msk_jumpTexSampler{4};

Rendering::Rendering(AppData& appData)
  : m_appData(appData)
  ,

  m_nvg(nvgCreateGL3(NVG_ANTIALIAS | NVG_STENCIL_STROKES /*| NVG_DEBUG*/))
  ,

  m_crossCorrelationProgram("CrossCorrelationProgram")
  , m_differenceProgram("DifferenceProgram")
  , m_edgeProgram("EdgeProgram")
  , m_imageProgram("ImageProgram")
  , m_imageRgbaProgram("ImageRgbaProgram")
  , m_xrayProgram("XrayProgram")
  , m_overlayProgram("OverlayProgram")
  , m_raycastIsoSurfaceProgram("RayCastIsoSurfaceProgram")
  , m_simpleProgram("SimpleProgram")
  ,

  m_isAppDoneLoadingImages(false)
  , m_showOverlays(true)
{
  if (!m_nvg)
  {
    spdlog::error("Could not initialize 'nanovg' vector graphcis library. "
                  "Proceeding without vector graphics.");
  }

  try
  {
    // Load the font for anatomical labels:
    auto filesystem = cmrc::fonts::get_filesystem();
    const cmrc::file robotoFont = filesystem.open("resources/fonts/Roboto/Roboto-Light.ttf");

    const int robotoLightFont = nvgCreateFontMem(
      m_nvg,
      ROBOTO_LIGHT.c_str(),
      reinterpret_cast<uint8_t*>(const_cast<char*>(robotoFont.begin())),
      static_cast<int32_t>(robotoFont.size()),
      0
    );

    if (-1 == robotoLightFont)
    {
      spdlog::error("Could not load font {}", ROBOTO_LIGHT);
    }
  }
  catch (const std::exception& e)
  {
    spdlog::error("Exception when loading font file: {}", e.what());
  }

  createShaderPrograms();

  /***************************************************/

  // This is a shared pointer, since it gets passed down to rendering
  // objects where it is held as a weak pointer
  // auto blankTextures = std::make_shared<BlankTextures>();

  // Constructs, manages, and modifies the assemblies of Drawables that are rendered.
  // It passes the shader programs and blank textures down to the Drawables.
  //    auto assemblyManager = std::make_unique<AssemblyManager>(
  //                *dataManager,
  //                std::bind( &ShaderProgramContainer::useProgram, shaderPrograms.get(), _1 ),
  //                std::bind( &ShaderProgramContainer::getRegisteredUniforms, shaderPrograms.get(), _1 ),
  //                blankTextures );
  /***************************************************/

  m_shaderPrograms = std::make_unique<ShaderProgramContainer>();
  m_shaderPrograms->initializeGL();

  m_shaderActivator
    = std::bind(&ShaderProgramContainer::useProgram, m_shaderPrograms.get(), std::placeholders::_1);
  m_uniformsProvider = std::bind(
    &ShaderProgramContainer::getRegisteredUniforms, m_shaderPrograms.get(), std::placeholders::_1
  );

  const glm::dvec3 center{0.0};
  const double radius = 100.0;
  const double height = 200.0;

  m_cylinderGpuMeshRecord = gpuhelper::createCylinderMeshGpuRecord(center, radius, height);
  m_basicMesh = std::make_unique<
    BasicMesh>("TestCylinder", m_shaderActivator, m_uniformsProvider, m_cylinderGpuMeshRecord);

  m_rootDrawableProvider = [this]() -> IDrawable* { return m_basicMesh.get(); };

  m_overlayDrawableProvider = []() -> IDrawable* { return nullptr; };

  int viewUid = 0;

  m_renderer = createDdpRenderer(
    viewUid, m_shaderActivator, m_uniformsProvider, m_rootDrawableProvider, m_overlayDrawableProvider
  );

  m_renderer->initialize();
}

Rendering::~Rendering()
{
  if (m_nvg)
  {
    nvgDeleteGL3(m_nvg);
    m_nvg = nullptr;
  }
}

void Rendering::setupOpenGlState()
{
  glEnable(GL_MULTISAMPLE);
  glDisable(GL_DEPTH_TEST);
  glEnable(GL_STENCIL_TEST);
  glDisable(GL_SCISSOR_TEST);
  glEnable(GL_BLEND);
  glDisable(GL_CULL_FACE);

  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glFrontFace(GL_CCW);

  // This is the state touched by NanoVG:
  //    glEnable(GL_CULL_FACE);
  //    glCullFace(GL_BACK);
  //    glFrontFace(GL_CCW);
  //    glEnable(GL_BLEND);
  //    glDisable(GL_DEPTH_TEST);
  //    glDisable(GL_SCISSOR_TEST);
  //    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
  //    glStencilMask(0xffffffff);
  //    glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
  //    glStencilFunc(GL_ALWAYS, 0, 0xffffffff);
  //    glActiveTexture(GL_TEXTURE0);
  //    glBindTexture(GL_TEXTURE_2D, 0);

#if 0
    int drawBuffer;
    glGetIntegerv( GL_DRAW_BUFFER, &drawBuffer );
    spdlog::info( "drawBuffer = {}", drawBuffer ); // GL_BACK;

    int framebufferAttachmentParameter;

    glGetFramebufferAttachmentParameteriv(
                GL_DRAW_FRAMEBUFFER,
                GL_BACK_LEFT,
                GL_FRAMEBUFFER_ATTACHMENT_COLOR_ENCODING,
                &framebufferAttachmentParameter );

    spdlog::info( "framebufferAttachmentParameter = {}", framebufferAttachmentParameter ); // GL_LINEAR;
#endif
}

void Rendering::init()
{
  nvgReset(m_nvg);
}

void Rendering::initTextures()
{
  m_appData.renderData().m_labelBufferTextures = createLabelColorTableTextures(m_appData);

  if (m_appData.renderData().m_labelBufferTextures.empty())
  {
    spdlog::critical("No label buffer textures loaded");
    throw_debug("No label buffer textures loaded")
  }

  m_appData.renderData().m_colormapTextures = createImageColorMapTextures(m_appData);

  if (m_appData.renderData().m_colormapTextures.empty())
  {
    spdlog::critical("No image color map textures loaded");
    throw_debug("No image color map textures loaded")
  }

  const std::vector<uuids::uuid> imageUidsOfCreatedTextures
    = createImageTextures(m_appData, m_appData.imageUidsOrdered());

  if (imageUidsOfCreatedTextures.size() != m_appData.numImages())
  {
    spdlog::error("Not all image textures were created");
    /// TODO: remove the image sfor which the texture was not created
  }

  const std::vector<uuids::uuid> segUidsOfCreatedTextures
    = createSegTextures(m_appData, m_appData.segUidsOrdered());

  if (segUidsOfCreatedTextures.size() != m_appData.numSegs())
  {
    spdlog::error("Not all segmentation textures were created");
    /// TODO: remove the segs for which the texture was not created
  }

  m_appData.renderData().m_distanceMapTextures = createDistanceMapTextures(m_appData);

  m_isAppDoneLoadingImages = true;
}

bool Rendering::createLabelColorTableTexture(const uuids::uuid& labelTableUid)
{
  // static const glm::vec4 sk_border{ 0.0f, 0.0f, 0.0f, 0.0f };

  const auto* table = m_appData.labelTable(labelTableUid);
  if (!table)
  {
    spdlog::warn("Label table {} is invalid", labelTableUid);
    return false;
  }

  int maxBufTexSize;
  glGetIntegerv(GL_MAX_TEXTURE_BUFFER_SIZE, &maxBufTexSize);

  if (table->numColorBytes_RGBA_U8() > static_cast<size_t>(maxBufTexSize))
  {
    spdlog::error(
      "Number of bytes ({}) in label color table {} exceeds "
      "maximum buffer texture size of {} bytes",
      table->numColorBytes_RGBA_U8(),
      labelTableUid,
      maxBufTexSize
    );
    return false;
  }

  auto it = m_appData.renderData().m_labelBufferTextures.emplace(
    std::piecewise_construct,
    std::forward_as_tuple(labelTableUid),
    std::forward_as_tuple(table->bufferTextureFormat_RGBA_U8(), BufferUsagePattern::StaticDraw)
  );

  if (!it.second)
    return false;

  GLBufferTexture& T = it.first->second;

  T.generate();

  T.allocate(table->numColorBytes_RGBA_U8(), table->colorData_RGBA_nonpremult_U8());

  spdlog::debug("Generated buffer texture for label color table {}", labelTableUid);
  return true;
}

bool Rendering::removeSegTexture(const uuids::uuid& segUid)
{
  const auto* seg = m_appData.seg(segUid);
  if (!seg)
  {
    spdlog::warn("Segmentation {} is invalid", segUid);
    return false;
  }

  auto it = m_appData.renderData().m_segTextures.find(segUid);

  if (std::end(m_appData.renderData().m_segTextures) == it)
  {
    spdlog::warn("Texture for segmentation {} does not exist and cannot be removed", segUid);
    return false;
  }

  m_appData.renderData().m_segTextures.erase(it);
  return true;
}

/*
bool Rendering::createImageTexture( const uuids::uuid& imageUid )
{
    static constexpr GLint sk_mipmapLevel = 0; // Load seg data into first mipmap level
    static constexpr GLint sk_alignment = 1; // Pixel pack/unpack alignment is 1 byte
    static const tex::WrapMode sk_wrapMode = tex::WrapMode::ClampToBorder;
    static const glm::vec4 sk_border{ 0.0f, 0.0f, 0.0f, 0.0f }; // Black border

    static const tex::MinificationFilter sk_minFilter = tex::MinificationFilter::Nearest;
    static const tex::MagnificationFilter sk_maxFilter = tex::MagnificationFilter::Nearest;

    GLTexture::PixelStoreSettings pixelPackSettings;
    pixelPackSettings.m_alignment = sk_alignment;
    GLTexture::PixelStoreSettings pixelUnpackSettings = pixelPackSettings;

    const auto* seg = m_appData.seg( segUid );
    if ( ! seg )
    {
        spdlog::warn( "Segmentation {} is invalid", segUid );
        return false;
    }

    const ComponentType compType = seg->header().memoryComponentType();

    auto it = m_appData.renderData().m_segTextures.try_emplace(
        segUid,
        tex::Target::Texture3D,
        GLTexture::MultisampleSettings(),
        pixelPackSettings, pixelUnpackSettings );

    if ( ! it.second ) return false;
    GLTexture& T = it.first->second;

    T.generate();
    T.setMinificationFilter( sk_minFilter );
    T.setMagnificationFilter( sk_maxFilter );
    T.setBorderColor( sk_border );
    T.setWrapMode( sk_wrapMode );
    T.setAutoGenerateMipmaps( false );
    T.setSize( seg->header().pixelDimensions() );

    T.setData( sk_mipmapLevel,
              GLTexture::getSizedInternalRedFormat( compType ),
              GLTexture::getBufferPixelRedFormat( compType ),
              GLTexture::getBufferPixelDataType( compType ),
              seg->bufferAsVoid( comp ) );

    spdlog::debug( "Created texture for segmentation {} ('{}')",
                  segUid, seg->settings().displayName() );

    return true;
}
*/

void Rendering::updateSegTexture(
  const uuids::uuid& segUid,
  const ComponentType& compType,
  const glm::uvec3& startOffsetVoxel,
  const glm::uvec3& sizeInVoxels,
  const void* data
)
{
  // Load seg data into first mipmap level
  static constexpr GLint sk_mipmapLevel = 0;

  auto it = m_appData.renderData().m_segTextures.find(segUid);
  if (std::end(m_appData.renderData().m_segTextures) == it)
  {
    spdlog::error("Cannot update segmentation {}: texture not found.", segUid);
    return;
  }

  GLTexture& T = it->second;

  const auto* seg = m_appData.seg(segUid);

  if (!seg)
  {
    spdlog::warn("Segmentation {} is invalid", segUid);
    return;
  }

  T.setSubData(
    sk_mipmapLevel,
    startOffsetVoxel,
    sizeInVoxels,
    GLTexture::getBufferPixelRedFormat(compType),
    GLTexture::getBufferPixelDataType(compType),
    data
  );
}

void Rendering::updateSegTextureWithInt64Data(
  const uuids::uuid& segUid,
  const ComponentType& compType,
  const glm::uvec3& startOffsetVoxel,
  const glm::uvec3& sizeInVoxels,
  const int64_t* data
)
{
  if (!data)
  {
    spdlog::error("Null segmentation texture data pointer");
    return;
  }

  if (!isValidSegmentationComponentType(compType))
  {
    spdlog::error(
      "Unable to update segmentation texture using buffer with invalid "
      "component type {}",
      componentTypeString(compType)
    );
    return;
  }

  const size_t N = static_cast<size_t>(sizeInVoxels.x) * static_cast<size_t>(sizeInVoxels.y)
                   * static_cast<size_t>(sizeInVoxels.z);

  switch (compType)
  {
  case ComponentType::UInt8:
  {
    const std::vector<uint8_t> castData(data, data + N);

    return updateSegTexture(
      segUid, compType, startOffsetVoxel, sizeInVoxels, static_cast<const void*>(castData.data())
    );
  }
  case ComponentType::UInt16:
  {
    const std::vector<uint16_t> castData(data, data + N);

    return updateSegTexture(
      segUid, compType, startOffsetVoxel, sizeInVoxels, static_cast<const void*>(castData.data())
    );
  }
  case ComponentType::UInt32:
  {
    const std::vector<uint32_t> castData(data, data + N);

    return updateSegTexture(
      segUid, compType, startOffsetVoxel, sizeInVoxels, static_cast<const void*>(castData.data())
    );
  }
  default:
    return;
  }
}

/// @todo Need to fix this to handle multicomponent images like
/// std::vector<uuids::uuid> createImageTextures( AppData& appData, uuid_range_t imageUids )
void Rendering::updateImageTexture(
  const uuids::uuid& imageUid,
  uint32_t comp,
  const ComponentType& compType,
  const glm::uvec3& startOffsetVoxel,
  const glm::uvec3& sizeInVoxels,
  const void* data
)
{
  // Load data into first mipmap level
  static constexpr GLint sk_mipmapLevel = 0;

  auto it = m_appData.renderData().m_imageTextures.find(imageUid);
  if (std::end(m_appData.renderData().m_imageTextures) == it)
  {
    spdlog::error("Cannot update image {}: texture not found.", imageUid);
    return;
  }

  std::vector<GLTexture>& T = it->second;

  if (comp >= T.size())
  {
    spdlog::error("Cannot update invalid component {} of image {}", comp, imageUid);
    return;
  }

  const auto* img = m_appData.image(imageUid);

  if (!img)
  {
    spdlog::warn("Segmentation {} is invalid", imageUid);
    return;
  }

  T.at(comp).setSubData(
    sk_mipmapLevel,
    startOffsetVoxel,
    sizeInVoxels,
    GLTexture::getBufferPixelRedFormat(compType),
    GLTexture::getBufferPixelDataType(compType),
    data
  );
}

Rendering::CurrentImages Rendering::getImageAndSegUidsForMetricShaders(
  const std::list<uuids::uuid>& metricImageUids
) const
{
  CurrentImages I;

  for (const auto& imageUid : metricImageUids)
  {
    if (I.size() >= NUM_METRIC_IMAGES)
      break;

    if (std::end(m_appData.renderData().m_imageTextures) != m_appData.renderData().m_imageTextures.find(imageUid))
    {
      ImgSegPair imgSegPair;

      // The texture for this image exists
      imgSegPair.first = imageUid;

      // Find the segmentation that belongs to this image
      if (const auto segUid = m_appData.imageToActiveSegUid(imageUid))
      {
        if (std::end(m_appData.renderData().m_segTextures) != m_appData.renderData().m_segTextures.find(*segUid))
        {
          // The texture for this seg exists
          imgSegPair.second = *segUid;
        }
      }

      I.push_back(imgSegPair);
    }
  }

  // Always return at least two elements.
  while (I.size() < Rendering::NUM_METRIC_IMAGES)
  {
    I.push_back(ImgSegPair());
  }

  return I;
}

Rendering::CurrentImages Rendering::getImageAndSegUidsForImageShaders(
  const std::list<uuids::uuid>& imageUids
) const
{
  CurrentImages I;

  for (const auto& imageUid : imageUids)
  {
    if (std::end(m_appData.renderData().m_imageTextures) != m_appData.renderData().m_imageTextures.find(imageUid))
    {
      std::pair<std::optional<uuids::uuid>, std::optional<uuids::uuid> > p;

      // The texture for this image exists
      p.first = imageUid;

      // Find the segmentation that belongs to this image
      if (const auto segUid = m_appData.imageToActiveSegUid(imageUid))
      {
        if (std::end(m_appData.renderData().m_segTextures) != m_appData.renderData().m_segTextures.find(*segUid))
        {
          // The texture for this segmentation exists
          p.second = *segUid;
        }
      }

      I.emplace_back(std::move(p));
    }
  }

  return I;
}

void Rendering::updateImageInterpolation(const uuids::uuid& imageUid)
{
  const auto* image = m_appData.image(imageUid);
  if (!image)
  {
    spdlog::warn("Image {} is invalid", imageUid);
    return;
  }

  if (!image->settings().displayImageAsColor())
  {
    // Modify the active component
    const uint32_t activeComp = image->settings().activeComponent();

    GLTexture& texture = m_appData.renderData().m_imageTextures.at(imageUid).at(activeComp);

    tex::MinificationFilter minFilter;
    tex::MagnificationFilter maxFilter;

    switch (image->settings().interpolationMode(activeComp))
    {
    case InterpolationMode::NearestNeighbor:
    {
      minFilter = tex::MinificationFilter::Nearest;
      maxFilter = tex::MagnificationFilter::Nearest;
      break;
    }
    case InterpolationMode::Trilinear:
    case InterpolationMode::Tricubic:
    {
      minFilter = tex::MinificationFilter::Linear;
      maxFilter = tex::MagnificationFilter::Linear;
      break;
    }
    }

    texture.setMinificationFilter(minFilter);
    texture.setMagnificationFilter(maxFilter);

    spdlog::debug("Set image interpolation mode for image {}", imageUid);
  }
  else
  {
    // Modify all components for color images
    for (uint32_t i = 0; i < image->header().numComponentsPerPixel(); ++i)
    {
      GLTexture& texture = m_appData.renderData().m_imageTextures.at(imageUid).at(i);

      tex::MinificationFilter minFilter;
      tex::MagnificationFilter maxFilter;

      switch (image->settings().colorInterpolationMode())
      {
      case InterpolationMode::NearestNeighbor:
      {
        minFilter = tex::MinificationFilter::Nearest;
        maxFilter = tex::MagnificationFilter::Nearest;
        break;
      }
      case InterpolationMode::Trilinear:
      case InterpolationMode::Tricubic:
      {
        minFilter = tex::MinificationFilter::Linear;
        maxFilter = tex::MagnificationFilter::Linear;
        break;
      }
      }

      texture.setMinificationFilter(minFilter);
      texture.setMagnificationFilter(maxFilter);

      spdlog::debug("Set image interpolation mode for color image {}", imageUid);
    }
  }
}

void Rendering::updateImageColorMapInterpolation(std::size_t cmapIndex)
{
  const auto cmapUid = m_appData.imageColorMapUid(cmapIndex);

  if (!cmapUid)
  {
    spdlog::warn("Image color map index {} is invalid", cmapIndex);
    return;
  }

  const auto* map = m_appData.imageColorMap(*cmapUid);

  if (!map)
  {
    spdlog::warn("Image color map {} is invalid", *cmapUid);
    return;
  }

  ImageColorMap* cmap = m_appData.imageColorMap(*cmapUid);

  if (!cmap)
  {
    spdlog::warn("Image color map {} is null", *cmapUid);
    return;
  }

  GLTexture& texture = m_appData.renderData().m_colormapTextures.at(*cmapUid);

  tex::MinificationFilter minFilter;
  tex::MagnificationFilter maxFilter;

  switch (cmap->interpolationMode())
  {
  case ImageColorMap::InterpolationMode::Nearest:
  {
    minFilter = tex::MinificationFilter::Nearest;
    maxFilter = tex::MagnificationFilter::Nearest;
    break;
  }
  case ImageColorMap::InterpolationMode::Linear:
  {
    minFilter = tex::MinificationFilter::Linear;
    maxFilter = tex::MagnificationFilter::Linear;
    break;
  }
  }

  texture.setMinificationFilter(minFilter);
  texture.setMagnificationFilter(maxFilter);

  spdlog::debug("Set interpolation mode for image color map {}", *cmapUid);
}

void Rendering::updateLabelColorTableTexture(size_t tableIndex)
{
  spdlog::trace("Begin updating texture for 1D label color map at index {}", tableIndex);

  if (tableIndex >= m_appData.numLabelTables())
  {
    spdlog::error("Label color table at index {} does not exist", tableIndex);
    return;
  }

  const auto tableUid = m_appData.labelTableUid(tableIndex);
  if (!tableUid)
  {
    spdlog::error("Label table index {} is invalid", tableIndex);
    return;
  }

  const auto* table = m_appData.labelTable(*tableUid);
  if (!table)
  {
    spdlog::error("Label table {} is invalid", *tableUid);
    return;
  }

  auto it = m_appData.renderData().m_labelBufferTextures.find(*tableUid);
  if (std::end(m_appData.renderData().m_labelBufferTextures) == it)
  {
    spdlog::error("Buffer texture for label color table {} is invalid", *tableUid);
    return;
  }

  it->second.write(0, table->numColorBytes_RGBA_U8(), table->colorData_RGBA_nonpremult_U8());

  spdlog::trace("Done updating buffer texture for label color table {}", *tableUid);
}

void Rendering::render()
{
  // Set up OpenGL state, because it changes after NanoVG calls in the render of the prior frame
  setupOpenGlState();

  // Set the OpenGL viewport in device units:
  const glm::ivec4 deviceViewport = m_appData.windowData().viewport().getDeviceAsVec4();
  glViewport(deviceViewport[0], deviceViewport[1], deviceViewport[2], deviceViewport[3]);

  glClearColor(
    m_appData.renderData().m_2dBackgroundColor.r,
    m_appData.renderData().m_2dBackgroundColor.g,
    m_appData.renderData().m_2dBackgroundColor.b,
    1.0f
  );

  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

  renderImageData();
  //    renderOverlays();
  renderVectorOverlays();
}

void Rendering::updateImageUniforms(uuid_range_t imageUids)
{
  for (const auto& imageUid : imageUids)
  {
    updateImageUniforms(imageUid);
  }
}

void Rendering::updateImageUniforms(const uuids::uuid& imageUid)
{
  auto it = m_appData.renderData().m_uniforms.find(imageUid);

  if (std::end(m_appData.renderData().m_uniforms) == it)
  {
    spdlog::debug("Adding rendering uniforms for image {}", imageUid);

    m_appData.renderData().m_uniforms.insert(std::make_pair(imageUid, RenderData::ImageUniforms()));
  }

  RenderData::ImageUniforms& uniforms = m_appData.renderData().m_uniforms[imageUid];

  Image* img = m_appData.image(imageUid);

  if (!img)
  {
    uniforms.imgOpacity = 0.0f;
    uniforms.segOpacity = 0.0f;
    uniforms.showEdges = false;
    spdlog::error(
      "Image {} is null on updating its uniforms; setting default uniform values", imageUid
    );
    return;
  }

  const auto& imgSettings = img->settings();

  uniforms.cmapQuantLevels = imgSettings.colorMapContinuous()
                               ? 0
                               : imgSettings.colorMapQuantizationLevels();
  uniforms.hsvModFactors = imgSettings.colorMapHsvModFactors();

  if (const auto cmapUid = m_appData.imageColorMapUid(imgSettings.colorMapIndex()))
  {
    if (const ImageColorMap* map = m_appData.imageColorMap(*cmapUid))
    {
      uniforms.cmapSlopeIntercept = map->slopeIntercept(imgSettings.isColorMapInverted());

      // If the color map has nearest-neighbor interpolation, then do NOT quantize:
      if (ImageColorMap::InterpolationMode::Nearest == map->interpolationMode())
      {
        uniforms.cmapQuantLevels = 0;
      }
    }
    else
    {
      spdlog::error("Null image color map {} on updating uniforms for image {}", *cmapUid, imageUid);
    }
  }
  else
  {
    spdlog::error(
      "Invalid image color map at index {} on updating uniforms for image {}",
      imgSettings.colorMapIndex(),
      imageUid
    );
  }

  glm::mat4 imgTexture_T_world(1.0f);

  /// @note This has been removed, since the reference image transformations are now always locked!
  /// In order for this commented-out code to work, the uniforms for all images must be updated
  /// when the reference image transformation is updated. Otherwise, the uniforms of the
  /// other images will be out of sync.

  /*
    if ( m_appData.refImageUid() &&
         imgSettings.isLockedToReference() &&
         imageUid != m_appData.refImageUid() )
    {
        if ( const Image* refImg = m_appData.refImage() )
        {
            /// @note \c img->transformations().subject_T_worldDef() is the contactenation of
            /// both the initial loaded affine tx and the manually applied affine tx for the given image.
            /// It used here, since when the given image is locked to the reference image, the tx from
            /// World space to image Subject space (\c img->transformations().subject_T_worldDef() )
            /// is repurposed as the tx from reference image Subject space to image Subject space.
            const glm::mat4& imgSubject_T_refSubject = img->transformations().subject_T_worldDef();

            imgTexture_T_world =
                    img->transformations().texture_T_subject() *
                    imgSubject_T_refSubject *
                    refImg->transformations().subject_T_worldDef();
        }
        else
        {
            spdlog::error( "Null reference image" );
        }
    }
    else
    */
  {
    imgTexture_T_world = img->transformations().texture_T_worldDef();
  }

  uniforms.imgTexture_T_world = imgTexture_T_world;
  uniforms.world_T_imgTexture = glm::inverse(imgTexture_T_world);

  if (imgSettings.displayImageAsColor() && (3 == imgSettings.numComponents() || 4 == imgSettings.numComponents()))
  {
    for (uint32_t i = 0; i < imgSettings.numComponents(); ++i)
    {
      uniforms.slopeInterceptRgba_normalized_T_texture[i]
        = imgSettings.slopeInterceptVec2_normalized_T_texture(i);

      uniforms.thresholdsRgba[i] = glm::vec2{
        static_cast<float>(imgSettings.mapNativeIntensityToTexture(imgSettings.thresholds(i).first)),
        static_cast<float>(imgSettings.mapNativeIntensityToTexture(imgSettings.thresholds(i).second))
      };

      uniforms.minMaxRgba[i] = glm::vec2{
        static_cast<float>(
          imgSettings.mapNativeIntensityToTexture(imgSettings.minMaxImageRange(i).first)
        ),
        static_cast<float>(
          imgSettings.mapNativeIntensityToTexture(imgSettings.minMaxImageRange(i).second)
        )
      };

      uniforms.imgOpacityRgba[i] = static_cast<float>(
        ((imgSettings.globalVisibility() && imgSettings.visibility(i)) ? 1.0 : 0.0)
        * imgSettings.globalOpacity() * imgSettings.opacity(i)
      );
    }

    if (3 == imgSettings.numComponents())
    {
      // These two will be ignored for RGB images:
      uniforms.slopeInterceptRgba_normalized_T_texture[3] = glm::vec2{1.0f, 0.0f};
      uniforms.thresholdsRgba[3] = glm::vec2{0.0f, 1.0f};
      uniforms.minMaxRgba[3] = glm::vec2{0.0f, 1.0f};

      uniforms.imgOpacityRgba[3] = static_cast<float>(
        (imgSettings.globalVisibility() ? 1.0 : 0.0) * imgSettings.globalOpacity()
      );
    }
  }
  else
  {
    uniforms.slopeIntercept_normalized_T_texture = imgSettings
                                                     .slopeInterceptVec2_normalized_T_texture();
  }

  uniforms.slope_native_T_texture = imgSettings.slope_native_T_texture();
  uniforms.largestSlopeIntercept = imgSettings.largestSlopeInterceptTextureVec2();

  const glm::vec3 dims = glm::vec3{img->header().pixelDimensions()};

  uniforms.textureGradientStep = glm::mat3{
    glm::vec3{1.0f / dims[0], 0.0f, 0.0f},
    glm::vec3{0.0f, 1.0f / dims[1], 0.0f},
    glm::vec3{0.0f, 0.0f, 1.0f / dims[2]}
  };

  uniforms.voxelSpacing = img->header().spacing();

  // Map the native thresholds to OpenGL texture values:
  uniforms.thresholds = glm::vec2{
    static_cast<float>(imgSettings.mapNativeIntensityToTexture(imgSettings.thresholds().first)),
    static_cast<float>(imgSettings.mapNativeIntensityToTexture(imgSettings.thresholds().second))
  };

  // Map the native image values to OpenGL texture values:
  uniforms.minMax = glm::vec2{
    static_cast<float>(imgSettings.mapNativeIntensityToTexture(imgSettings.minMaxImageRange().first)
    ),
    static_cast<float>(imgSettings.mapNativeIntensityToTexture(imgSettings.minMaxImageRange().second
    ))
  };

  uniforms.imgOpacity
    = (static_cast<float>(imgSettings.globalVisibility() && imgSettings.visibility()) ? 1.0 : 0.0)
      * imgSettings.opacity()
      * ((imgSettings.numComponents() > 0) ? imgSettings.globalOpacity() : 1.0f);

  // Edges
  uniforms.showEdges = imgSettings.showEdges();
  uniforms.thresholdEdges = imgSettings.thresholdEdges();
  uniforms.edgeMagnitude = static_cast<float>(imgSettings.edgeMagnitude());
  uniforms.useFreiChen = imgSettings.useFreiChen();
  //    uniforms.windowedEdges = imgSettings.windowedEdges();
  uniforms.overlayEdges = imgSettings.overlayEdges();
  uniforms.colormapEdges = imgSettings.colormapEdges();
  uniforms.edgeColor = static_cast<float>(imgSettings.edgeOpacity())
                       * glm::vec4{imgSettings.edgeColor(), 1.0f};

  // The segmentation linked to this image:
  const auto& segUid = m_appData.imageToActiveSegUid(imageUid);

  if (!segUid)
  {
    // The image has no segmentation
    uniforms.segOpacity = 0.0f;
    return;
  }

  Image* seg = m_appData.seg(*segUid);

  if (!seg)
  {
    spdlog::error("Segmentation {} is null on updating uniforms for image {}", *segUid, imageUid);
    return;
  }

  // The texture_T_world transformation of the segmentation uses the manual affine component
  // (subject_T_worldDef) of the image.
  uniforms.segTexture_T_world = seg->transformations().texture_T_subject()
                                * img->transformations().subject_T_worldDef();

  uniforms.segVoxel_T_world = seg->transformations().pixel_T_subject()
                              * img->transformations().subject_T_worldDef();

  // Both the image and segmenation must have visibility true for the segmentation to be shown
  if (imgSettings.numComponents() > 1)
  {
    uniforms.segOpacity = static_cast<float>(
      ((seg->settings().visibility() && imgSettings.globalVisibility()) ? 1.0 : 0.0)
      * seg->settings().opacity()
    );
  }
  else
  {
    uniforms.segOpacity = static_cast<float>(
      ((seg->settings().visibility() && imgSettings.visibility(0) && imgSettings.globalVisibility())
         ? 1.0
         : 0.0)
      * seg->settings().opacity()
    );
  }
}

void Rendering::updateMetricUniforms()
{
  auto update = [this](RenderData::MetricParams& params, const char* name)
  {
    if (const auto cmapUid = m_appData.imageColorMapUid(params.m_colorMapIndex))
    {
      if (const auto* map = m_appData.imageColorMap(*cmapUid))
      {
        params.m_cmapSlopeIntercept = map->slopeIntercept(params.m_invertCmap);
      }
      else
      {
        spdlog::error("Null image color map {} on updating uniforms for {} metric", *cmapUid, name);
      }
    }
    else
    {
      spdlog::error(
        "Invalid image color map at index {} on updating uniforms for {} metric",
        params.m_colorMapIndex,
        name
      );
    }
  };

  update(m_appData.renderData().m_squaredDifferenceParams, "Difference");
  update(m_appData.renderData().m_crossCorrelationParams, "Cross-Correlation");
  update(m_appData.renderData().m_jointHistogramParams, "Joint Histogram");
}

std::list<std::reference_wrapper<GLTexture> > Rendering::bindImageTextures(const ImgSegPair& p)
{
  std::list<std::reference_wrapper<GLTexture> > textures;

  auto& R = m_appData.renderData();

  const auto& imageUid = p.first;
  const auto& segUid = p.second;

  const Image* image = (imageUid ? m_appData.image(*imageUid) : nullptr);

  const auto cmapUid
    = (image ? m_appData.imageColorMapUid(image->settings().colorMapIndex()) : std::nullopt);

  if (image)
  {
    const ImageSettings& imgSettings = image->settings();

    // Bind the active component of the image
    const uint32_t activeComp = imgSettings.activeComponent();

    const bool useDistMap = imgSettings.useDistanceMapForRaycasting();

    // Uncomment this to render the image's distance map instead:
    // GLTexture& imgTex = R.m_distanceMapTextures.at( *imageUid ).at( activeComp );

    if (imgSettings.displayImageAsColor())
    {
      if (3 == imgSettings.numComponents() || 4 == imgSettings.numComponents())
      {
        GLTexture& imgRedTex = R.m_imageTextures.at(*imageUid).at(0);
        GLTexture& imgGreenTex = R.m_imageTextures.at(*imageUid).at(1);
        GLTexture& imgBlueTex = R.m_imageTextures.at(*imageUid).at(2);

        // If the image has no 4th (alpha) component, or if alpha is ignored,
        // then bind the white texture as alpha.
        GLTexture& imgAlphaTex = (4 == imgSettings.numComponents())
                                   ? R.m_imageTextures.at(*imageUid).at(3)
                                   : R.m_blankImageBlackTransparentTexture;

        imgRedTex.bind(msk_imgRgbaTexSamplers.indices[0]);
        imgGreenTex.bind(msk_imgRgbaTexSamplers.indices[1]);
        imgBlueTex.bind(msk_imgRgbaTexSamplers.indices[2]);
        imgAlphaTex.bind(msk_imgRgbaTexSamplers.indices[3]);

        textures.push_back(imgRedTex);
        textures.push_back(imgGreenTex);
        textures.push_back(imgBlueTex);
        textures.push_back(imgAlphaTex);
      }
      else
      {
        spdlog::error(
          "Textures for color image {} cannot be bound: it has {} components",
          *imageUid,
          imgSettings.numComponents()
        );
      }
    }
    else
    {
      GLTexture& imgTex = R.m_imageTextures.at(*imageUid).at(activeComp);
      imgTex.bind(msk_imgTexSampler.index);
      textures.push_back(imgTex);
    }

    static bool alreadyShowedWarning = false;

    if (useDistMap)
    {
      const auto& distMaps = m_appData.distanceMaps(*imageUid, activeComp);

      if (distMaps.empty())
      {
        if (!alreadyShowedWarning)
        {
          spdlog::warn("No distance map for component {} of image {}", activeComp, *imageUid);
          alreadyShowedWarning = true;

          // Disable use of distance map for this image:
          if (Image* image2 = (imageUid ? m_appData.image(*imageUid) : nullptr))
          {
            image2->settings().setUseDistanceMapForRaycasting(false);
          }
        }
      }
    }

    bool foundMap = false;

    if (useDistMap)
    {
      auto it = R.m_distanceMapTextures.find(*imageUid);
      if (std::end(R.m_distanceMapTextures) != it)
      {
        auto it2 = it->second.find(activeComp);
        if (std::end(it->second) != it2)
        {
          foundMap = true;
          GLTexture& distTex = it2->second;
          distTex.bind(msk_jumpTexSampler.index);
          textures.push_back(distTex);
        }
      }
    }

    if (!useDistMap || !foundMap)
    {
      // Bind blank (zero) distance map:
      GLTexture& distTex = R.m_blankDistMapTexture;
      distTex.bind(msk_jumpTexSampler.index);
      textures.push_back(distTex);
    }
  }
  else
  {
    // No image, so bind the blank one:
    GLTexture& imgTex = R.m_blankImageBlackTransparentTexture;
    imgTex.bind(msk_imgTexSampler.index);
    textures.push_back(imgTex);

    // Also bind blank distance map:
    GLTexture& distTex = R.m_blankDistMapTexture;
    distTex.bind(msk_jumpTexSampler.index);
    textures.push_back(distTex);
  }

  if (segUid)
  {
    // Uncomment this to render the image's distance map instead:
    // GLTexture& segTex = R.m_distanceMapTextures.at( *imageUid ).at( 0 );
    GLTexture& segTex = R.m_segTextures.at(*segUid);
    segTex.bind(msk_segTexSampler.index);
    textures.push_back(segTex);
  }
  else
  {
    // No segmentation, so bind the blank one:
    GLTexture& segTex = R.m_blankSegTexture;
    segTex.bind(msk_segTexSampler.index);
    textures.push_back(segTex);
  }

  if (cmapUid)
  {
    GLTexture& cmapTex = R.m_colormapTextures.at(*cmapUid);
    cmapTex.bind(msk_imgCmapTexSampler.index);
    textures.push_back(cmapTex);
  }
  else
  {
    // No colormap, so bind the first available one:
    auto it = std::begin(R.m_colormapTextures);
    GLTexture& cmapTex = it->second;
    cmapTex.bind(msk_imgCmapTexSampler.index);
    textures.push_back(cmapTex);
  }

  return textures;
}

void Rendering::unbindTextures(const std::list<std::reference_wrapper<GLTexture> >& textures)
{
  for (auto& T : textures)
  {
    T.get().unbind();
  }
}

std::list<std::reference_wrapper<GLBufferTexture> > Rendering::bindBufferTextures(
  const CurrentImages& I
)
{
  std::list<std::reference_wrapper<GLBufferTexture> > bufferTextures;

  auto& R = m_appData.renderData();

  for (const auto& imgSegPair : I)
  {
    const auto& segUid = imgSegPair.second;
    if (!segUid)
      continue;

    const Image* seg = (segUid ? m_appData.seg(*segUid) : nullptr);
    const auto tableUid
      = (seg ? m_appData.labelTableUid(seg->settings().labelTableIndex()) : std::nullopt);

    if (tableUid)
    {
      GLBufferTexture& tblTex = R.m_labelBufferTextures.at(*tableUid);
      tblTex.bind(msk_labelTableTexSampler.index);
      tblTex.attachBufferToTexture(msk_labelTableTexSampler.index);
      bufferTextures.push_back(tblTex);
    }
    else
    {
      // No label table, so bind the first available one:
      auto it = std::begin(R.m_labelBufferTextures);
      GLBufferTexture& tblTex = it->second;
      tblTex.bind(msk_labelTableTexSampler.index);
      tblTex.attachBufferToTexture(msk_labelTableTexSampler.index);
      bufferTextures.push_back(tblTex);
    }
  }

  return bufferTextures;
}

void Rendering::unbindBufferTextures(
  const std::list<std::reference_wrapper<GLBufferTexture> >& textures
)
{
  for (auto& T : textures)
  {
    T.get().unbind();
  }
}

std::list<std::reference_wrapper<GLTexture> > Rendering::bindMetricImageTextures(
  const CurrentImages& I, const camera::ViewRenderMode& metricType
)
{
  std::list<std::reference_wrapper<GLTexture> > textures;

  auto& R = m_appData.renderData();

  bool usesMetricColormap = false;
  size_t metricCmapIndex = 0;

  switch (metricType)
  {
  case camera::ViewRenderMode::Difference:
  {
    usesMetricColormap = true;
    metricCmapIndex = R.m_squaredDifferenceParams.m_colorMapIndex;
    break;
  }
  case camera::ViewRenderMode::CrossCorrelation:
  {
    usesMetricColormap = true;
    metricCmapIndex = R.m_crossCorrelationParams.m_colorMapIndex;
    break;
  }
  case camera::ViewRenderMode::JointHistogram:
  {
    usesMetricColormap = true;
    metricCmapIndex = R.m_jointHistogramParams.m_colorMapIndex;
    break;
  }
  case camera::ViewRenderMode::Overlay:
  {
    usesMetricColormap = false;
    break;
  }
  case camera::ViewRenderMode::Disabled:
  {
    return textures;
  }
  default:
  {
    spdlog::error("Invalid metric shader type {}", camera::typeString(metricType));
    return textures;
  }
  }

  if (usesMetricColormap)
  {
    const auto cmapUid = m_appData.imageColorMapUid(metricCmapIndex);

    if (cmapUid)
    {
      GLTexture& T = R.m_colormapTextures.at(*cmapUid);
      T.bind(msk_metricCmapTexSampler.index);
      textures.push_back(T);
    }
    else
    {
      auto it = std::begin(R.m_colormapTextures);
      GLTexture& T = it->second;
      T.bind(msk_metricCmapTexSampler.index);
      textures.push_back(T);
    }
  }

  size_t i = 0;

  for (const auto& imgSegPair : I)
  {
    const auto& imageUid = imgSegPair.first;
    const auto& segUid = imgSegPair.second;

    const Image* image = (imageUid ? m_appData.image(*imageUid) : nullptr);

    if (image)
    {
      // Bind the active component
      const uint32_t activeComp = image->settings().activeComponent();
      GLTexture& T = R.m_imageTextures.at(*imageUid).at(activeComp);
      T.bind(msk_imgTexSamplers.indices[i]);
      textures.push_back(T);
    }
    else
    {
      GLTexture& T = R.m_blankImageBlackTransparentTexture;
      T.bind(msk_imgTexSamplers.indices[i]);
      textures.push_back(T);
    }

    if (segUid)
    {
      GLTexture& T = R.m_segTextures.at(*segUid);
      T.bind(msk_segTexSamplers.indices[i]);
      textures.push_back(T);
    }
    else
    {
      GLTexture& T = R.m_blankSegTexture;
      T.bind(msk_segTexSamplers.indices[i]);
      textures.push_back(T);
    }

    ++i;
  }

  return textures;
}

void Rendering::renderOneImage(
  const View& view,
  const FrameBounds& miewportViewBounds,
  const glm::vec3& worldOffsetXhairs,
  GLShaderProgram& program,
  const CurrentImages& I,
  bool showEdges
)
{
  auto getImage = [this](const std::optional<uuids::uuid>& imageUid) -> const Image*
  { return (imageUid ? m_appData.image(*imageUid) : nullptr); };

  auto& renderData = m_appData.renderData();

  drawImageQuad(
    program,
    view.renderMode(),
    renderData.m_quad,
    view,
    m_appData.windowData().viewport(),
    worldOffsetXhairs,
    renderData.m_flashlightRadius,
    renderData.m_flashlightOverlays,
    renderData.m_intensityProjectionSlabThickness,
    renderData.m_doMaxExtentIntensityProjection,
    renderData.m_xrayIntensityWindow,
    renderData.m_xrayIntensityLevel,
    I,
    getImage,
    showEdges,
    renderData.m_segOutlineStyle,
    renderData.m_segInteriorOpacity,
    renderData.m_segInterpolation,
    renderData.m_segInterpCutoff
  );

  if (!renderData.m_globalLandmarkParams.renderOnTopOfAllImagePlanes)
  {
    drawLandmarks(m_nvg, miewportViewBounds, worldOffsetXhairs, m_appData, view, I);
    setupOpenGlState();
  }

  if (!renderData.m_globalAnnotationParams.renderOnTopOfAllImagePlanes)
  {
    drawAnnotations(m_nvg, miewportViewBounds, worldOffsetXhairs, m_appData, view, I);
    setupOpenGlState();
  }

  drawImageViewIntersections(
    m_nvg,
    miewportViewBounds,
    worldOffsetXhairs,
    m_appData,
    view,
    I,
    renderData.m_globalSliceIntersectionParams.renderInactiveImageViewIntersections
  );

  setupOpenGlState();
}

void Rendering::volumeRenderOneImage(
  const View& view, GLShaderProgram& program, const CurrentImages& I
)
{
  auto getImage = [this](const std::optional<uuids::uuid>& imageUid) -> const Image*
  { return (imageUid ? m_appData.image(*imageUid) : nullptr); };

  drawRaycastQuad(program, m_appData.renderData().m_quad, view, I, getImage);

  setupOpenGlState();
}

void Rendering::renderAllImages(
  const View& view, const FrameBounds& miewportViewBounds, const glm::vec3& worldOffsetXhairs
)
{
  static std::list<std::reference_wrapper<GLTexture> > boundImageTextures;
  static std::list<std::reference_wrapper<GLTexture> > boundMetricTextures;
  static std::list<std::reference_wrapper<GLBufferTexture> > boundBufferTextures;

  static const RenderData::ImageUniforms sk_defaultImageUniforms;

  auto& renderData = m_appData.renderData();
  const bool modSegOpacity = renderData.m_modulateSegOpacityWithImageOpacity;

  const auto renderMode = view.renderMode();
  const auto& metricImages = view.metricImages();
  const auto& renderedImages = view.renderedImages();

  switch (getShaderGroup(renderMode))
  {
  case camera::ShaderGroup::Image:
  {
    CurrentImages I;

    int displayModeUniform = 0;

    if (camera::ViewRenderMode::Image == renderMode)
    {
      displayModeUniform = 0;
      I = getImageAndSegUidsForImageShaders(renderedImages);
    }
    else if (camera::ViewRenderMode::Checkerboard == renderMode)
    {
      displayModeUniform = 1;
      I = getImageAndSegUidsForMetricShaders(metricImages); // guaranteed size 2
    }
    else if (camera::ViewRenderMode::Quadrants == renderMode)
    {
      displayModeUniform = 2;
      I = getImageAndSegUidsForMetricShaders(metricImages);
    }
    else if (camera::ViewRenderMode::Flashlight == renderMode)
    {
      displayModeUniform = 3;
      I = getImageAndSegUidsForMetricShaders(metricImages);
    }

    bool isFixedImage = true; // true for the first image

    for (const auto& imgSegPair : I)
    {
      if (!imgSegPair.first)
      {
        isFixedImage = false;
        continue;
      }

      boundImageTextures = bindImageTextures(imgSegPair);
      boundBufferTextures = bindBufferTextures(std::vector<ImgSegPair>{imgSegPair});

      const auto& U = renderData.m_uniforms.at(*imgSegPair.first);

      const Image* img = m_appData.image(*imgSegPair.first);

      if (!img)
      {
        spdlog::error("Null image during render");
        return;
      }

      const bool doXray = (camera::IntensityProjectionMode::Xray == view.intensityProjectionMode());

      GLShaderProgram* P = nullptr;

      if (img->settings().displayImageAsColor())
      {
        P = &m_imageRgbaProgram;
      }
      else
      {
        if (U.showEdges)
        {
          P = &m_edgeProgram;
        }
        else
        {
          if (doXray)
          {
            P = &m_xrayProgram;
          }
          else
          {
            P = &m_imageProgram;
          }
        }
      }

      if (!P)
      {
        spdlog::error("Null program when rendering image {}", *imgSegPair.first);
        return;
      }

      P->use();

      if (!img->settings().displayImageAsColor())
      {
        // Greyscale image:

        P->setSamplerUniform("u_imgTex", msk_imgTexSampler.index);
        P->setSamplerUniform("u_segTex", msk_segTexSampler.index);
        P->setSamplerUniform("u_imgCmapTex", msk_imgCmapTexSampler.index);
        P->setSamplerUniform("u_segLabelCmapTex", msk_labelTableTexSampler.index);

        // P->setUniform( "u_useTricubicInterpolation",
        //     ( InterpolationMode::Tricubic == img->settings().interpolationMode() ) );

        P->setUniform("u_numSquares", static_cast<float>(renderData.m_numCheckerboardSquares));
        P->setUniform("u_imgTexture_T_world", U.imgTexture_T_world);
        P->setUniform("u_segTexture_T_world", U.segTexture_T_world);
        P->setUniform("u_segVoxel_T_world", U.segVoxel_T_world);

        if (!doXray)
        {
          P->setUniform("u_imgSlopeIntercept", U.slopeIntercept_normalized_T_texture);
          P->setUniform("u_imgCmapHsvModFactors", U.hsvModFactors);

          if (!U.showEdges)
          {
            updateIsosurfaceDataFor2d(m_appData, *imgSegPair.first);

            P->setUniform("u_isoValues", renderData.m_isosurfaceData.values);
            P->setUniform("u_isoOpacities", renderData.m_isosurfaceData.opacities);
            P->setUniform("u_isoColors", renderData.m_isosurfaceData.colors);
            P->setUniform("u_isoWidth", renderData.m_isosurfaceData.widthIn2d);
          }
        }
        else
        {
          P->setUniform("imgSlope_native_T_texture", U.slope_native_T_texture);

          P->setUniform("waterAttenCoeff", renderData.m_waterMassAttenCoeff);
          P->setUniform("airAttenCoeff", renderData.m_airMassAttenCoeff);
        }

        P->setUniform("u_imgCmapSlopeIntercept", U.cmapSlopeIntercept);
        P->setUniform("u_imgCmapQuantLevels", U.cmapQuantLevels);
        P->setUniform("u_imgThresholds", U.thresholds);
        P->setUniform("u_imgMinMax", U.minMax);

        P->setUniform("u_imgOpacity", U.imgOpacity);
        P->setUniform("u_segOpacity", U.segOpacity * (modSegOpacity ? U.imgOpacity : 1.0f));
        P->setUniform("u_masking", renderData.m_maskedImages);
        P->setUniform("u_quadrants", renderData.m_quadrants);
        P->setUniform("u_showFix", isFixedImage); // ignored if not checkerboard or quadrants
        P->setUniform("u_renderMode", displayModeUniform);

        if (U.showEdges)
        {
          P->setUniform("u_imgSlopeInterceptLargest", U.largestSlopeIntercept);
          P->setUniform("u_thresholdEdges", U.thresholdEdges);
          P->setUniform("u_edgeMagnitude", U.edgeMagnitude);
          //                     P.setUniform( "useFreiChen", U.useFreiChen );
          P->setUniform("u_overlayEdges", U.overlayEdges);
          P->setUniform("u_colormapEdges", U.colormapEdges);
          P->setUniform("u_edgeColor", U.edgeColor);
        }

        renderOneImage(
          view, miewportViewBounds, worldOffsetXhairs, *P, CurrentImages{imgSegPair}, U.showEdges
        );
      }
      else
      {
        // Color image:

        P->setSamplerUniform("u_imgTex", msk_imgRgbaTexSamplers);
        P->setSamplerUniform("u_segTex", msk_segTexSampler.index);
        P->setSamplerUniform("u_imgCmapTex", msk_imgCmapTexSampler.index);
        P->setSamplerUniform("u_segLabelCmapTex", msk_labelTableTexSampler.index);

        // P->setUniform( "u_useTricubicInterpolation", ( InterpolationMode::Tricubic == img->settings().colorInterpolationMode() ) );

        P->setUniform("u_numSquares", static_cast<float>(renderData.m_numCheckerboardSquares));
        P->setUniform("u_imgTexture_T_world", U.imgTexture_T_world);
        P->setUniform("u_segTexture_T_world", U.segTexture_T_world);
        P->setUniform("u_segVoxel_T_world", U.segVoxel_T_world);

        P->setUniform("u_imgSlopeIntercept", U.slopeInterceptRgba_normalized_T_texture);
        P->setUniform("u_imgThresholds", U.thresholdsRgba);
        P->setUniform("u_imgMinMax", U.minMaxRgba);

        const bool forceAlphaToOne
          = (img->settings().ignoreAlpha() || 3 == img->header().numComponentsPerPixel());

        P->setUniform("u_alphaIsOne", forceAlphaToOne);

        P->setUniform("u_imgOpacity", U.imgOpacityRgba);
        P->setUniform("u_segOpacity", U.segOpacity * (modSegOpacity ? U.imgOpacityRgba[3] : 1.0f));
        P->setUniform("u_masking", renderData.m_maskedImages);
        P->setUniform("u_quadrants", renderData.m_quadrants);
        P->setUniform("u_showFix", isFixedImage); // ignored if not checkerboard or quadrants
        P->setUniform("renderMode", displayModeUniform);

        renderOneImage(
          view, miewportViewBounds, worldOffsetXhairs, *P, CurrentImages{imgSegPair}, U.showEdges
        );
      }

      P->stopUse();

      unbindTextures(boundImageTextures);
      unbindBufferTextures(boundBufferTextures);

      isFixedImage = false;
    }

    break;
  }

  case camera::ShaderGroup::Metric:
  {
    // This function guarantees that I has size at least 2:
    const CurrentImages I = getImageAndSegUidsForMetricShaders(metricImages);

    const auto& U0 = (I.size() >= 1 && I[0].first) ? renderData.m_uniforms.at(*I[0].first)
                                                   : sk_defaultImageUniforms;
    const auto& U1 = (I.size() >= 2 && I[1].first) ? renderData.m_uniforms.at(*I[1].first)
                                                   : sk_defaultImageUniforms;

    boundMetricTextures = bindMetricImageTextures(I, renderMode);
    boundBufferTextures = bindBufferTextures(I);

    if (camera::ViewRenderMode::Difference == renderMode)
    {
      const auto& metricParams = renderData.m_squaredDifferenceParams;
      GLShaderProgram& P = m_differenceProgram;

      P.use();
      {
        P.setSamplerUniform("u_imgTex", msk_imgTexSamplers);
        P.setSamplerUniform("u_segTex", msk_segTexSamplers);
        P.setSamplerUniform("u_segLabelCmapTex", msk_labelTableTexSamplers);
        P.setSamplerUniform("u_metricCmapTex", msk_metricCmapTexSampler.index);

        P.setUniform(
          "u_imgTexture_T_world",
          std::vector<glm::mat4>{U0.imgTexture_T_world, U1.imgTexture_T_world}
        );
        P.setUniform(
          "u_segTexture_T_world",
          std::vector<glm::mat4>{U0.segTexture_T_world, U1.segTexture_T_world}
        );
        P.setUniform("img1Tex_T_img0Tex", U1.imgTexture_T_world * glm::inverse(U0.imgTexture_T_world));

        P.setUniform(
          "u_imgSlopeIntercept",
          std::vector<glm::vec2>{U0.largestSlopeIntercept, U1.largestSlopeIntercept}
        );
        P.setUniform("u_segOpacity", std::vector<float>{U0.segOpacity, U1.segOpacity});

        P.setUniform("u_metricCmapSlopeIntercept", metricParams.m_cmapSlopeIntercept);
        P.setUniform("u_metricSlopeIntercept", metricParams.m_slopeIntercept);
        P.setUniform("u_metricMasking", metricParams.m_doMasking);

        P.setUniform("u_useSquare", renderData.m_useSquare);

        renderOneImage(view, miewportViewBounds, worldOffsetXhairs, P, I, false);
      }
      P.stopUse();
    }
    else if (camera::ViewRenderMode::CrossCorrelation == renderMode)
    {
      const auto& metricParams = renderData.m_crossCorrelationParams;
      GLShaderProgram& P = m_crossCorrelationProgram;

      P.use();
      {
        P.setSamplerUniform("u_imgTex", msk_imgTexSamplers);
        P.setSamplerUniform("u_segTex", msk_segTexSamplers);
        P.setSamplerUniform("u_segLabelCmapTex", msk_labelTableTexSamplers);
        P.setSamplerUniform("u_metricCmapTex", msk_metricCmapTexSampler.index);

        P.setUniform(
          "u_imgTexture_T_world",
          std::vector<glm::mat4>{U0.imgTexture_T_world, U1.imgTexture_T_world}
        );
        P.setUniform(
          "u_segTexture_T_world",
          std::vector<glm::mat4>{U0.segTexture_T_world, U1.segTexture_T_world}
        );
        P.setUniform("u_segOpacity", std::vector<float>{U0.segOpacity, U1.segOpacity});

        P.setUniform("u_metricCmapSlopeIntercept", metricParams.m_cmapSlopeIntercept);
        P.setUniform("u_metricSlopeIntercept", metricParams.m_slopeIntercept);
        P.setUniform("u_metricMasking", metricParams.m_doMasking);

        P.setUniform(
          "u_texture1_T_texture0", U1.imgTexture_T_world * glm::inverse(U0.imgTexture_T_world)
        );

        renderOneImage(view, miewportViewBounds, worldOffsetXhairs, P, I, false);
      }
      P.stopUse();
    }
    else if (camera::ViewRenderMode::Overlay == renderMode)
    {
      GLShaderProgram& P = m_overlayProgram;

      P.use();
      {
        P.setSamplerUniform("u_imgTex", msk_imgTexSamplers);
        P.setSamplerUniform("u_segTex", msk_segTexSamplers);
        P.setSamplerUniform("u_segLabelCmapTex", msk_labelTableTexSamplers);

        P.setUniform(
          "u_imgTexture_T_world",
          std::vector<glm::mat4>{U0.imgTexture_T_world, U1.imgTexture_T_world}
        );
        P.setUniform(
          "u_segTexture_T_world",
          std::vector<glm::mat4>{U0.segTexture_T_world, U1.segTexture_T_world}
        );
        P.setUniform(
          "u_imgSlopeIntercept",
          std::vector<glm::vec2>{
            U0.slopeIntercept_normalized_T_texture, U1.slopeIntercept_normalized_T_texture
          }
        );
        P.setUniform("u_imgThresholds", std::vector<glm::vec2>{U0.thresholds, U1.thresholds});
        P.setUniform("u_imgMinMax", std::vector<glm::vec2>{U0.minMax, U1.minMax});
        P.setUniform("u_imgOpacity", std::vector<float>{U0.imgOpacity, U1.imgOpacity});

        P.setUniform(
          "u_segOpacity",
          std::vector<float>{
            U0.segOpacity * (modSegOpacity ? U0.imgOpacity : 1.0f),
            U1.segOpacity * (modSegOpacity ? U1.imgOpacity : 1.0f)
          }
        );

        P.setUniform("magentaCyan", renderData.m_overlayMagentaCyan);

        renderOneImage(view, miewportViewBounds, worldOffsetXhairs, P, I, false);
      }
      P.stopUse();
    }

    unbindTextures(boundMetricTextures);
    unbindBufferTextures(boundBufferTextures);

    break;
  }

  case camera::ShaderGroup::Volume:
  {
    const CurrentImages I = getImageAndSegUidsForImageShaders(renderedImages);

    if (I.empty())
    {
      return;
    }

    // Only volume render the first image:
    /// @todo Either 1) let use only select one image or
    /// 2) enable rendering more than one image
    const auto& imgSegPair = I.front();

    const Image* image = m_appData.image(*imgSegPair.first);

    if (!image)
    {
      spdlog::warn("Null image {} when raycasting", *imgSegPair.first);
      return;
    }

    const ImageSettings& settings = image->settings();

    if (!settings.isosurfacesVisible())
    {
      return; // Hide all surfaces
    }

    // Render surfaces of the active image component
    const uint32_t activeComp = image->settings().activeComponent();

    const auto isosurfaceUids = m_appData.isosurfaceUids(*imgSegPair.first, activeComp);

    if (isosurfaceUids.empty())
    {
      return;
    }

    updateIsosurfaceDataFor3d(m_appData, *imgSegPair.first);

    boundImageTextures = bindImageTextures(imgSegPair);
    boundBufferTextures = bindBufferTextures(std::vector<ImgSegPair>{imgSegPair});

    const auto& U = renderData.m_uniforms.at(*imgSegPair.first);

    GLShaderProgram& P = m_raycastIsoSurfaceProgram;

    P.use();
    {
      P.setSamplerUniform("u_imgTex", msk_imgTexSampler.index);
      P.setSamplerUniform("u_segTex", msk_segTexSampler.index);
      P.setSamplerUniform("u_jumpTex", msk_jumpTexSampler.index);

      /// @todo Put a lot of these into the uniform settings...

      P.setUniform("u_imgTexture_T_world", U.imgTexture_T_world);
      P.setUniform("world_T_imgTexture", U.world_T_imgTexture);

      // The camera is positioned at the crosshairs:
      P.setUniform("worldEyePos", worldOffsetXhairs);

      //            P.setUniform( "voxelSpacing", U.voxelSpacing );
      P.setUniform("texGrads", U.textureGradientStep);

      P.setUniform("u_isoValues", renderData.m_isosurfaceData.values);
      P.setUniform("u_isoOpacities", renderData.m_isosurfaceData.opacities);
      P.setUniform("isoEdges", renderData.m_isosurfaceData.edgeStrengths);

      P.setUniform("lightAmbient", renderData.m_isosurfaceData.ambientLights);
      P.setUniform("lightDiffuse", renderData.m_isosurfaceData.diffuseLights);
      P.setUniform("lightSpecular", renderData.m_isosurfaceData.specularLights);
      P.setUniform("lightShininess", renderData.m_isosurfaceData.shininesses);

      /// @todo Set this to larger sampling factor when the user is moving the slider
      P.setUniform("samplingFactor", renderData.m_raycastSamplingFactor);

      P.setUniform("renderFrontFaces", renderData.m_renderFrontFaces);
      P.setUniform("renderBackFaces", renderData.m_renderBackFaces);

      P.setUniform(
        "segMasksIn", (RenderData::SegMaskingForRaycasting::SegMasksIn == renderData.m_segMasking)
      );
      P.setUniform(
        "segMasksOut", (RenderData::SegMaskingForRaycasting::SegMasksOut == renderData.m_segMasking)
      );

      P.setUniform("bgColor", renderData.m_3dBackgroundColor.a * renderData.m_3dBackgroundColor);
      P.setUniform("noHitTransparent", renderData.m_3dTransparentIfNoHit);

      volumeRenderOneImage(view, P, CurrentImages{imgSegPair});
    }
    P.stopUse();

    unbindTextures(boundImageTextures);
    unbindBufferTextures(boundBufferTextures);

    break;
  }

  case camera::ShaderGroup::None:
  default:
  {
    return;
  }
  }
}

void Rendering::renderAllLandmarks(
  const View& view, const FrameBounds& miewportViewBounds, const glm::vec3& worldOffsetXhairs
)
{
  const auto shaderType = view.renderMode();
  const auto metricImages = view.metricImages();
  const auto renderedImages = view.renderedImages();

  CurrentImages I;

  if (camera::ViewRenderMode::Image == shaderType)
  {
    I = getImageAndSegUidsForImageShaders(renderedImages);

    for (const auto& imgSegPair : I)
    {
      drawLandmarks(
        m_nvg, miewportViewBounds, worldOffsetXhairs, m_appData, view, CurrentImages{imgSegPair}
      );

      setupOpenGlState();
    }
  }
  else if (camera::ViewRenderMode::Checkerboard == shaderType || camera::ViewRenderMode::Quadrants == shaderType || camera::ViewRenderMode::Flashlight == shaderType)
  {
    I = getImageAndSegUidsForMetricShaders(metricImages); // guaranteed size 2

    for (const auto& imgSegPair : I)
    {
      drawLandmarks(
        m_nvg, miewportViewBounds, worldOffsetXhairs, m_appData, view, CurrentImages{imgSegPair}
      );

      setupOpenGlState();
    }
  }
  else if (camera::ViewRenderMode::Disabled == shaderType)
  {
    return;
  }
  else
  {
    // This function guarantees that I has size at least 2:
    drawLandmarks(
      m_nvg,
      miewportViewBounds,
      worldOffsetXhairs,
      m_appData,
      view,
      getImageAndSegUidsForMetricShaders(metricImages)
    );

    setupOpenGlState();
  }
}

void Rendering::renderAllAnnotations(
  const View& view, const FrameBounds& miewportViewBounds, const glm::vec3& worldOffsetXhairs
)
{
  const auto shaderType = view.renderMode();
  const auto& metricImages = view.metricImages();
  const auto& renderedImages = view.renderedImages();

  CurrentImages I;

  if (camera::ViewRenderMode::Image == shaderType)
  {
    I = getImageAndSegUidsForImageShaders(renderedImages);

    for (const auto& imgSegPair : I)
    {
      drawAnnotations(
        m_nvg, miewportViewBounds, worldOffsetXhairs, m_appData, view, CurrentImages{imgSegPair}
      );

      setupOpenGlState();
    }
  }
  else if (camera::ViewRenderMode::Checkerboard == shaderType || camera::ViewRenderMode::Quadrants == shaderType || camera::ViewRenderMode::Flashlight == shaderType)
  {
    I = getImageAndSegUidsForMetricShaders(metricImages); // guaranteed size 2

    for (const auto& imgSegPair : I)
    {
      drawAnnotations(
        m_nvg, miewportViewBounds, worldOffsetXhairs, m_appData, view, CurrentImages{imgSegPair}
      );

      setupOpenGlState();
    }
  }
  else if (camera::ViewRenderMode::Disabled == shaderType)
  {
    return;
  }
  else
  {
    // This function guarantees that I has size at least 2:
    drawAnnotations(
      m_nvg,
      miewportViewBounds,
      worldOffsetXhairs,
      m_appData,
      view,
      getImageAndSegUidsForMetricShaders(metricImages)
    );

    setupOpenGlState();
  }
}

void Rendering::renderImageData()
{
  if (!m_isAppDoneLoadingImages)
  {
    // Don't render images if the app is still loading them
    return;
  }

  auto& renderData = m_appData.renderData();

  const bool renderLandmarksOnTop = renderData.m_globalLandmarkParams.renderOnTopOfAllImagePlanes;
  const bool renderAnnotationsOnTop = renderData.m_globalAnnotationParams.renderOnTopOfAllImagePlanes;

  // Render images for each view in the layout
  for (const auto& V : m_appData.windowData().currentLayout().views())
  {
    if (!V.second)
      continue;
    View& view = *(V.second);

    // Offset the crosshairs according to the image slice in the view
    const glm::vec3 worldOffsetXhairs
      = view.updateImageSlice(m_appData, m_appData.state().worldCrosshairs().worldOrigin());

    const auto miewportViewBounds = camera::computeMiewportFrameBounds(
      view.windowClipViewport(), m_appData.windowData().viewport().getAsVec4()
    );

    renderAllImages(view, miewportViewBounds, worldOffsetXhairs);

    // Do not render landmarks and annotations in volume rendering mode
    if (camera::ViewRenderMode::VolumeRender != view.renderMode())
    {
      if (renderLandmarksOnTop)
      {
        renderAllLandmarks(view, miewportViewBounds, worldOffsetXhairs);
      }

      if (renderAnnotationsOnTop)
      {
        renderAllAnnotations(view, miewportViewBounds, worldOffsetXhairs);
      }
    }
  }
}

void Rendering::renderOverlays()
{
  /*
    m_simpleProgram.use();
    {
        for ( const auto& view : m_appData.m_windowData.currentViews() )
        {
            switch ( view.renderMode() )
            {
            case camera::ShaderType::Image:
            case camera::ShaderType::MetricMI:
            case camera::ShaderType::MetricNCC:
            case camera::ShaderType::MetricSSD:
            case camera::ShaderType::Checkerboard:
            {
                renderCrosshairs( m_simpleProgram, m_appData.renderData().m_quad, view,
                                  m_appData.m_worldCrosshairs.worldOrigin() );
                break;
            }
            case camera::ShaderType::None:
            {
                break;
            }
            }
        }
    }
    m_simpleProgram.stopUse();
    */
}

void Rendering::renderVectorOverlays()
{
  if (!m_nvg)
    return;

  const WindowData& windowData = m_appData.windowData();
  const Viewport& windowVP = windowData.viewport();

  if (!m_isAppDoneLoadingImages)
  {
    startNvgFrame(m_nvg, windowVP);
    drawLoadingOverlay(m_nvg, windowVP);
    endNvgFrame(m_nvg);
    return;

    //            nvgFontSize( m_nvg, 64.0f );
    //            const char* txt = "Text me up.";
    //            float bounds[4];
    //            nvgTextBounds( m_nvg, 10, 10, txt, NULL, bounds );
    //            nvgBeginPath( m_nvg );
    ////            nvgRoundedRect( m_nvg, bounds[0],bounds[1], bounds[2]-bounds[0], bounds[3]-bounds[1], 0 );
    //            nvgText( m_nvg, vp.width() / 2, vp.height() / 2, "Loading images...", NULL );
    //            nvgFill( m_nvg );
  }

  startNvgFrame(m_nvg, windowVP);

  glm::mat4 world_T_refSubject(1.0f);

  if (m_appData.settings().lockAnatomicalCoordinateAxesWithReferenceImage())
  {
    if (const Image* refImage = m_appData.refImage())
    {
      world_T_refSubject = refImage->transformations().worldDef_T_subject();
    }
  }

  for (const auto& viewUid : windowData.currentViewUids())
  {
    const View* view = windowData.getCurrentView(viewUid);
    if (!view)
      continue;

    // Bounds of the view frame in Miewport space:
    const auto miewportViewBounds
      = camera::computeMiewportFrameBounds(view->windowClipViewport(), windowVP.getAsVec4());

    // Do not render vector overlays when view is disabled
    if (m_showOverlays && camera::ViewRenderMode::Disabled != view->renderMode())
    {
      const auto labelPosInfo = math::computeAnatomicalLabelPosInfo(
        miewportViewBounds,
        windowVP,
        view->camera(),
        world_T_refSubject,
        view->windowClip_T_viewClip(),
        m_appData.state().worldCrosshairs().worldOrigin()
      );

      // Do not render crosshairs in volume rendering mode
      if (camera::ViewRenderMode::VolumeRender != view->renderMode())
      {
        drawCrosshairs(
          m_nvg, miewportViewBounds, *view, m_appData.renderData().m_crosshairsColor, labelPosInfo
        );
      }

      if (AnatomicalLabelType::Disabled != m_appData.renderData().m_anatomicalLabelType)
      {
        drawAnatomicalLabels(
          m_nvg,
          miewportViewBounds,
          (ViewType::Oblique == view->viewType()),
          m_appData.renderData().m_anatomicalLabelColor,
          m_appData.renderData().m_anatomicalLabelType,
          labelPosInfo
        );
      }
    }

    ViewOutlineMode outlineMode = ViewOutlineMode::None;

    if (state::isInStateWhereViewSelectionsVisible() && ASM::current_state_ptr)
    {
      const auto hoveredViewUid = ASM::current_state_ptr->hoveredViewUid();
      const auto selectedViewUid = ASM::current_state_ptr->selectedViewUid();

      if (selectedViewUid && (viewUid == *selectedViewUid))
      {
        outlineMode = ViewOutlineMode::Selected;
      }
      else if (hoveredViewUid && (viewUid == *hoveredViewUid))
      {
        outlineMode = ViewOutlineMode::Hovered;
      }
    }

    drawViewOutline(m_nvg, miewportViewBounds, outlineMode);
  }

  drawWindowOutline(m_nvg, windowVP);

  endNvgFrame(m_nvg);
}

void Rendering::createShaderPrograms()
{
  if (!createCrossCorrelationProgram(m_crossCorrelationProgram))
  {
    throw_debug("Failed to create cross-correlation metric program")
  }

  if (!createDifferenceProgram(m_differenceProgram))
  {
    throw_debug("Failed to create difference metric program")
  }

  if (!createEdgeProgram(m_edgeProgram))
  {
    throw_debug("Failed to create edge detection program")
  }

  if (!createImageProgram(m_imageProgram))
  {
    throw_debug("Failed to create image program")
  }

  if (!createImageRgbaProgram(m_imageRgbaProgram))
  {
    throw_debug("Failed to create color image program")
  }

  if (!createXrayProgram(m_xrayProgram))
  {
    throw_debug("Failed to create x-ray projection program")
  }

  if (!createOverlayProgram(m_overlayProgram))
  {
    throw_debug("Failed to create overlay program")
  }

  if (!createSimpleProgram(m_simpleProgram))
  {
    throw_debug("Failed to create simple program")
  }

  if (!createRaycastIsoSurfaceProgram(m_raycastIsoSurfaceProgram))
  {
    throw_debug("Failed to create isosurface raycasting program")
  }
}

bool Rendering::createImageProgram(GLShaderProgram& program)
{
  static const std::string vsFileName{"src/rendering/shaders/Image.vs"};
  static const std::string fsFileName{"src/rendering/shaders/Image.fs"};

  auto filesystem = cmrc::shaders::get_filesystem();
  std::string vsSource;
  std::string fsSource;

  try
  {
    cmrc::file vsData = filesystem.open(vsFileName.c_str());
    cmrc::file fsData = filesystem.open(fsFileName.c_str());

    vsSource = std::string(vsData.begin(), vsData.end());
    fsSource = std::string(fsData.begin(), fsData.end());
  }
  catch (const std::exception& e)
  {
    spdlog::critical("Exception when loading shader file: {}", e.what());
    throw_debug("Unable to load shader")
  }

  {
    Uniforms vsUniforms;
    vsUniforms.insertUniform("u_view_T_clip", UniformType::Mat4, sk_identMat4);
    vsUniforms.insertUniform("u_world_T_clip", UniformType::Mat4, sk_identMat4);
    vsUniforms.insertUniform("u_clipDepth", UniformType::Float, 0.0f);

    // For checkerboarding:
    vsUniforms.insertUniform("u_aspectRatio", UniformType::Float, 1.0f);
    vsUniforms.insertUniform("u_numSquares", UniformType::Int, 1);

    vsUniforms.insertUniform("u_imgTexture_T_world", UniformType::Mat4, sk_identMat4);
    vsUniforms.insertUniform("u_segTexture_T_world", UniformType::Mat4, sk_identMat4);
    vsUniforms.insertUniform("u_segVoxel_T_world", UniformType::Mat4, sk_identMat4);

    auto vs = std::make_shared<GLShader>("vsImage", ShaderType::Vertex, vsSource.c_str());
    vs->setRegisteredUniforms(std::move(vsUniforms));
    program.attachShader(vs);

    spdlog::debug("Compiled vertex shader {}", vsFileName);
  }

  {
    Uniforms fsUniforms;

    fsUniforms.insertUniform("u_imgTex", UniformType::Sampler, msk_imgTexSampler);
    fsUniforms.insertUniform("u_segTex", UniformType::Sampler, msk_segTexSampler);
    fsUniforms.insertUniform("u_imgCmapTex", UniformType::Sampler, msk_imgCmapTexSampler);
    fsUniforms.insertUniform("u_segLabelCmapTex", UniformType::Sampler, msk_labelTableTexSampler);

    fsUniforms.insertUniform("u_imgSlopeIntercept", UniformType::Vec2, sk_zeroVec2);
    fsUniforms.insertUniform("u_imgCmapSlopeIntercept", UniformType::Vec2, sk_zeroVec2);
    fsUniforms.insertUniform("u_imgCmapQuantLevels", UniformType::Int, 0);
    fsUniforms
      .insertUniform("u_imgCmapHsvModFactors", UniformType::Vec3, glm::vec3{0.0f, 1.0f, 0.5f});

    fsUniforms.insertUniform("u_imgMinMax", UniformType::Vec2, sk_zeroVec2);
    fsUniforms.insertUniform("u_imgThresholds", UniformType::Vec2, sk_zeroVec2);
    fsUniforms.insertUniform("u_imgOpacity", UniformType::Float, 0.0f);
    fsUniforms.insertUniform("u_segOpacity", UniformType::Float, 0.0f);

    fsUniforms.insertUniform("u_masking", UniformType::Bool, false);

    fsUniforms.insertUniform("u_quadrants", UniformType::IVec2, sk_zeroIVec2); // For quadrants
    fsUniforms.insertUniform("u_showFix", UniformType::Bool, true); // For checkerboarding
    fsUniforms.insertUniform(
      "u_renderMode", UniformType::Int, 0
    ); // 0: image, 1: checkerboard, 2: quadrants, 3: flashlight

    // For flashlighting:
    fsUniforms.insertUniform("u_flashlightRadius", UniformType::Float, 0.5f);
    fsUniforms.insertUniform("u_flashlightOverlays", UniformType::Bool, true);

    // For intensity projection:
    // 0: none, 1: max, 2: mean, 3: min, 4: x-ray (not used in image shader)
    fsUniforms.insertUniform("u_mipMode", UniformType::Int, 0);
    fsUniforms.insertUniform("u_halfNumMipSamples", UniformType::Int, 0);
    fsUniforms.insertUniform("u_texSamplingDirZ", UniformType::Vec3, sk_zeroVec3);

    fsUniforms.insertUniform("u_segInteriorOpacity", UniformType::Float, 1.0f);
    //        fsUniforms.insertUniform( "u_segInterpCutoff", UniformType::Float, 0.5f );
    fsUniforms.insertUniform(
      "u_texSamplingDirsForSegOutline", UniformType::Vec3Vector, Vec3Vector{sk_zeroVec3}
    );
    //        fsUniforms.insertUniform( "u_texSamplingDirsForSmoothSeg", UniformType::Vec3Vector, Vec3Vector{ sk_zeroVec3 } );

    fsUniforms.insertUniform("u_isoValues", UniformType::FloatVector, FloatVector{0.0f});
    fsUniforms.insertUniform("u_isoOpacities", UniformType::FloatVector, FloatVector{1.0f});
    fsUniforms.insertUniform("u_isoColors", UniformType::Vec3Vector, Vec3Vector{sk_zeroVec3});
    fsUniforms.insertUniform("u_isoWidth", UniformType::Float, 0.0f);

    auto fs = std::make_shared<GLShader>("fsImage", ShaderType::Fragment, fsSource.c_str());
    fs->setRegisteredUniforms(std::move(fsUniforms));
    program.attachShader(fs);

    spdlog::debug("Compiled fragment shader {}", fsFileName);
  }

  if (!program.link())
  {
    spdlog::critical("Failed to link shader program {}", program.name());
    return false;
  }

  spdlog::debug("Linked shader program {}", program.name());
  return true;
}

bool Rendering::createImageRgbaProgram(GLShaderProgram& program)
{
  static const std::string vsFileName{"src/rendering/shaders/Image.vs"};
  static const std::string fsFileName{"src/rendering/shaders/ImageRgba.fs"};

  auto filesystem = cmrc::shaders::get_filesystem();
  std::string vsSource;
  std::string fsSource;

  try
  {
    cmrc::file vsData = filesystem.open(vsFileName.c_str());
    cmrc::file fsData = filesystem.open(fsFileName.c_str());

    vsSource = std::string(vsData.begin(), vsData.end());
    fsSource = std::string(fsData.begin(), fsData.end());
  }
  catch (const std::exception& e)
  {
    spdlog::critical("Exception when loading shader file: {}", e.what());
    throw_debug("Unable to load shader")
  }

  {
    Uniforms vsUniforms;
    vsUniforms.insertUniform("u_view_T_clip", UniformType::Mat4, sk_identMat4);
    vsUniforms.insertUniform("u_world_T_clip", UniformType::Mat4, sk_identMat4);
    vsUniforms.insertUniform("u_clipDepth", UniformType::Float, 0.0f);

    // For checkerboarding:
    vsUniforms.insertUniform("u_aspectRatio", UniformType::Float, 1.0f);
    vsUniforms.insertUniform("u_numSquares", UniformType::Int, 1);

    vsUniforms.insertUniform("u_imgTexture_T_world", UniformType::Mat4, sk_identMat4);
    vsUniforms.insertUniform("u_segTexture_T_world", UniformType::Mat4, sk_identMat4);
    vsUniforms.insertUniform("u_segVoxel_T_world", UniformType::Mat4, sk_identMat4);

    auto vs = std::make_shared<GLShader>("vsImage", ShaderType::Vertex, vsSource.c_str());
    vs->setRegisteredUniforms(std::move(vsUniforms));
    program.attachShader(vs);

    spdlog::debug("Compiled vertex shader {}", vsFileName);
  }

  {
    Uniforms fsUniforms;

    fsUniforms.insertUniform("u_imgTex", UniformType::SamplerVector, msk_imgRgbaTexSamplers);
    fsUniforms.insertUniform("u_segTex", UniformType::Sampler, msk_segTexSampler);
    fsUniforms.insertUniform("u_segLabelCmapTex", UniformType::Sampler, msk_labelTableTexSampler);

    // fsUniforms.insertUniform( "u_useTricubicInterpolation", UniformType::Bool, false );

    fsUniforms.insertUniform("u_imgSlopeIntercept", UniformType::Vec2Vector, Vec2Vector{sk_zeroVec2});
    fsUniforms.insertUniform("u_alphaIsOne", UniformType::Bool, true);

    fsUniforms.insertUniform("u_imgOpacity", UniformType::FloatVector, FloatVector{0.0f});
    fsUniforms.insertUniform("u_segOpacity", UniformType::Float, 0.0f);

    fsUniforms.insertUniform("u_segInteriorOpacity", UniformType::Float, 1.0f);
    fsUniforms.insertUniform(
      "u_texSamplingDirsForSegOutline", UniformType::Vec3Vector, Vec3Vector{sk_zeroVec3}
    );

    fsUniforms.insertUniform("u_imgMinMax", UniformType::Vec2Vector, Vec2Vector{sk_zeroVec2});
    fsUniforms.insertUniform("u_imgThresholds", UniformType::Vec2Vector, Vec2Vector{sk_zeroVec2});

    fsUniforms.insertUniform("u_masking", UniformType::Bool, false);

    fsUniforms.insertUniform("u_quadrants", UniformType::IVec2, sk_zeroIVec2); // For quadrants
    fsUniforms.insertUniform("u_showFix", UniformType::Bool, true); // For checkerboarding
    fsUniforms.insertUniform(
      "u_renderMode", UniformType::Int, 0
    ); // 0: image, 1: checkerboard, 2: quadrants, 3: flashlight

    // For flashlighting:
    fsUniforms.insertUniform("u_flashlightRadius", UniformType::Float, 0.5f);
    fsUniforms.insertUniform("u_flashlightOverlays", UniformType::Bool, true);

    auto fs = std::make_shared<GLShader>("fsImage", ShaderType::Fragment, fsSource.c_str());
    fs->setRegisteredUniforms(std::move(fsUniforms));
    program.attachShader(fs);

    spdlog::debug("Compiled fragment shader {}", fsFileName);
  }

  if (!program.link())
  {
    spdlog::critical("Failed to link shader program {}", program.name());
    return false;
  }

  spdlog::debug("Linked shader program {}", program.name());
  return true;
}

bool Rendering::createXrayProgram(GLShaderProgram& program)
{
  static const std::string vsFileName{"src/rendering/shaders/Image.vs"};
  static const std::string fsFileName{"src/rendering/shaders/Xray.fs"};

  auto filesystem = cmrc::shaders::get_filesystem();
  std::string vsSource;
  std::string fsSource;

  try
  {
    cmrc::file vsData = filesystem.open(vsFileName.c_str());
    cmrc::file fsData = filesystem.open(fsFileName.c_str());

    vsSource = std::string(vsData.begin(), vsData.end());
    fsSource = std::string(fsData.begin(), fsData.end());
  }
  catch (const std::exception& e)
  {
    spdlog::critical("Exception when loading shader file: {}", e.what());
    throw_debug("Unable to load shader")
  }

  {
    Uniforms vsUniforms;
    vsUniforms.insertUniform("u_view_T_clip", UniformType::Mat4, sk_identMat4);
    vsUniforms.insertUniform("u_world_T_clip", UniformType::Mat4, sk_identMat4);
    vsUniforms.insertUniform("u_clipDepth", UniformType::Float, 0.0f);

    // For checkerboarding:
    vsUniforms.insertUniform("u_aspectRatio", UniformType::Float, 1.0f);
    vsUniforms.insertUniform("u_numSquares", UniformType::Int, 1);

    vsUniforms.insertUniform("u_imgTexture_T_world", UniformType::Mat4, sk_identMat4);
    vsUniforms.insertUniform("u_segTexture_T_world", UniformType::Mat4, sk_identMat4);
    vsUniforms.insertUniform("u_segVoxel_T_world", UniformType::Mat4, sk_identMat4);

    auto vs = std::make_shared<GLShader>("vsImage", ShaderType::Vertex, vsSource.c_str());
    vs->setRegisteredUniforms(std::move(vsUniforms));
    program.attachShader(vs);

    spdlog::debug("Compiled vertex shader {}", vsFileName);
  }

  {
    Uniforms fsUniforms;

    fsUniforms.insertUniform("u_imgTex", UniformType::Sampler, msk_imgTexSampler);
    fsUniforms.insertUniform("u_segTex", UniformType::Sampler, msk_segTexSampler);
    fsUniforms.insertUniform("u_imgCmapTex", UniformType::Sampler, msk_imgCmapTexSampler);
    fsUniforms.insertUniform("u_segLabelCmapTex", UniformType::Sampler, msk_labelTableTexSampler);

    // fsUniforms.insertUniform( "u_useTricubicInterpolation", UniformType::Bool, false );

    fsUniforms.insertUniform("imgSlope_native_T_texture", UniformType::Float, 1.0f);
    fsUniforms.insertUniform("u_imgCmapSlopeIntercept", UniformType::Vec2, sk_zeroVec2);
    //        fsUniforms.insertUniform( "u_imgCmapQuantLevels", UniformType::Int, 0 );
    fsUniforms.insertUniform("u_imgMinMax", UniformType::Vec2, sk_zeroVec2);
    fsUniforms.insertUniform("u_imgThresholds", UniformType::Vec2, sk_zeroVec2);
    fsUniforms.insertUniform("slopeInterceptWindowLevel", UniformType::Vec2, sk_zeroVec2);

    fsUniforms.insertUniform("u_imgOpacity", UniformType::Float, 0.0f);
    fsUniforms.insertUniform("u_segOpacity", UniformType::Float, 0.0f);

    fsUniforms.insertUniform("u_segInteriorOpacity", UniformType::Float, 1.0f);
    fsUniforms.insertUniform(
      "u_texSamplingDirsForSegOutline", UniformType::Vec3Vector, Vec3Vector{sk_zeroVec3}
    );

    fsUniforms.insertUniform("u_masking", UniformType::Bool, false);

    fsUniforms.insertUniform("u_quadrants", UniformType::IVec2, sk_zeroIVec2); // For quadrants
    fsUniforms.insertUniform("u_showFix", UniformType::Bool, true); // For checkerboarding
    fsUniforms.insertUniform(
      "u_renderMode", UniformType::Int, 0
    ); // 0: image, 1: checkerboard, 2: quadrants, 3: flashlight

    // For flashlighting:
    fsUniforms.insertUniform("u_flashlightRadius", UniformType::Float, 0.5f);
    fsUniforms.insertUniform("u_flashlightOverlays", UniformType::Bool, true);

    // For X-ray projection mode:
    fsUniforms.insertUniform("u_halfNumMipSamples", UniformType::Int, 0);
    fsUniforms.insertUniform("mipSamplingDistance_cm", UniformType::Float, 0.0f);
    fsUniforms.insertUniform("u_texSamplingDirZ", UniformType::Vec3, sk_zeroVec3);
    fsUniforms.insertUniform("waterAttenCoeff", UniformType::Float, 0.0f);
    fsUniforms.insertUniform("airAttenCoeff", UniformType::Float, 0.0f);

    auto fs = std::make_shared<GLShader>("fsXray", ShaderType::Fragment, fsSource.c_str());
    fs->setRegisteredUniforms(std::move(fsUniforms));
    program.attachShader(fs);

    spdlog::debug("Compiled fragment shader {}", fsFileName);
  }

  if (!program.link())
  {
    spdlog::critical("Failed to link shader program {}", program.name());
    return false;
  }

  spdlog::debug("Linked shader program {}", program.name());
  return true;
}

bool Rendering::createRaycastIsoSurfaceProgram(GLShaderProgram& program)
{
  static const std::string vsFileName{"src/rendering/shaders/RaycastIsoSurface.vs"};
  static const std::string fsFileName{"src/rendering/shaders/RaycastIsoSurface.fs"};

  auto filesystem = cmrc::shaders::get_filesystem();
  std::string vsSource;
  std::string fsSource;

  try
  {
    cmrc::file vsData = filesystem.open(vsFileName.c_str());
    cmrc::file fsData = filesystem.open(fsFileName.c_str());

    vsSource = std::string(vsData.begin(), vsData.end());
    fsSource = std::string(fsData.begin(), fsData.end());
  }
  catch (const std::exception& e)
  {
    spdlog::critical("Exception when loading shader file: {}", e.what());
    throw_debug("Unable to load shader")
  }

  {
    Uniforms vsUniforms;
    vsUniforms.insertUniform("u_view_T_clip", UniformType::Mat4, sk_identMat4);
    vsUniforms.insertUniform("u_world_T_clip", UniformType::Mat4, sk_identMat4);
    vsUniforms.insertUniform("clip_T_world", UniformType::Mat4, sk_identMat4);
    vsUniforms.insertUniform("u_clipDepth", UniformType::Float, 0.0f);

    auto vs = std::make_shared<GLShader>("vsRaycast", ShaderType::Vertex, vsSource.c_str());
    vs->setRegisteredUniforms(std::move(vsUniforms));
    program.attachShader(vs);

    spdlog::debug("Compiled vertex shader {}", vsFileName);
  }

  {
    Uniforms fsUniforms;

    fsUniforms.insertUniform("u_imgTex", UniformType::Sampler, msk_imgTexSampler);
    fsUniforms.insertUniform("u_segTex", UniformType::Sampler, msk_segTexSampler);
    fsUniforms.insertUniform("u_jumpTex", UniformType::Sampler, msk_jumpTexSampler);

    // fsUniforms.insertUniform( "u_useTricubicInterpolation", UniformType::Bool, false );

    fsUniforms.insertUniform("u_imgTexture_T_world", UniformType::Mat4, sk_identMat4);
    fsUniforms.insertUniform("world_T_imgTexture", UniformType::Mat4, sk_identMat4);

    fsUniforms.insertUniform("worldEyePos", UniformType::Vec3, sk_zeroVec3);
    fsUniforms.insertUniform("texGrads", UniformType::Mat3, sk_identMat3);

    fsUniforms.insertUniform("u_isoValues", UniformType::FloatVector, FloatVector{0.0f});
    fsUniforms.insertUniform("u_isoOpacities", UniformType::FloatVector, FloatVector{1.0f});
    fsUniforms.insertUniform("isoEdges", UniformType::FloatVector, FloatVector{0.0f});

    fsUniforms.insertUniform("lightAmbient", UniformType::Vec3Vector, Vec3Vector{sk_zeroVec3});
    fsUniforms.insertUniform("lightDiffuse", UniformType::Vec3Vector, Vec3Vector{sk_zeroVec3});
    fsUniforms.insertUniform("lightSpecular", UniformType::Vec3Vector, Vec3Vector{sk_zeroVec3});
    fsUniforms.insertUniform("lightShininess", UniformType::FloatVector, FloatVector{0.0f});

    fsUniforms.insertUniform("bgColor", UniformType::Vec4, sk_zeroVec4);

    fsUniforms.insertUniform("samplingFactor", UniformType::Float, 1.0f);

    fsUniforms.insertUniform("renderFrontFaces", UniformType::Bool, true);
    fsUniforms.insertUniform("renderBackFaces", UniformType::Bool, true);
    fsUniforms.insertUniform("noHitTransparent", UniformType::Bool, true);

    fsUniforms.insertUniform("segMasksIn", UniformType::Bool, false);
    fsUniforms.insertUniform("segMasksOut", UniformType::Bool, false);

    auto fs = std::make_shared<GLShader>("fsRaycast", ShaderType::Fragment, fsSource.c_str());
    fs->setRegisteredUniforms(std::move(fsUniforms));
    program.attachShader(fs);

    spdlog::debug("Compiled fragment shader {}", fsFileName);
  }

  if (!program.link())
  {
    spdlog::critical("Failed to link shader program {}", program.name());
    return false;
  }

  spdlog::debug("Linked shader program {}", program.name());
  return true;
}

bool Rendering::createEdgeProgram(GLShaderProgram& program)
{
  static const std::string vsFileName{"src/rendering/shaders/Image.vs"};
  static const std::string fsFileName{"src/rendering/shaders/Edge.fs"};

  auto filesystem = cmrc::shaders::get_filesystem();
  std::string vsSource;
  std::string fsSource;

  try
  {
    cmrc::file vsData = filesystem.open(vsFileName.c_str());
    cmrc::file fsData = filesystem.open(fsFileName.c_str());

    vsSource = std::string(vsData.begin(), vsData.end());
    fsSource = std::string(fsData.begin(), fsData.end());
  }
  catch (const std::exception& e)
  {
    spdlog::critical("Exception when loading shader file: {}", e.what());
    throw_debug("Unable to load shader")
  }

  {
    Uniforms vsUniforms;
    vsUniforms.insertUniform("u_view_T_clip", UniformType::Mat4, sk_identMat4);
    vsUniforms.insertUniform("u_world_T_clip", UniformType::Mat4, sk_identMat4);
    vsUniforms.insertUniform("u_clipDepth", UniformType::Float, 0.0f);

    // For checkerboarding:
    vsUniforms.insertUniform("u_aspectRatio", UniformType::Float, 1.0f);
    vsUniforms.insertUniform("u_numSquares", UniformType::Int, 1);

    vsUniforms.insertUniform("u_imgTexture_T_world", UniformType::Mat4, sk_identMat4);
    vsUniforms.insertUniform("u_segTexture_T_world", UniformType::Mat4, sk_identMat4);
    vsUniforms.insertUniform("u_segVoxel_T_world", UniformType::Mat4, sk_identMat4);

    auto vs = std::make_shared<GLShader>("vsEdge", ShaderType::Vertex, vsSource.c_str());
    vs->setRegisteredUniforms(std::move(vsUniforms));
    program.attachShader(vs);

    spdlog::debug("Compiled vertex shader {}", vsFileName);
  }

  {
    Uniforms fsUniforms;

    fsUniforms.insertUniform("u_imgTex", UniformType::Sampler, msk_imgTexSampler);
    fsUniforms.insertUniform("u_segTex", UniformType::Sampler, msk_segTexSampler);
    fsUniforms.insertUniform("u_imgCmapTex", UniformType::Sampler, msk_imgCmapTexSampler);
    fsUniforms.insertUniform("u_segLabelCmapTex", UniformType::Sampler, msk_labelTableTexSampler);

    // fsUniforms.insertUniform( "u_useTricubicInterpolation", UniformType::Bool, false );

    fsUniforms.insertUniform("u_imgSlopeIntercept", UniformType::Vec2, sk_zeroVec2);
    fsUniforms.insertUniform("u_imgSlopeInterceptLargest", UniformType::Vec2, sk_zeroVec2);
    fsUniforms.insertUniform("u_imgCmapSlopeIntercept", UniformType::Vec2, sk_zeroVec2);
    fsUniforms.insertUniform("u_imgCmapQuantLevels", UniformType::Int, 0);
    fsUniforms
      .insertUniform("u_imgCmapHsvModFactors", UniformType::Vec3, glm::vec3{0.0f, 1.0f, 0.5f});

    fsUniforms.insertUniform("u_imgMinMax", UniformType::Vec2, sk_zeroVec2);
    fsUniforms.insertUniform("u_imgThresholds", UniformType::Vec2, sk_zeroVec2);
    fsUniforms.insertUniform("u_imgOpacity", UniformType::Float, 0.0f);
    fsUniforms.insertUniform("u_segOpacity", UniformType::Float, 0.0f);

    fsUniforms.insertUniform("u_masking", UniformType::Bool, false);

    fsUniforms.insertUniform("u_quadrants", UniformType::IVec2, sk_zeroIVec2);
    fsUniforms.insertUniform("u_showFix", UniformType::Bool, true);
    fsUniforms.insertUniform("u_renderMode", UniformType::Int, 0);

    // For flashlighting:
    fsUniforms.insertUniform("u_flashlightRadius", UniformType::Float, 0.5f);
    fsUniforms.insertUniform("u_flashlightOverlays", UniformType::Bool, true);

    fsUniforms.insertUniform("u_thresholdEdges", UniformType::Bool, true);
    fsUniforms.insertUniform("u_edgeMagnitude", UniformType::Float, 0.0f);
    //        fsUniforms.insertUniform( "useFreiChen", UniformType::Bool, false );
    //        fsUniforms.insertUniform( "windowedEdges", UniformType::Bool, false );
    fsUniforms.insertUniform("u_overlayEdges", UniformType::Bool, false);
    fsUniforms.insertUniform("u_colormapEdges", UniformType::Bool, false);
    fsUniforms.insertUniform("u_edgeColor", UniformType::Vec4, sk_zeroVec4);

    fsUniforms
      .insertUniform("u_texSamplingDirsForEdges", UniformType::Vec3Vector, Vec3Vector{sk_zeroVec3});

    fsUniforms.insertUniform(
      "u_texSamplingDirsForSegOutline", UniformType::Vec3Vector, Vec3Vector{sk_zeroVec3}
    );
    fsUniforms.insertUniform("u_segInteriorOpacity", UniformType::Float, 1.0f);

    auto fs = std::make_shared<GLShader>("fsEdge", ShaderType::Fragment, fsSource.c_str());
    fs->setRegisteredUniforms(std::move(fsUniforms));
    program.attachShader(fs);

    spdlog::debug("Compiled fragment shader {}", fsFileName);
  }

  if (!program.link())
  {
    spdlog::critical("Failed to link shader program {}", program.name());
    return false;
  }

  spdlog::debug("Linked shader program {}", program.name());
  return true;
}

bool Rendering::createOverlayProgram(GLShaderProgram& program)
{
  static const std::string vsFileName{"src/rendering/shaders/Metric.vs"};
  static const std::string fsFileName{"src/rendering/shaders/Overlay.fs"};

  auto filesystem = cmrc::shaders::get_filesystem();
  std::string vsSource;
  std::string fsSource;

  try
  {
    cmrc::file vsData = filesystem.open(vsFileName.c_str());
    cmrc::file fsData = filesystem.open(fsFileName.c_str());

    vsSource = std::string(vsData.begin(), vsData.end());
    fsSource = std::string(fsData.begin(), fsData.end());
  }
  catch (const std::exception& e)
  {
    spdlog::critical("Exception when loading shader file: {}", e.what());
    throw_debug("Unable to load shader")
  }

  {
    Uniforms vsUniforms;
    vsUniforms.insertUniform("u_view_T_clip", UniformType::Mat4, sk_identMat4);
    vsUniforms.insertUniform("u_world_T_clip", UniformType::Mat4, sk_identMat4);
    vsUniforms.insertUniform("u_clipDepth", UniformType::Float, 0.0f);

    vsUniforms.insertUniform(
      "u_imgTexture_T_world", UniformType::Mat4Vector, Mat4Vector{sk_identMat4, sk_identMat4}
    );
    vsUniforms.insertUniform(
      "u_segTexture_T_world", UniformType::Mat4Vector, Mat4Vector{sk_identMat4, sk_identMat4}
    );

    auto vs = std::make_shared<GLShader>("vsOverlay", ShaderType::Vertex, vsSource.c_str());
    vs->setRegisteredUniforms(std::move(vsUniforms));
    program.attachShader(vs);

    spdlog::debug("Compiled vertex shader {}", vsFileName);
  }

  {
    Uniforms fsUniforms;

    fsUniforms.insertUniform("u_imgTex", UniformType::SamplerVector, msk_imgTexSamplers);
    fsUniforms.insertUniform("u_segTex", UniformType::SamplerVector, msk_segTexSamplers);

    fsUniforms
      .insertUniform("u_segLabelCmapTex", UniformType::SamplerVector, msk_labelTableTexSamplers);

    // fsUniforms.insertUniform( "u_useTricubicInterpolation", UniformType::Bool, false );

    fsUniforms.insertUniform(
      "u_imgSlopeIntercept", UniformType::Vec2Vector, Vec2Vector{sk_zeroVec2, sk_zeroVec2}
    );

    fsUniforms
      .insertUniform("u_imgMinMax", UniformType::Vec2Vector, Vec2Vector{sk_zeroVec2, sk_zeroVec2});
    fsUniforms.insertUniform(
      "u_imgThresholds", UniformType::Vec2Vector, Vec2Vector{sk_zeroVec2, sk_zeroVec2}
    );
    fsUniforms.insertUniform("u_imgOpacity", UniformType::FloatVector, FloatVector{0.0f, 0.0f});
    fsUniforms.insertUniform("u_segOpacity", UniformType::FloatVector, FloatVector{0.0f, 0.0f});

    fsUniforms.insertUniform("u_segInteriorOpacity", UniformType::Float, 1.0f);
    fsUniforms.insertUniform(
      "u_texSamplingDirsForSegOutline", UniformType::Vec3Vector, Vec3Vector{sk_zeroVec3}
    );

    fsUniforms.insertUniform("magentaCyan", UniformType::Bool, true);

    auto fs = std::make_shared<GLShader>("fsOverlay", ShaderType::Fragment, fsSource.c_str());
    fs->setRegisteredUniforms(std::move(fsUniforms));
    program.attachShader(fs);

    spdlog::debug("Compiled fragment shader {}", fsFileName);
  }

  if (!program.link())
  {
    spdlog::critical("Failed to link shader program {}", program.name());
    return false;
  }

  spdlog::debug("Linked shader program {}", program.name());
  return true;
}

bool Rendering::createDifferenceProgram(GLShaderProgram& program)
{
  static const std::string vsFileName{"src/rendering/shaders/Metric.vs"};
  static const std::string fsFileName{"src/rendering/shaders/Difference.fs"};

  auto filesystem = cmrc::shaders::get_filesystem();
  std::string vsSource;
  std::string fsSource;

  try
  {
    cmrc::file vsData = filesystem.open(vsFileName.c_str());
    cmrc::file fsData = filesystem.open(fsFileName.c_str());

    vsSource = std::string(vsData.begin(), vsData.end());
    fsSource = std::string(fsData.begin(), fsData.end());
  }
  catch (const std::exception& e)
  {
    spdlog::critical("Exception when loading shader file: {}", e.what());
    throw_debug("Unable to load shader")
  }

  {
    Uniforms vsUniforms;
    vsUniforms.insertUniform("u_view_T_clip", UniformType::Mat4, sk_identMat4);
    vsUniforms.insertUniform("u_world_T_clip", UniformType::Mat4, sk_identMat4);
    vsUniforms.insertUniform("u_clipDepth", UniformType::Float, 0.0f);

    vsUniforms.insertUniform(
      "u_imgTexture_T_world", UniformType::Mat4Vector, Mat4Vector{sk_identMat4, sk_identMat4}
    );
    vsUniforms.insertUniform(
      "u_segTexture_T_world", UniformType::Mat4Vector, Mat4Vector{sk_identMat4, sk_identMat4}
    );

    auto vs = std::make_shared<GLShader>("vsDiff", ShaderType::Vertex, vsSource.c_str());
    vs->setRegisteredUniforms(std::move(vsUniforms));
    program.attachShader(vs);

    spdlog::debug("Compiled vertex shader {}", vsFileName);
  }

  {
    Uniforms fsUniforms;

    fsUniforms.insertUniform("u_imgTex", UniformType::SamplerVector, msk_imgTexSamplers);
    fsUniforms.insertUniform("u_segTex", UniformType::SamplerVector, msk_segTexSamplers);
    fsUniforms.insertUniform("u_metricCmapTex", UniformType::Sampler, msk_metricCmapTexSampler);
    fsUniforms
      .insertUniform("u_segLabelCmapTex", UniformType::SamplerVector, msk_labelTableTexSamplers);

    // fsUniforms.insertUniform( "u_useTricubicInterpolation", UniformType::Bool, false );

    fsUniforms.insertUniform(
      "u_imgSlopeIntercept", UniformType::Vec2Vector, Vec2Vector{sk_zeroVec2, sk_zeroVec2}
    );
    fsUniforms.insertUniform("u_segOpacity", UniformType::FloatVector, FloatVector{0.0f, 0.0f});

    fsUniforms.insertUniform("u_segInteriorOpacity", UniformType::Float, 1.0f);
    fsUniforms.insertUniform(
      "u_texSamplingDirsForSegOutline", UniformType::Vec3Vector, Vec3Vector{sk_zeroVec3}
    );

    fsUniforms.insertUniform("u_metricCmapSlopeIntercept", UniformType::Vec2, sk_zeroVec2);
    fsUniforms.insertUniform("u_metricSlopeIntercept", UniformType::Vec2, sk_zeroVec2);
    fsUniforms.insertUniform("u_metricMasking", UniformType::Bool, false);

    fsUniforms.insertUniform("u_useSquare", UniformType::Bool, true);

    // For intensity projection:
    // 0: none, 1: max, 2: mean, 3: min, 4: xray
    fsUniforms.insertUniform("u_mipMode", UniformType::Int, 0);
    fsUniforms.insertUniform("u_halfNumMipSamples", UniformType::Int, 0);
    fsUniforms.insertUniform("u_texSamplingDirZ", UniformType::Vec3, sk_zeroVec3);
    fsUniforms.insertUniform("img1Tex_T_img0Tex", UniformType::Mat4, sk_identMat4);

    auto fs = std::make_shared<GLShader>("fsDiff", ShaderType::Fragment, fsSource.c_str());
    fs->setRegisteredUniforms(std::move(fsUniforms));
    program.attachShader(fs);

    spdlog::debug("Compiled fragment shader {}", fsFileName);
  }

  if (!program.link())
  {
    spdlog::critical("Failed to link shader program {}", program.name());
    return false;
  }

  spdlog::debug("Linked shader program {}", program.name());
  return true;
}

bool Rendering::createCrossCorrelationProgram(GLShaderProgram& program)
{
  static const std::string vsFileName{"src/rendering/shaders/Metric.vs"};
  static const std::string fsFileName{"src/rendering/shaders/Correlation.fs"};

  auto filesystem = cmrc::shaders::get_filesystem();
  std::string vsSource;
  std::string fsSource;

  try
  {
    cmrc::file vsData = filesystem.open(vsFileName.c_str());
    cmrc::file fsData = filesystem.open(fsFileName.c_str());

    vsSource = std::string(vsData.begin(), vsData.end());
    fsSource = std::string(fsData.begin(), fsData.end());
  }
  catch (const std::exception& e)
  {
    spdlog::critical("Exception when loading shader file: {}", e.what());
    throw_debug("Unable to load shader")
  }

  {
    Uniforms vsUniforms;
    vsUniforms.insertUniform("u_view_T_clip", UniformType::Mat4, sk_identMat4);
    vsUniforms.insertUniform("u_world_T_clip", UniformType::Mat4, sk_identMat4);
    vsUniforms.insertUniform("u_clipDepth", UniformType::Float, 0.0f);

    vsUniforms.insertUniform(
      "u_imgTexture_T_world", UniformType::Mat4Vector, Mat4Vector{sk_identMat4, sk_identMat4}
    );
    vsUniforms.insertUniform(
      "u_segTexture_T_world", UniformType::Mat4Vector, Mat4Vector{sk_identMat4, sk_identMat4}
    );

    auto vs = std::make_shared<GLShader>("vsCorr", ShaderType::Vertex, vsSource.c_str());
    vs->setRegisteredUniforms(std::move(vsUniforms));
    program.attachShader(vs);

    spdlog::debug("Compiled vertex shader {}", vsFileName);
  }

  {
    Uniforms fsUniforms;

    fsUniforms.insertUniform("u_imgTex", UniformType::SamplerVector, msk_imgTexSamplers);
    fsUniforms.insertUniform("u_segTex", UniformType::SamplerVector, msk_segTexSamplers);
    fsUniforms.insertUniform("u_metricCmapTex", UniformType::Sampler, msk_metricCmapTexSampler);
    fsUniforms
      .insertUniform("u_segLabelCmapTex", UniformType::SamplerVector, msk_labelTableTexSamplers);

    // fsUniforms.insertUniform( "u_useTricubicInterpolation", UniformType::Bool, false );

    fsUniforms.insertUniform("u_segOpacity", UniformType::FloatVector, FloatVector{0.0f, 0.0f});

    fsUniforms.insertUniform("u_segInteriorOpacity", UniformType::Float, 1.0f);
    fsUniforms.insertUniform(
      "u_texSamplingDirsForSegOutline", UniformType::Vec3Vector, Vec3Vector{sk_zeroVec3}
    );

    fsUniforms.insertUniform("u_metricCmapSlopeIntercept", UniformType::Vec2, sk_zeroVec2);
    fsUniforms.insertUniform("u_metricSlopeIntercept", UniformType::Vec2, sk_zeroVec2);
    fsUniforms.insertUniform("u_metricMasking", UniformType::Bool, false);

    fsUniforms.insertUniform("u_texture1_T_texture0", UniformType::Mat4, sk_identMat4);
    fsUniforms.insertUniform("u_tex0SamplingDirX", UniformType::Vec3, sk_zeroVec3);
    fsUniforms.insertUniform("u_tex0SamplingDirY", UniformType::Vec3, sk_zeroVec3);

    auto fs = std::make_shared<GLShader>("fsCorr", ShaderType::Fragment, fsSource.c_str());
    fs->setRegisteredUniforms(std::move(fsUniforms));
    program.attachShader(fs);

    spdlog::debug("Compiled fragment shader {}", fsFileName);
  }

  if (!program.link())
  {
    spdlog::critical("Failed to link shader program {}", program.name());
    return false;
  }

  spdlog::debug("Linked shader program {}", program.name());
  return true;
}

bool Rendering::createSimpleProgram(GLShaderProgram& program)
{
  auto filesystem = cmrc::shaders::get_filesystem();
  std::string vsSource;
  std::string fsSource;

  try
  {
    cmrc::file vsData = filesystem.open("src/rendering/shaders/Simple.vs");
    cmrc::file fsData = filesystem.open("src/rendering/shaders/Simple.fs");

    vsSource = std::string(vsData.begin(), vsData.end());
    fsSource = std::string(fsData.begin(), fsData.end());
  }
  catch (const std::exception& e)
  {
    spdlog::critical("Exception when loading shader file: {}", e.what());
    throw_debug("Unable to load shader")
  }

  {
    Uniforms vsUniforms;
    vsUniforms.insertUniform("u_view_T_clip", UniformType::Mat4, sk_identMat4);
    vsUniforms.insertUniform("u_clipDepth", UniformType::Float, 0.0f);
    vsUniforms.insertUniform("u_clipMin", UniformType::Float, 0.0f);
    vsUniforms.insertUniform("u_clipMax", UniformType::Float, 0.0f);

    auto vs = std::make_shared<GLShader>("vsSimple", ShaderType::Vertex, vsSource.c_str());
    vs->setRegisteredUniforms(std::move(vsUniforms));
    program.attachShader(vs);
    spdlog::debug("Compiled simple vertex shader");
  }

  {
    Uniforms fsUniforms;
    fsUniforms.insertUniform("color", UniformType::Vec4, glm::vec4{0.0f, 0.0f, 0.0f, 1.0f});

    auto fs = std::make_shared<GLShader>("fsSimple", ShaderType::Fragment, fsSource.c_str());
    fs->setRegisteredUniforms(std::move(fsUniforms));
    program.attachShader(fs);
    spdlog::debug("Compiled simple fragment shader");
  }

  if (!program.link())
  {
    spdlog::critical("Failed to link shader program {}", program.name());
    return false;
  }

  spdlog::debug("Linked shader program {}", program.name());
  return true;
}

bool Rendering::showVectorOverlays() const
{
  return m_showOverlays;
}
void Rendering::setShowVectorOverlays(bool show)
{
  m_showOverlays = show;
}

void Rendering::updateIsosurfaceDataFor2d(AppData& appData, const uuids::uuid& imageUid)
{
  auto& isoData = appData.renderData().m_isosurfaceData;
  const Image* image = appData.image(imageUid);
  const ImageSettings& settings = image->settings();

  // Turn off all of the isosurfaces
  std::fill(std::begin(isoData.opacities), std::end(isoData.opacities), 0.0f);

  // Set width of isovalue threshold as a percentage of the image intensity range:
  const double w = settings.isosurfaceWidthIn2d()
                   * (settings.minMaxImageRange().second - settings.minMaxImageRange().first)
                   / 100.0;

  isoData.widthIn2d = std::max(
    1.0e-4f,
    static_cast<float>(
      settings.mapNativeIntensityToTexture(w) - settings.mapNativeIntensityToTexture(0.0)
    )
  );

  if (!settings.showIsosurfacesIn2d() || !settings.isosurfacesVisible())
  {
    return;
  }

  const uint32_t activeComp = settings.activeComponent();

  size_t i = 0;

  for (const auto& surfaceUid : appData.isosurfaceUids(imageUid, activeComp))
  {
    if (i >= RenderData::IsosurfaceData::MAX_NUM_ISOSURFACES)
    {
      // Only render the first MAX_NUM_ISOSURFACES surfaces
      break;
    }

    const Isosurface* surface = m_appData.isosurface(imageUid, activeComp, surfaceUid);

    if (!surface)
    {
      spdlog::warn("Null isosurface {} for image {}", surfaceUid, imageUid);
      continue;
    }

    if (!surface->visible)
    {
      continue;
    }

    // Map isovalue from native image intensity to texture intensity:
    const double texValue = settings.mapNativeIntensityToTexture(surface->value);

    isoData.values[i] = static_cast<float>(texValue);

    // The isolines are hidden if the image is hidden
    isoData.opacities[i] = settings.visibility()
                             ? surface->opacity * settings.isosurfaceOpacityModulator()
                             : 0.0f;

    if (settings.applyImageColormapToIsosurfaces())
    {
      /// @note This case is only needed when the image is transparent, since otherwise the
      /// isoline color is the same as the image color
      isoData.colors[i] = getIsosurfaceColor(m_appData, *surface, settings, activeComp);
    }
    else
    {
      // Color the surface using its explicitly defined color:
      isoData.colors[i] = surface->color;
    }

    ++i;
  }
}

void Rendering::updateIsosurfaceDataFor3d(AppData& appData, const uuids::uuid& imageUid)
{
  auto& isoData = appData.renderData().m_isosurfaceData;
  const Image* image = appData.image(imageUid);
  const ImageSettings& settings = image->settings();

  // Turn off all of the isosurfaces
  std::fill(std::begin(isoData.opacities), std::end(isoData.opacities), 0.0f);

  if (!settings.isosurfacesVisible())
  {
    return;
  }

  const uint32_t activeComp = settings.activeComponent();

  size_t i = 0;

  for (const auto& surfaceUid : appData.isosurfaceUids(imageUid, activeComp))
  {
    if (i >= RenderData::IsosurfaceData::MAX_NUM_ISOSURFACES)
    {
      // Only render the first MAX_NUM_ISOSURFACES surfaces
      break;
    }

    const Isosurface* surface = m_appData.isosurface(imageUid, activeComp, surfaceUid);

    if (!surface)
    {
      spdlog::warn("Null isosurface {} for image {}", surfaceUid, imageUid);
      continue;
    }

    if (!surface->visible)
    {
      continue;
    }

    // Map isovalue from native image intensity to texture intensity:
    const double texValue = settings.mapNativeIntensityToTexture(surface->value);

    isoData.values[i] = static_cast<float>(texValue);

    // The isosurfaces are hidden if the image is hidden
    isoData.opacities[i] = settings.visibility()
                             ? surface->opacity * settings.isosurfaceOpacityModulator()
                             : 0.0f;

    isoData.edgeStrengths[i] = surface->edgeStrength;
    isoData.shininesses[i] = surface->material.shininess;

    if (settings.applyImageColormapToIsosurfaces())
    {
      // Color the surface using the current image colormap:
      const glm::vec3 cmapColor = getIsosurfaceColor(m_appData, *surface, settings, activeComp);
      isoData.ambientLights[i] = surface->material.ambient * cmapColor;
      isoData.diffuseLights[i] = surface->material.diffuse * cmapColor;
      isoData.specularLights[i] = surface->material.specular * WHITE;
    }
    else
    {
      // Color the surface using its explicitly defined color:
      isoData.ambientLights[i] = surface->ambientColor();
      isoData.diffuseLights[i] = surface->diffuseColor();
      isoData.specularLights[i] = surface->specularColor();
    }

    ++i;
  }
}
