#include "rendering/Rendering.h"

#include "common/Exception.hpp"
#include "common/Types.h"

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

// #include "rendering/renderers/DepthPeelRenderer.h"
// #include "rendering/utility/CreateGLObjects.h"
// #include "rendering_old/utility/containers/BlankTextures.h"
// #include "rendering/utility/vtk/PolyDataGenerator.h"

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

#include <chrono>
#include <expected>
#include <functional>
#include <list>
#include <memory>
#include <thread>
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
using namespace uuids;

const glm::vec3 WHITE{1.0f};
const glm::mat4 sk_identMat3{1.0f};
const glm::mat4 sk_identMat4{1.0f};
const glm::vec2 sk_zeroVec2{0.0f, 0.0f};
const glm::vec3 sk_zeroVec3{0.0f, 0.0f, 0.0f};
const glm::vec4 sk_zeroVec4{0.0f, 0.0f, 0.0f, 0.0f};
const glm::ivec2 sk_zeroIVec2{0, 0};

/// @note OpenGL should have a at least a minimum of 16 texture units

// Samplers for grayscale image shaders:
const Uniforms::SamplerIndexType msk_imgTexSampler{0}; // one image
const Uniforms::SamplerIndexType msk_imgCmapTexSampler{1}; // one image colormap

// Samplers for color image shaders:
const Uniforms::SamplerIndexVectorType msk_imgRgbaTexSamplers{{0, 1, 2, 3}}; // Four (RGBA) images

// Samplers for segmentation shaders:
const Uniforms::SamplerIndexType msk_segTexSampler{0}; // one segmentation
const Uniforms::SamplerIndexType msk_segLabelTableTexSampler{1}; // one label table

// Sampler for volume rendering shader:
const Uniforms::SamplerIndexType msk_jumpTexSampler{4}; // distance map texture

/// @todo Change these to account for segs being in their own shader:
// Samplers for metric shaders:
const Uniforms::SamplerIndexVectorType msk_metricImgTexSamplers{{0, 1}};
const Uniforms::SamplerIndexType msk_metricCmapTexSampler{2};

std::string loadFile(const std::string& path)
{
  const auto filesystem = cmrc::shaders::get_filesystem();
  const cmrc::file data = filesystem.open(path.c_str());
  return std::string(data.begin(), data.end());
}

/**
 * @brief Replace placeholders in source string
 * @param source
 * @param placeholdersToStringMap Map of placeholders to replacement strings
 * @return Source string with placeholders replaced
 */
std::string replacePlaceholders(
  const std::string& source,
  const std::unordered_map<std::string, std::string>& placeholdersToStringMap)
{
  std::string result = source;
  for (const auto& [placeholder, replacement] : placeholdersToStringMap) {
    std::size_t pos = 0;
    while ((pos = result.find(placeholder, pos)) != std::string::npos)
    {
      result.replace(pos, placeholder.length(), replacement);
      pos += replacement.length();
    }
  }
  return result;
}

std::expected<std::unique_ptr<GLShaderProgram>, std::string>
createShaderProgram(
  const std::string& programName,
  const std::string& vsName, const std::string& fsName,
  const std::unordered_map<std::string, std::string>& fsReplacements,
  const Uniforms& vsUniforms, const Uniforms& fsUniforms)
{
  static const std::string shaderPath("src/rendering/shaders/");

  const auto filesystem = cmrc::shaders::get_filesystem();
  std::string vsSource;
  std::string fsSource;

  try {
    const cmrc::file vsData = filesystem.open(shaderPath + vsName);
    const cmrc::file fsData = filesystem.open(shaderPath + fsName);
    vsSource = std::string(vsData.begin(), vsData.end());
    fsSource = std::string(fsData.begin(), fsData.end());
  }
  catch (const std::exception& e) {
    return std::unexpected(std::format("Exception loading shader for program {}: {}", programName, e.what()));
  }

  fsSource = replacePlaceholders(fsSource, fsReplacements);

  auto vs = std::make_shared<GLShader>(vsName, ShaderType::Vertex, vsSource.c_str());
  vs->setRegisteredUniforms(vsUniforms);

  auto fs = std::make_shared<GLShader>(fsName, ShaderType::Fragment, fsSource.c_str());
  fs->setRegisteredUniforms(fsUniforms);

  auto program = std::make_unique<GLShaderProgram>(programName);

  if (!program->attachShader(vs)) {
    return std::unexpected(std::format("Unable to compile vertex shader {}", vsName));
  }
  spdlog::debug("Compiled vertex shader {}", vsName);

  if (!program->attachShader(fs)) {
    return std::unexpected(std::format("Unable to compile fragment shader {}", fsName));
  }
  spdlog::debug("Compiled fragment shader {}", fsName);

  if (!program->link()) {
    return std::unexpected(std::format("Failed to link shader program {}", programName));
  }

  spdlog::debug("Linked shader program {}", programName);
  return program;
}

/**
 * @brief Create the Dual-Depth Peel renderer for a given view
 * @param viewUid View UID
 * @param shaderActivator Function that activates shader programs
 * @param uniformsProvider Function returning the uniforms
 * @return Unique pointer to the renderer
 */
#if 0
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
#endif
} // namespace

Rendering::Rendering(AppData& appData)
  : m_appData(appData)
  , m_nvg(nvgCreateGL3(NVG_ANTIALIAS | NVG_STENCIL_STROKES /*| NVG_DEBUG*/))
  , m_raycastIsoSurfaceProgram("RayCastIsoSurfaceProgram")
  , m_isAppDoneLoadingImages(false)
  , m_showOverlays(true)
{
  static const std::string ROBOTO_LIGHT("robotoLight");

  if (!m_nvg) {
    spdlog::error("Could not initialize 'nanovg' vector graphcis library. "
                  "Proceeding without vector graphics.");
  }

  try
  {
    // Load the font for anatomical labels:
    auto filesystem = cmrc::fonts::get_filesystem();
    const cmrc::file robotoFont = filesystem.open("resources/fonts/Roboto/Roboto-Light.ttf");

    const int robotoLightFont = nvgCreateFontMem(
      m_nvg, ROBOTO_LIGHT.c_str(),
      reinterpret_cast<uint8_t*>(const_cast<char*>(robotoFont.begin())),
      static_cast<int32_t>(robotoFont.size()), 0);

    if (-1 == robotoLightFont) {
      spdlog::error("Could not load font {}", ROBOTO_LIGHT);
    }
  }
  catch (const std::exception& e) {
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

  /*
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
*/
}

Rendering::~Rendering()
{
  if (m_nvg) {
    nvgDeleteGL3(m_nvg);
    m_nvg = nullptr;
  }
}

void Rendering::setupOpenGLState()
{
  glEnable(GL_BLEND);
  glDisable(GL_CULL_FACE);
  glDisable(GL_DEPTH_TEST);
  glEnable(GL_MULTISAMPLE);
  glDisable(GL_SCISSOR_TEST);
  glEnable(GL_STENCIL_TEST);

  glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
  glFrontFace(GL_CCW);

  /// @todo This check should be done only once at initialization:
  /*
  GLint encoding;
  glGetFramebufferAttachmentParameteriv(
    GL_FRAMEBUFFER, GL_BACK_LEFT, GL_FRAMEBUFFER_ATTACHMENT_COLOR_ENCODING, &encoding);

  const std::string sRGBCapable = (encoding == GL_SRGB)
    ? "Yes, framebuffer is sRGB" : "No, framebuffer is linear";
  spdlog::warn("Check whether framebuffer is sRGB-capable: {}", sRGBCapable);
  */

  // Enables gamma correction, but only if the framebuffer is sRGB-capable:
  // glEnable(GL_FRAMEBUFFER_SRGB);

/*
On macOS, the system may be gamma-correcting, even if we don't ask for it.

Even though the OpenGL framebuffer is linear, and shader outputs linear color, the colors look correct
because macOS applies gamma correction when compositing the OpenGL output to the screen.

1. macOS Uses a Color-Managed Windowing System
When OpenGL draws to a window (NSView-backed), macOS treats the framebuffer as being in linear space.
macOS then automatically applies the correct color space transform when compositing the window to the
display, which expects sRGB.

So your linear output is gamma corrected by the OS, not OpenGL. This is different from most platforms
(e.g., Windows or Linux), where the GPU outputs pixels directly to the screen without OS-level color
correction unless you manage it yourself.

2. The Retina Display Pipeline Is Fully Color-Managed
On Retina MacBooks, the entire rendering pipeline — from OpenGL → CALayer → Quartz Compositor → Display
— is color-aware. This includes proper application of ICC profiles and conversion to display-referred sRGB.

You might read:

"If you're rendering to a linear framebuffer, you must gamma correct in the shader."
But on macOS, that advice does not always apply, because the OS is silently helping you.

There is no official way to completely disable macOS’s automatic color correction for an OpenGL window
created via standard means like GLFW or NSOpenGLView.

macOS assumes that OpenGL window surfaces are in linear space, and it automatically applies color space
conversion to sRGB (or whatever the display’s ICC profile requires) when compositing your app’s window onto the screen.

3 Ways to Avoid Double Gamma Correction:

Option 1: Don’t Gamma Correct in Shader on macOS
Let macOS handle it:
#ifdef __APPLE__
    fragColor = vec4(intensity); // Linear output only
#else
    float gamma = 1.0 / 2.2;
    fragColor = vec4(pow(intensity, gamma)); // Manual gamma correction
#endif

Option 3: Render to a linear framebuffer texture and show it with a color-managed blit pass
This is the most robust cross-platform strategy.

Render your grayscale image to a linear float texture (e.g., GL_R16F)
Then, in a second pass, apply gamma correction and draw a fullscreen quad to your main window
On macOS, skip gamma correction in the second pass
Again, platform-specific, but cleanly separates concerns:

bool needsGammaCorrection =
#ifdef __APPLE__
    false;
#else
    true;
#endif


To make rendering look visually consistent across platforms (macOS, Windows, Linux),
especially for grayscale or color medical images, you must take control of the entire gamma correction path
— both in shader outputs and framebuffer setup — instead of relying on platform-specific behavior
like macOS's automatic color correction.

1. Take Full Control: Always Output Gamma-Corrected Color

Strategy:
Treat your output as linear in the shader
Always apply gamma correction yourself
Always render to a linear framebuffer
This ensures consistent appearance whether the OS applies color correction or not

Shader Output:
float linearGray = ...; // e.g., 0.0 to 1.0 linear intensity
float gamma = 1.0 / 2.2; // sRGB gamma
float corrected = pow(linearGray, gamma);
fragColor = vec4(corrected, corrected, corrected, 1.0);

2. Use a Linear Framebuffer (default on all platforms)
By default, the main framebuffer is not sRGB-correcting unless explicitly requested —
and on platforms like Windows/Linux, you get raw linear output to the screen.

So: just don’t enable GL_FRAMEBUFFER_SRGB
Don’t request GL_SRGB_CAPABLE framebuffer
Output gamma-corrected colors from shader as shown above
This gives you:

Linear framebuffer
Manual gamma correction
Platform-independent appearance

*/



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

  if (m_appData.renderData().m_labelBufferTextures.empty()) {
    spdlog::critical("No label buffer textures loaded");
    throw_debug("No label buffer textures loaded")
  }

  m_appData.renderData().m_colormapTextures = createImageColorMapTextures(m_appData);

  if (m_appData.renderData().m_colormapTextures.empty()) {
    spdlog::critical("No image color map textures loaded");
    throw_debug("No image color map textures loaded")
  }

  const std::vector<uuid> imageUidsOfCreatedTextures =
    createImageTextures(m_appData, m_appData.imageUidsOrdered());

  if (imageUidsOfCreatedTextures.size() != m_appData.numImages()) {
    spdlog::error("Not all image textures were created");
    /// @todo remove the images for which the texture was not created
  }

  const std::vector<uuid> segUidsOfCreatedTextures =
    createSegTextures(m_appData, m_appData.segUidsOrdered());

  if (segUidsOfCreatedTextures.size() != m_appData.numSegs()) {
    spdlog::error("Not all segmentation textures were created");
    /// @todo remove the segs for which the texture was not created
  }

  m_appData.renderData().m_distanceMapTextures = createDistanceMapTextures(m_appData);

  m_isAppDoneLoadingImages = true;
}

bool Rendering::createLabelColorTableTexture(const uuid& labelTableUid)
{
  // static const glm::vec4 sk_border{ 0.0f, 0.0f, 0.0f, 0.0f };

  const auto* table = m_appData.labelTable(labelTableUid);
  if (!table) {
    spdlog::warn("Label table {} is invalid", labelTableUid);
    return false;
  }

  int maxBufTexSize;
  glGetIntegerv(GL_MAX_TEXTURE_BUFFER_SIZE, &maxBufTexSize);

  if (table->numColorBytes_RGBA_U8() > static_cast<size_t>(maxBufTexSize)) {
    spdlog::error(
      "Number of bytes ({}) in label color table {} exceeds "
      "maximum buffer texture size of {} bytes",
      table->numColorBytes_RGBA_U8(), labelTableUid, maxBufTexSize);
    return false;
  }

  auto it = m_appData.renderData().m_labelBufferTextures.emplace(
    std::piecewise_construct,
    std::forward_as_tuple(labelTableUid),
    std::forward_as_tuple(table->bufferTextureFormat_RGBA_U8(), BufferUsagePattern::StaticDraw));

  if (!it.second) {
    return false;
  }

  GLBufferTexture& T = it.first->second;
  T.generate();
  T.allocate(table->numColorBytes_RGBA_U8(), table->colorData_RGBA_nonpremult_U8());

  spdlog::debug("Generated buffer texture for label color table {}", labelTableUid);
  return true;
}

bool Rendering::removeSegTexture(const uuid& segUid)
{
  const auto* seg = m_appData.seg(segUid);
  if (!seg) {
    spdlog::warn("Segmentation {} is invalid", segUid);
    return false;
  }

  auto it = m_appData.renderData().m_segTextures.find(segUid);

  if (std::end(m_appData.renderData().m_segTextures) == it) {
    spdlog::warn("Texture for segmentation {} does not exist and cannot be removed", segUid);
    return false;
  }

  m_appData.renderData().m_segTextures.erase(it);
  return true;
}

/*
bool Rendering::createImageTexture( const uuid& imageUid )
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
  const uuid& segUid,
  const ComponentType& compType,
  const glm::uvec3& startOffsetVoxel,
  const glm::uvec3& sizeInVoxels,
  const void* data)
{
  // Load seg data into first mipmap level
  static constexpr GLint sk_mipmapLevel = 0;

  auto it = m_appData.renderData().m_segTextures.find(segUid);
  if (std::end(m_appData.renderData().m_segTextures) == it) {
    spdlog::error("Cannot update segmentation {}: texture not found.", segUid);
    return;
  }

  const auto* seg = m_appData.seg(segUid);
  if (!seg) {
    spdlog::warn("Segmentation {} is invalid", segUid);
    return;
  }

  GLTexture& T = it->second;
  T.setSubData(sk_mipmapLevel, startOffsetVoxel, sizeInVoxels,
               GLTexture::getBufferPixelRedFormat(compType),
               GLTexture::getBufferPixelDataType(compType), data);
}

void Rendering::updateSegTextureWithInt64Data(
  const uuid& segUid,
  const ComponentType& compType,
  const glm::uvec3& startOffsetVoxel,
  const glm::uvec3& sizeInVoxels,
  const int64_t* data)
{
  if (!data) {
    spdlog::error("Null segmentation texture data pointer");
    return;
  }

  if (!isValidSegmentationComponentType(compType)) {
    spdlog::error(
      "Unable to update segmentation texture using buffer with invalid component type {}",
      componentTypeString(compType));
    return;
  }

  const size_t N = static_cast<size_t>(sizeInVoxels.x) * static_cast<size_t>(sizeInVoxels.y)
                   * static_cast<size_t>(sizeInVoxels.z);

  switch (compType)
  {
  case ComponentType::UInt8: {
    const std::vector<uint8_t> castData(data, data + N);
    return updateSegTexture(segUid, compType, startOffsetVoxel, sizeInVoxels, static_cast<const void*>(castData.data()));
  }
  case ComponentType::UInt16: {
    const std::vector<uint16_t> castData(data, data + N);
    return updateSegTexture(segUid, compType, startOffsetVoxel, sizeInVoxels, static_cast<const void*>(castData.data()));
  }
  case ComponentType::UInt32: {
    const std::vector<uint32_t> castData(data, data + N);
    return updateSegTexture(segUid, compType, startOffsetVoxel, sizeInVoxels, static_cast<const void*>(castData.data()));
  }
  default: {
    return;
  }
  }
}

/// @todo Need to fix this to handle multicomponent images like
/// std::vector<uuid> createImageTextures( AppData& appData, uuid_range_t imageUids )
void Rendering::updateImageTexture(
  const uuid& imageUid,
  uint32_t comp,
  const ComponentType& compType,
  const glm::uvec3& startOffsetVoxel,
  const glm::uvec3& sizeInVoxels,
  const void* data)
{
  // Load data into first mipmap level
  static constexpr GLint sk_mipmapLevel = 0;

  auto it = m_appData.renderData().m_imageTextures.find(imageUid);
  if (std::end(m_appData.renderData().m_imageTextures) == it) {
    spdlog::error("Cannot update image {}: texture not found.", imageUid);
    return;
  }

  std::vector<GLTexture>& T = it->second;
  if (comp >= T.size()) {
    spdlog::error("Cannot update invalid component {} of image {}", comp, imageUid);
    return;
  }

  const auto* img = m_appData.image(imageUid);
  if (!img) {
    spdlog::warn("Segmentation {} is invalid", imageUid);
    return;
  }

  T.at(comp).setSubData(sk_mipmapLevel, startOffsetVoxel, sizeInVoxels,
                        GLTexture::getBufferPixelRedFormat(compType),
                        GLTexture::getBufferPixelDataType(compType), data);
}

Rendering::CurrentImages
Rendering::getImageAndSegUidsForMetricShaders(const std::list<uuid>& metricImageUids) const
{
  const RenderData& R = m_appData.renderData();
  CurrentImages I;

  for (const auto& imageUid : metricImageUids)
  {
    if (I.size() >= NUM_METRIC_IMAGES) {
      break; // Stop after NUM_METRIC_IMAGES images reached
    }

    if (std::end(R.m_imageTextures) != R.m_imageTextures.find(imageUid))
    {
      ImgSegPair imgSegPair;
      imgSegPair.first = imageUid; // The texture for this image exists

      // Find the segmentation that belongs to this image
      if (const auto segUid = m_appData.imageToActiveSegUid(imageUid)) {
        if (std::end(R.m_segTextures) != R.m_segTextures.find(*segUid)) {
          imgSegPair.second = *segUid; // The texture for this segmentation exists
        }
      }

      I.emplace_back(std::move(imgSegPair));
    }
  }

  // Always return at least two elements
  while (I.size() < Rendering::NUM_METRIC_IMAGES) {
    I.emplace_back(ImgSegPair());
  }

  return I;
}

Rendering::CurrentImages
Rendering::getImageAndSegUidsForImageShaders(const std::list<uuid>& imageUids) const
{
  const RenderData& R = m_appData.renderData();
  CurrentImages I;

  for (const auto& imageUid : imageUids)
  {
    if (std::end(R.m_imageTextures) != R.m_imageTextures.find(imageUid))
    {
      std::pair<std::optional<uuid>, std::optional<uuid>> imgSegPair;
      imgSegPair.first = imageUid; // The texture for this image exists

      // Find the segmentation that belongs to this image
      if (const auto segUid = m_appData.imageToActiveSegUid(imageUid)) {
        if (std::end(R.m_segTextures) != R.m_segTextures.find(*segUid)) {
          imgSegPair.second = *segUid; // The texture for this segmentation exists
        }
      }

      I.emplace_back(std::move(imgSegPair));
    }
  }

  return I;
}

void Rendering::updateImageInterpolation(const uuid& imageUid)
{
  const auto* image = m_appData.image(imageUid);
  if (!image) {
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
    case InterpolationMode::NearestNeighbor: {
      minFilter = tex::MinificationFilter::Nearest;
      maxFilter = tex::MagnificationFilter::Nearest;
      break;
    }
    case InterpolationMode::Trilinear:
    case InterpolationMode::Tricubic: {
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
      case InterpolationMode::NearestNeighbor: {
        minFilter = tex::MinificationFilter::Nearest;
        maxFilter = tex::MagnificationFilter::Nearest;
        break;
      }
      case InterpolationMode::Trilinear:
      case InterpolationMode::Tricubic: {
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
  if (!cmapUid) {
    spdlog::warn("Image color map index {} is invalid", cmapIndex);
    return;
  }

  const auto* map = m_appData.imageColorMap(*cmapUid);
  if (!map) {
    spdlog::warn("Image color map {} is invalid", *cmapUid);
    return;
  }

  ImageColorMap* cmap = m_appData.imageColorMap(*cmapUid);
  if (!cmap) {
    spdlog::warn("Image color map {} is null", *cmapUid);
    return;
  }

  GLTexture& texture = m_appData.renderData().m_colormapTextures.at(*cmapUid);
  tex::MinificationFilter minFilter;
  tex::MagnificationFilter maxFilter;

  switch (cmap->interpolationMode())
  {
  case ImageColorMap::InterpolationMode::Nearest: {
    minFilter = tex::MinificationFilter::Nearest;
    maxFilter = tex::MagnificationFilter::Nearest;
    break;
  }
  case ImageColorMap::InterpolationMode::Linear: {
    minFilter = tex::MinificationFilter::Linear;
    maxFilter = tex::MagnificationFilter::Linear;
    break;
  }
  }

  texture.setMinificationFilter(minFilter);
  texture.setMagnificationFilter(maxFilter);

  spdlog::debug("Set interpolation mode for image color map {}", *cmapUid);
}

void Rendering::updateLabelColorTableTexture(std::size_t tableIndex)
{
  spdlog::trace("Begin updating texture for 1D label color map at index {}", tableIndex);

  if (tableIndex >= m_appData.numLabelTables()) {
    spdlog::error("Label color table at index {} does not exist", tableIndex);
    return;
  }

  const auto tableUid = m_appData.labelTableUid(tableIndex);
  if (!tableUid) {
    spdlog::error("Label table index {} is invalid", tableIndex);
    return;
  }

  const auto* table = m_appData.labelTable(*tableUid);
  if (!table) {
    spdlog::error("Label table {} is invalid", *tableUid);
    return;
  }

  auto it = m_appData.renderData().m_labelBufferTextures.find(*tableUid);
  if (std::end(m_appData.renderData().m_labelBufferTextures) == it) {
    spdlog::error("Buffer texture for label color table {} is invalid", *tableUid);
    return;
  }

  it->second.write(0, table->numColorBytes_RGBA_U8(), table->colorData_RGBA_nonpremult_U8());
  spdlog::trace("Done updating buffer texture for label color table {}", *tableUid);
}

void Rendering::framerateLimiter(std::chrono::time_point<Clock>& lastFrameTime)
{
  if (!m_appData.renderData().m_manualFramerateLimiter) {
    return;
  }

  const double elapsed = std::chrono::duration<double>(Clock::now() - lastFrameTime).count();
  const double targetTime = m_appData.renderData().m_targetFrameTimeSeconds;

  if (elapsed < targetTime) {
    std::this_thread::sleep_for(std::chrono::duration<double>(targetTime - elapsed));
  }

  lastFrameTime = Clock::now();
}

void Rendering::render()
{
  // Set up OpenGL state, because it changes after NanoVG calls in the render of the prior frame
  setupOpenGLState();

  // Set the OpenGL viewport in device units:
  const glm::ivec4 deviceViewport = m_appData.windowData().viewport().getDeviceAsVec4();
  glViewport(deviceViewport[0], deviceViewport[1], deviceViewport[2], deviceViewport[3]);

  const auto& bg = m_appData.renderData().m_2dBackgroundColor;
  glClearColor(bg.r, bg.g, bg.b, 1.0f);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

  renderImageData();
  renderVectorOverlays();
}

void Rendering::updateImageUniforms(uuid_range_t imageUids)
{
  for (const auto& imageUid : imageUids) {
    updateImageUniforms(imageUid);
  }
}

void Rendering::updateImageUniforms(const uuid& imageUid)
{
  auto it = m_appData.renderData().m_uniforms.find(imageUid);

  if (std::end(m_appData.renderData().m_uniforms) == it) {
    spdlog::debug("Adding rendering uniforms for image {}", imageUid);
    m_appData.renderData().m_uniforms.insert(std::make_pair(imageUid, RenderData::ImageUniforms()));
  }

  RenderData::ImageUniforms& uniforms = m_appData.renderData().m_uniforms[imageUid];
  Image* img = m_appData.image(imageUid);
  if (!img) {
    uniforms.imgOpacity = 0.0f;
    uniforms.segOpacity = 0.0f;
    uniforms.showEdges = false;
    spdlog::error("Image {} is null on updating its uniforms; setting default uniform values", imageUid);
    return;
  }

  const auto& imgSettings = img->settings();

  uniforms.cmapQuantLevels = imgSettings.colorMapContinuous() ? 0 : imgSettings.colorMapQuantizationLevels();
  uniforms.hsvModFactors = imgSettings.colorMapHsvModFactors();

  if (const auto cmapUid = m_appData.imageColorMapUid(imgSettings.colorMapIndex()))
  {
    if (const ImageColorMap* map = m_appData.imageColorMap(*cmapUid)) {
      uniforms.cmapSlopeIntercept = map->slopeIntercept(imgSettings.isColorMapInverted());

      // If the color map has nearest-neighbor interpolation, then do NOT quantize:
      if (ImageColorMap::InterpolationMode::Nearest == map->interpolationMode()) {
        uniforms.cmapQuantLevels = 0;
      }
    }
    else {
      spdlog::error("Null image color map {} on updating uniforms for image {}", *cmapUid, imageUid);
    }
  }
  else
  {
    spdlog::error("Invalid image color map at index {} on updating uniforms for image {}",
                  imgSettings.colorMapIndex(), imageUid);
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
      uniforms.slopeInterceptRgba_normalized_T_texture[i] = imgSettings.slopeInterceptVec2_normalized_T_texture(i);

      uniforms.thresholdsRgba[i] = glm::vec2{
        static_cast<float>(imgSettings.mapNativeIntensityToTexture(imgSettings.thresholds(i).first)),
        static_cast<float>(imgSettings.mapNativeIntensityToTexture(imgSettings.thresholds(i).second))};

      uniforms.minMaxRgba[i] = glm::vec2{
        static_cast<float>(imgSettings.mapNativeIntensityToTexture(imgSettings.minMaxImageRange(i).first)),
        static_cast<float>(imgSettings.mapNativeIntensityToTexture(imgSettings.minMaxImageRange(i).second))};

      uniforms.imgOpacityRgba[i] = static_cast<float>(
        ((imgSettings.globalVisibility() && imgSettings.visibility(i)) ? 1.0 : 0.0) *
        imgSettings.globalOpacity() * imgSettings.opacity(i));
    }

    if (3 == imgSettings.numComponents())
    {
      // These two will be ignored for RGB images:
      uniforms.slopeInterceptRgba_normalized_T_texture[3] = glm::vec2{1.0f, 0.0f};
      uniforms.thresholdsRgba[3] = glm::vec2{0.0f, 1.0f};
      uniforms.minMaxRgba[3] = glm::vec2{0.0f, 1.0f};

      uniforms.imgOpacityRgba[3] = static_cast<float>(
        (imgSettings.globalVisibility() ? 1.0 : 0.0) * imgSettings.globalOpacity());
    }
  }
  else
  {
    uniforms.slopeIntercept_normalized_T_texture = imgSettings.slopeInterceptVec2_normalized_T_texture();
  }

  uniforms.slope_native_T_texture = imgSettings.slope_native_T_texture();
  uniforms.largestSlopeIntercept = imgSettings.largestSlopeInterceptTextureVec2();

  const glm::vec3 dims = glm::vec3{img->header().pixelDimensions()};

  uniforms.textureGradientStep = glm::mat3{
    glm::vec3{1.0f / dims[0], 0.0f, 0.0f},
    glm::vec3{0.0f, 1.0f / dims[1], 0.0f},
    glm::vec3{0.0f, 0.0f, 1.0f / dims[2]}};

  uniforms.voxelSpacing = img->header().spacing();

  // Map the native thresholds to OpenGL texture values:
  uniforms.thresholds = glm::vec2{
    static_cast<float>(imgSettings.mapNativeIntensityToTexture(imgSettings.thresholds().first)),
    static_cast<float>(imgSettings.mapNativeIntensityToTexture(imgSettings.thresholds().second))};

  // Map the native image values to OpenGL texture values:
  uniforms.minMax = glm::vec2{
    static_cast<float>(imgSettings.mapNativeIntensityToTexture(imgSettings.minMaxImageRange().first)),
    static_cast<float>(imgSettings.mapNativeIntensityToTexture(imgSettings.minMaxImageRange().second))};

  uniforms.imgOpacity =
    (static_cast<float>(imgSettings.globalVisibility() && imgSettings.visibility()) ? 1.0 : 0.0)
      * imgSettings.opacity()
      * ((imgSettings.numComponents() > 0) ? imgSettings.globalOpacity() : 1.0f);

  // Edges
  uniforms.showEdges = imgSettings.showEdges();
  uniforms.thresholdEdges = imgSettings.thresholdEdges();
  uniforms.edgeMagnitude = static_cast<float>(imgSettings.edgeMagnitude());
  uniforms.useFreiChen = imgSettings.useFreiChen();
  uniforms.overlayEdges = imgSettings.overlayEdges();
  uniforms.colormapEdges = imgSettings.colormapEdges();
  uniforms.edgeColor = static_cast<float>(imgSettings.edgeOpacity()) * glm::vec4{imgSettings.edgeColor(), 1.0f};

  // The segmentation linked to this image:
  const auto& segUid = m_appData.imageToActiveSegUid(imageUid);
  if (!segUid) {
    // The image has no segmentation
    uniforms.segOpacity = 0.0f;
    return;
  }

  Image* seg = m_appData.seg(*segUid);
  if (!seg) {
    spdlog::error("Segmentation {} is null on updating uniforms for image {}", *segUid, imageUid);
    return;
  }

  // The texture_T_world transformation of the segmentation uses the manual affine component
  // (subject_T_worldDef) of the image.
  uniforms.segTexture_T_world = seg->transformations().texture_T_subject() * img->transformations().subject_T_worldDef();
  uniforms.segVoxel_T_world = seg->transformations().pixel_T_subject() * img->transformations().subject_T_worldDef();

  // Both the image and segmenation must have visibility true for the segmentation to be shown
  if (imgSettings.numComponents() > 1) {
    uniforms.segOpacity = static_cast<float>(
      ((seg->settings().visibility() && imgSettings.globalVisibility()) ? 1.0 : 0.0)
      * seg->settings().opacity());
  }
  else {
    uniforms.segOpacity = static_cast<float>(
      ((seg->settings().visibility() && imgSettings.visibility(0) && imgSettings.globalVisibility())
         ? 1.0 : 0.0) * seg->settings().opacity());
  }
}

void Rendering::updateMetricUniforms()
{
  auto update = [this](RenderData::MetricParams& params, const char* name)
  {
    if (const auto cmapUid = m_appData.imageColorMapUid(params.m_colorMapIndex)) {
      if (const auto* map = m_appData.imageColorMap(*cmapUid)) {
        params.m_cmapSlopeIntercept = map->slopeIntercept(params.m_invertCmap);
      }
      else {
        spdlog::error("Null image color map {} on updating uniforms for {} metric", *cmapUid, name);
      }
    }
    else {
      spdlog::error("Invalid image color map at index {} on updating uniforms for {} metric",
                    params.m_colorMapIndex, name);
    }
  };

  update(m_appData.renderData().m_squaredDifferenceParams, "Difference");
  update(m_appData.renderData().m_crossCorrelationParams, "Cross-Correlation");
  update(m_appData.renderData().m_jointHistogramParams, "Joint Histogram");
}

std::list<std::reference_wrapper<GLTexture>>
Rendering::bindScalarImageTextures(const ImgSegPair& p)
{
  const auto& imgUid = p.first;
  const Image* image = (imgUid ? m_appData.image(*imgUid) : nullptr);

  std::list<std::reference_wrapper<GLTexture>> boundTextures;
  auto& R = m_appData.renderData();

  if (!image) {
    // No image, so bind the blank one:
    GLTexture& imgTex = R.m_blankImageBlackTransparentTexture;
    imgTex.bind(msk_imgTexSampler.index);
    boundTextures.push_back(imgTex);

    // Bind the first available colormap:
    auto it = std::begin(R.m_colormapTextures);
    GLTexture& cmapTex = it->second;
    cmapTex.bind(msk_imgCmapTexSampler.index);
    boundTextures.push_back(cmapTex);

    ///////////////////////////////// PUT THIS INTO SEPARATE BINDER THAT ONLY RUNS FOR VOLUME RENDERING
    // Also bind blank distance map:
    GLTexture& distTex = R.m_blankDistMapTexture;
    distTex.bind(msk_jumpTexSampler.index);
    boundTextures.push_back(distTex);
    ///////////////////////////////// PUT THIS INTO SEPARATE BINDER THAT ONLY RUNS FOR VOLUME RENDERING

    return boundTextures;
  }

  const ImageSettings& S = image->settings();

  // Uncomment this to render the image's distance map instead:
  // GLTexture& imgTex = R.m_distanceMapTextures.at(*imageUid).at(imgSettings.activeComponent());

  // Bind the active component of the image
  GLTexture& imgTex = R.m_imageTextures.at(*imgUid).at(S.activeComponent());
  imgTex.bind(msk_imgTexSampler.index);
  boundTextures.push_back(imgTex);

  // Bind the color map
  const auto cmapUid = image ? m_appData.imageColorMapUid(image->settings().colorMapIndex()) : std::nullopt;

  if (cmapUid) {
    GLTexture& cmapTex = R.m_colormapTextures.at(*cmapUid);
    cmapTex.bind(msk_imgCmapTexSampler.index);
    boundTextures.push_back(cmapTex);
  }
  else {
    // No colormap, so bind the first available one:
    auto it = std::begin(R.m_colormapTextures);
    GLTexture& cmapTex = it->second;
    cmapTex.bind(msk_imgCmapTexSampler.index);
    boundTextures.push_back(cmapTex);
  }

  ///////////////////////////////// PUT THIS INTO SEPARATE BINDER THAT ONLY RUNS FOR VOLUME RENDERING
  const bool useDistMap = S.useDistanceMapForRaycasting();
  bool foundMap = false;

  if (useDistMap)
  {
    const auto& distMaps = m_appData.distanceMaps(*imgUid, S.activeComponent());

    if (distMaps.empty()) {
      static bool alreadyShowedWarning = false;
      if (!alreadyShowedWarning) {
        spdlog::warn("No distance map for component {} of image {}", S.activeComponent(), *imgUid);
        alreadyShowedWarning = true;

        // Disable use of distance map for this image:
        if (Image* imageNonConst = (imgUid ? m_appData.image(*imgUid) : nullptr)) {
          imageNonConst->settings().setUseDistanceMapForRaycasting(false);
        }
      }
    }

    auto it = R.m_distanceMapTextures.find(*imgUid);
    if (std::end(R.m_distanceMapTextures) != it)
    {
      auto it2 = it->second.find(S.activeComponent());
      if (std::end(it->second) != it2) {
        foundMap = true;
        GLTexture& distTex = it2->second;
        distTex.bind(msk_jumpTexSampler.index);
        boundTextures.push_back(distTex);
      }
    }
  }

  if (!useDistMap || !foundMap) {
    // Bind blank (zero) distance map:
    GLTexture& distTex = R.m_blankDistMapTexture;
    distTex.bind(msk_jumpTexSampler.index);
    boundTextures.push_back(distTex);
  }
  ///////////////////////////////// PUT THIS INTO SEPARATE BINDER THAT ONLY RUNS FOR VOLUME RENDERING

  return boundTextures;
}

std::list<std::reference_wrapper<GLTexture>>
Rendering::bindColorImageTextures(const ImgSegPair& p)
{
  const auto& imgUid = p.first;
  const Image* image = (imgUid ? m_appData.image(*imgUid) : nullptr);

  auto& R = m_appData.renderData();
  std::list<std::reference_wrapper<GLTexture>> boundTextures;

  if (!image) {
    // No image, so bind the blank one:
    GLTexture& imgTex = R.m_blankImageBlackTransparentTexture;
    imgTex.bind(msk_imgTexSampler.index);
    boundTextures.push_back(imgTex);
    return boundTextures;
  }

  // Bind the four (RGBA) components:
  auto& compTextures = R.m_imageTextures.at(*imgUid);

  for (std::size_t i = 0; i < 4; ++i) {
    const bool compExists = (i < image->settings().numComponents() && i < compTextures.size());
    GLTexture& tex = compExists ? compTextures.at(i) : R.m_blankImageBlackTransparentTexture;
    tex.bind(msk_imgRgbaTexSamplers.indices[i]);
    boundTextures.push_back(tex);
  }

  return boundTextures;
}

std::list<std::reference_wrapper<GLTexture>>
Rendering::bindSegTextures(const ImgSegPair& p)
{
  const auto& segUid = p.second;

  std::list<std::reference_wrapper<GLTexture>> boundTextures;
  auto& R = m_appData.renderData();

  if (segUid) {
    // Uncomment this to render the image's distance map instead:
    // GLTexture& segTex = R.m_distanceMapTextures.at( *imageUid ).at( 0 );
    GLTexture& segTex = R.m_segTextures.at(*segUid);
    segTex.bind(msk_segTexSampler.index);
    boundTextures.push_back(segTex);
  }
  else {
    // No segmentation, so bind the blank one:
    GLTexture& segTex = R.m_blankSegTexture;
    segTex.bind(msk_segTexSampler.index);
    boundTextures.push_back(segTex);
  }

  return boundTextures;
}

void Rendering::unbindTextures(const std::list<std::reference_wrapper<GLTexture>>& textures)
{
  for (auto& T : textures) {
    T.get().unbind();
  }
}

/// @todo Do we need to take in the vector of image/seg IDs?
std::list<std::reference_wrapper<GLBufferTexture>>
Rendering::bindSegBufferTextures(const ImgSegPair& p)
{
  std::list<std::reference_wrapper<GLBufferTexture>> boundBufferTextures;
  auto& R = m_appData.renderData();

  const auto& segUid = p.second;

  const Image* seg = segUid ? m_appData.seg(*p.second) : nullptr;
  const auto tableUid = seg ? m_appData.labelTableUid(seg->settings().labelTableIndex()) : std::nullopt;

  if (tableUid) {
    GLBufferTexture& tblTex = R.m_labelBufferTextures.at(*tableUid);
    tblTex.bind(msk_segLabelTableTexSampler.index);
    tblTex.attachBufferToTexture(msk_segLabelTableTexSampler.index);
    boundBufferTextures.push_back(tblTex);
  }
  else {
    // No label table, so bind the first available one:
    auto it = std::begin(R.m_labelBufferTextures);
    GLBufferTexture& tblTex = it->second;
    tblTex.bind(msk_segLabelTableTexSampler.index);
    tblTex.attachBufferToTexture(msk_segLabelTableTexSampler.index);
    boundBufferTextures.push_back(tblTex);
  }

  return boundBufferTextures;
}

void Rendering::unbindBufferTextures(const std::list<std::reference_wrapper<GLBufferTexture> >& textures)
{
  for (auto& T : textures) {
    T.get().unbind();
  }
}

std::list<std::reference_wrapper<GLTexture>>
Rendering::bindMetricImageTextures(const CurrentImages& I, const ViewRenderMode& metricType)
{
  std::list<std::reference_wrapper<GLTexture>> textures;

  auto& R = m_appData.renderData();
  bool usesMetricColormap = false;
  std::size_t metricCmapIndex = 0;

  switch (metricType)
  {
  case ViewRenderMode::Difference: {
    usesMetricColormap = true;
    metricCmapIndex = R.m_squaredDifferenceParams.m_colorMapIndex;
    break;
  }
  /*
  case ViewRenderMode::CrossCorrelation: {
    usesMetricColormap = true;
    metricCmapIndex = R.m_crossCorrelationParams.m_colorMapIndex;
    break;
  }
  */
  case ViewRenderMode::JointHistogram: {
    usesMetricColormap = true;
    metricCmapIndex = R.m_jointHistogramParams.m_colorMapIndex;
    break;
  }
  case ViewRenderMode::Overlay: {
    usesMetricColormap = false;
    break;
  }
  case ViewRenderMode::Disabled: {
    return textures;
  }
  default: {
    spdlog::error("Invalid metric shader type {}", typeString(metricType));
    return textures;
  }
  }

  if (usesMetricColormap)
  {
    const auto cmapUid = m_appData.imageColorMapUid(metricCmapIndex);
    if (cmapUid) {
      GLTexture& T = R.m_colormapTextures.at(*cmapUid);
      T.bind(msk_metricCmapTexSampler.index);
      textures.push_back(T);
    }
    else {
      auto it = std::begin(R.m_colormapTextures);
      GLTexture& T = it->second;
      T.bind(msk_metricCmapTexSampler.index);
      textures.push_back(T);
    }
  }

  std::size_t i = 0;

  for (const auto& [imgUid, segUid] : I)
  {
    const Image* image = (imgUid ? m_appData.image(*imgUid) : nullptr);
    if (image) {
      // Bind the active component
      const uint32_t activeComp = image->settings().activeComponent();
      GLTexture& T = R.m_imageTextures.at(*imgUid).at(activeComp);
      T.bind(msk_metricImgTexSamplers.indices[i]);
      textures.push_back(T);
    }
    else {
      GLTexture& T = R.m_blankImageBlackTransparentTexture;
      T.bind(msk_metricImgTexSamplers.indices[i]);
      textures.push_back(T);
    }
    ++i;
  }

  return textures;
}

void Rendering::renderOneImage(
  const View& view,
  const glm::vec3& worldOffsetXhairs,
  GLShaderProgram& program,
  const CurrentImages& I,
  bool showEdges)
{
  auto getImage = [this](const std::optional<uuid>& imageUid) -> const Image*
  { return (imageUid ? m_appData.image(*imageUid) : nullptr); };

  auto& R = m_appData.renderData();

  drawImageQuad(program, view.renderMode(), R.m_quad, view, m_appData.windowData().viewport(), worldOffsetXhairs,
                R.m_flashlightRadius, R.m_flashlightOverlays, R.m_intensityProjectionSlabThickness,
                R.m_doMaxExtentIntensityProjection, R.m_xrayIntensityWindow, R.m_xrayIntensityLevel,
                I, getImage, showEdges);
}

void Rendering::renderOneImage_overlays(
  const View& view,
  const FrameBounds& miewportViewBounds,
  const glm::vec3& worldOffsetXhairs,
  const CurrentImages& I)
{
  auto& renderData = m_appData.renderData();

  if (!renderData.m_globalLandmarkParams.renderOnTopOfAllImagePlanes) {
    drawLandmarks(m_nvg, miewportViewBounds, worldOffsetXhairs, m_appData, view, I);
    setupOpenGLState();
  }

  if (!renderData.m_globalAnnotationParams.renderOnTopOfAllImagePlanes) {
    drawAnnotations(m_nvg, miewportViewBounds, worldOffsetXhairs, m_appData, view, I);
    setupOpenGLState();
  }

  drawImageViewIntersections(m_nvg, miewportViewBounds, worldOffsetXhairs, m_appData,
                             view, I,
                             renderData.m_globalSliceIntersectionParams.renderInactiveImageViewIntersections);

  setupOpenGLState();
}

void Rendering::volumeRenderOneImage(const View& view, GLShaderProgram& program, const CurrentImages& I)
{
  auto getImage = [this](const std::optional<uuid>& imageUid) -> const Image*
  { return (imageUid ? m_appData.image(*imageUid) : nullptr); };

  drawRaycastQuad(program, m_appData.renderData().m_quad, view, I, getImage);

  setupOpenGLState();
}

void Rendering::renderAllImages(
  const View& view, const FrameBounds& miewportViewBounds, const glm::vec3& worldOffsetXhairs)
{
  static const RenderData::ImageUniforms sk_defaultImageUniforms;
  const RenderData& R = m_appData.renderData();

  switch (getShaderGroup(view.renderMode()))
  {
  case ShaderGroup::Image:
  {
    const bool doXray = (IntensityProjectionMode::Xray == view.intensityProjectionMode());
    CurrentImages I;

    int displayModeUniform = 0;

    if (ViewRenderMode::Image == view.renderMode()) {
      displayModeUniform = 0;
      I = getImageAndSegUidsForImageShaders(view.renderedImages());
    }
    else if (ViewRenderMode::Checkerboard == view.renderMode()) {
      displayModeUniform = 1;
      I = getImageAndSegUidsForMetricShaders(view.metricImages()); // guaranteed size 2
    }
    else if (ViewRenderMode::Quadrants == view.renderMode()) {
      displayModeUniform = 2;
      I = getImageAndSegUidsForMetricShaders(view.metricImages());
    }
    else if (ViewRenderMode::Flashlight == view.renderMode()) {
      displayModeUniform = 3;
      I = getImageAndSegUidsForMetricShaders(view.metricImages());
    }

    // The first image in the stack is the fixed one:
    bool isFixedImage = true;

    for (const auto& imgSegPair : I)
    {
      if (!imgSegPair.first) {
        isFixedImage = false;
        continue;
      }

      const uuid& imgUid = *imgSegPair.first;
      const Image* img = m_appData.image(imgUid);
      if (!img) {
        spdlog::error("Null image during render");
        return;
      }

      const RenderData::ImageUniforms& U = R.m_uniforms.at(imgUid);

      if (!img->settings().displayImageAsColor())
      {
        // Render greyscale image
        if (!U.showEdges || (U.showEdges && U.overlayEdges))
        {
          GLShaderProgram* P = nullptr;
          switch (img->settings().interpolationMode()) {
          case InterpolationMode::NearestNeighbor: {
            P = m_shaderPrograms.at(ShaderProgramType::ImageGrayLinear).get();
            break;
          }
          case InterpolationMode::Trilinear: {
            if (doXray) {
              P = m_shaderPrograms.at(ShaderProgramType::XrayLinear).get();
            }
            else {
              P = R.m_imageGrayFloatingPointInterpolation
                  ? m_shaderPrograms.at(ShaderProgramType::ImageGrayLinearFloating).get()
                  : m_shaderPrograms.at(ShaderProgramType::ImageGrayLinear).get();
            }
            break;
          }
          case InterpolationMode::Tricubic: {
            if (doXray) {
              P = m_shaderPrograms.at(ShaderProgramType::XrayCubic).get();
            }
            else {
              P = m_shaderPrograms.at(ShaderProgramType::ImageGrayCubic).get();
            }
            break;
          }
          }

          const auto boundTextures = bindScalarImageTextures(imgSegPair);
          P->use();
          {
            P->setSamplerUniform("u_imgTex", msk_imgTexSampler.index);
            P->setSamplerUniform("u_cmapTex", msk_imgCmapTexSampler.index);

            P->setUniform("u_numCheckers", static_cast<float>(R.m_numCheckerboardSquares));
            P->setUniform("u_tex_T_world", U.imgTexture_T_world);

            if (doXray) {
              P->setUniform("u_imgSlope_native_T_texture", U.slope_native_T_texture);
              P->setUniform("u_waterAttenCoeff", R.m_waterMassAttenCoeff);
              P->setUniform("u_airAttenCoeff", R.m_airMassAttenCoeff);
            }
            else {
              P->setUniform("u_imgSlopeIntercept", U.slopeIntercept_normalized_T_texture);
            }

            const bool useHsv = (U.hsvModFactors.x != 0.0f) || (U.hsvModFactors.y != 1.0f) || (U.hsvModFactors.z != 1.0f);
            P->setUniform("u_applyHsvMod", useHsv);
            P->setUniform("u_cmapHsvModFactors", U.hsvModFactors);
            P->setUniform("u_cmapSlopeIntercept", U.cmapSlopeIntercept);
            P->setUniform("u_cmapQuantLevels", U.cmapQuantLevels);
            P->setUniform("u_imgThresholds", U.thresholds);
            P->setUniform("u_imgMinMax", U.minMax);
            P->setUniform("u_imgOpacity", U.imgOpacity);
            P->setUniform("u_quadrants", R.m_quadrants);
            P->setUniform("u_showFix", isFixedImage); // ignored if not checkerboard or quadrants
            P->setUniform("u_renderMode", displayModeUniform);

            renderOneImage(view, worldOffsetXhairs, *P, CurrentImages{imgSegPair}, U.showEdges);
          }
          P->stopUse();
          unbindTextures(boundTextures);
        }

        // Render edges
        if (U.showEdges)
        {
          GLShaderProgram* P = nullptr;
          switch (img->settings().interpolationMode()) {
          case InterpolationMode::NearestNeighbor:
          case InterpolationMode::Trilinear: {
            P = m_shaderPrograms.at(ShaderProgramType::EdgeLinear).get();
            break;
          }
          case InterpolationMode::Tricubic: {
            P = m_shaderPrograms.at(ShaderProgramType::EdgeCubic).get();
            break;
          }
          }

          const auto boundTextures = bindScalarImageTextures(imgSegPair);
          P->use();
          {
            P->setSamplerUniform("u_imgTex", msk_imgTexSampler.index);
            P->setSamplerUniform("u_cmapTex", msk_imgCmapTexSampler.index);

            P->setUniform("u_numCheckers", static_cast<float>(R.m_numCheckerboardSquares));
            P->setUniform("u_tex_T_world", U.imgTexture_T_world);
            P->setUniform("u_imgSlopeIntercept", U.largestSlopeIntercept);
            P->setUniform("u_imgThresholds", U.thresholds);
            P->setUniform("u_imgMinMax", U.minMax);
            P->setUniform("u_imgOpacity", U.imgOpacity);
            P->setUniform("u_cmapSlopeIntercept", U.cmapSlopeIntercept);
            P->setUniform("u_quadrants", R.m_quadrants);
            P->setUniform("u_showFix", isFixedImage); // ignored if not checkerboard or quadrants
            P->setUniform("u_renderMode", displayModeUniform);
            P->setUniform("u_thresholdEdges", U.thresholdEdges);
            P->setUniform("u_edgeMagnitude", U.edgeMagnitude);
            P->setUniform("u_useFreiChen", U.useFreiChen);
            P->setUniform("u_colormapEdges", U.colormapEdges);
            P->setUniform("u_edgeColor", U.edgeColor);

            renderOneImage(view, worldOffsetXhairs, *P, CurrentImages{imgSegPair}, U.showEdges);
          }
          P->stopUse();
          unbindTextures(boundTextures);
        }

        // Render isosurfaces:
        const auto& imgS = img->settings();

        if (imgS.isosurfacesVisible() && imgS.showIsocontoursIn2D())
        {
          const auto& vp = m_appData.windowData().viewport();
          const glm::vec2 windowSize{vp.width(), vp.height()};
          const glm::vec2 viewSize = 0.5f * glm::vec2{view.windowClipViewport()[2], view.windowClipViewport()[3]} * windowSize;

          const uint32_t activeComp = imgS.activeComponent();

          GLShaderProgram* isoP = nullptr;

          /// @todo Turn on floating-pointer interpolation automatically when zoom is high enough
          switch (img->settings().interpolationMode()) {
          case InterpolationMode::NearestNeighbor: {
            // Unfortunately, the (fixed-point) linear program looks bad when NN sampling is used for the image.
            // Therefore, we use the floating-point linear program instead (the only option). This effectively
            // never allows us to show NN isocontours.
            // isoP = &m_isoContourTexLookupLinearProgram; // fixed-point (disabled)
            isoP = m_shaderPrograms.at(ShaderProgramType::IsoContourLinearFloating).get();
            break;
          }
          case InterpolationMode::Trilinear: {
            if (R.m_isocontourFloatingPointInterpolation) {
              // Floating-point interpolation is linear only (at this time)
              isoP = m_shaderPrograms.at(ShaderProgramType::IsoContourLinearFloating).get();
            }
            else {
              isoP = m_shaderPrograms.at(ShaderProgramType::IsoContourLinearFixed).get();
            }
            break;
          }
          case InterpolationMode::Tricubic: {
            isoP = m_shaderPrograms.at(ShaderProgramType::IsoContourCubicFixed).get();
            break;
          }
          }

          const auto boundIsoTextures = bindScalarImageTextures(imgSegPair);
          isoP->use();

          for (const auto& surfaceUid : m_appData.isosurfaceUids(imgUid, activeComp))
          {
            const Isosurface* surface = m_appData.isosurface(imgUid, activeComp, surfaceUid);
            if (!surface) {
              spdlog::warn("Null isosurface {} for image {}", surfaceUid, imgUid);
              continue;
            }

            if (!surface->visible || !surface->showIn2d) {
              continue;
            }

            /// @note This case is only needed when the image is transparent, since otherwise the
            /// isoline color is the same as the image color

            static constexpr bool premult = false;
            const glm::vec3 color = glm::vec3{getIsosurfaceColor(m_appData, *surface, imgS, activeComp, premult)};

            const float imgOp = R.m_modulateIsocontourOpacityWithImageOpacity ? U.imgOpacity : 1.0f;
            const float isoOp = imgS.isosurfaceOpacityModulator() * imgOp; // global opacity mod

            isoP->setSamplerUniform("u_imgTex", msk_imgTexSampler.index);

            isoP->setUniform("u_numCheckers", static_cast<float>(R.m_numCheckerboardSquares));
            isoP->setUniform("u_tex_T_world", U.imgTexture_T_world);

            isoP->setUniform("u_isoValue", static_cast<float>(imgS.mapNativeIntensityToTexture(surface->value)));
            isoP->setUniform("u_fillOpacity", static_cast<float>(isoOp * surface->fillOpacity));
            isoP->setUniform("u_lineOpacity", static_cast<float>(isoOp * surface->opacity));
            isoP->setUniform("u_contourWidth", static_cast<float>(imgS.isoContourLineWidthIn2D()));
            isoP->setUniform("u_color", color);
            isoP->setUniform("u_viewSize", viewSize);
            isoP->setUniform("u_imgMinMax", U.minMax);
            isoP->setUniform("u_imgThresholds", U.thresholds);
            isoP->setUniform("u_quadrants", R.m_quadrants);
            isoP->setUniform("u_showFix", isFixedImage); // ignored if not checkerboard or quadrants
            isoP->setUniform("u_renderMode", displayModeUniform);

            renderOneImage(view, worldOffsetXhairs, *isoP, CurrentImages{imgSegPair}, false);
          }

          isoP->stopUse();
          unbindTextures(boundIsoTextures);
        }
      }
      else
      {
        // Color image:
        GLShaderProgram* P = nullptr;

        switch (img->settings().colorInterpolationMode()) {
        case InterpolationMode::NearestNeighbor:
        case InterpolationMode::Trilinear: {
          P = m_shaderPrograms.at(ShaderProgramType::ImageColorLinear).get();
          break;
        }
        case InterpolationMode::Tricubic: {
          P = m_shaderPrograms.at(ShaderProgramType::ImageColorCubic).get();
          break;
        }
        }

        const auto boundTextures = bindColorImageTextures(imgSegPair);
        P->use();
        {
          P->setSamplerUniform("u_imgTex", msk_imgRgbaTexSamplers);
          P->setSamplerUniform("u_cmapTex", msk_imgCmapTexSampler.index);

          P->setUniform("u_numCheckers", static_cast<float>(R.m_numCheckerboardSquares));
          P->setUniform("u_tex_T_world", U.imgTexture_T_world);
          P->setUniform("u_imgSlopeIntercept", U.slopeInterceptRgba_normalized_T_texture);
          P->setUniform("u_imgThresholds", U.thresholdsRgba);
          P->setUniform("u_imgMinMax", U.minMaxRgba);

          const bool forceAlphaToOne = (img->settings().ignoreAlpha() || 3 == img->header().numComponentsPerPixel());
          P->setUniform("u_alphaIsOne", forceAlphaToOne);
          P->setUniform("u_imgOpacity", U.imgOpacityRgba);
          P->setUniform("u_quadrants", R.m_quadrants);
          P->setUniform("u_showFix", isFixedImage); // ignored if not checkerboard or quadrants
          P->setUniform("renderMode", displayModeUniform);

          renderOneImage(view, worldOffsetXhairs, *P, CurrentImages{imgSegPair}, U.showEdges);
        }
        P->stopUse();
        unbindTextures(boundTextures);
      }

      const auto& segUid = imgSegPair.second;
      const Image* seg = segUid ? m_appData.seg(*segUid) : nullptr;

      if (seg)
      {
        GLShaderProgram& P = (InterpolationMode::NearestNeighbor == seg->settings().interpolationMode())
                               ? *m_shaderPrograms.at(ShaderProgramType::SegmentationNearest)
                               : *m_shaderPrograms.at(ShaderProgramType::SegmentationLinear);

        const auto boundTextures = bindSegTextures(imgSegPair);
        const auto boundBufferTextures = bindSegBufferTextures(imgSegPair);

        P.use();
        {
          P.setSamplerUniform("u_segTex", msk_segTexSampler.index);
          P.setSamplerUniform("u_segLabelCmapTex", msk_segLabelTableTexSampler.index);

          P.setUniform("u_numCheckers", static_cast<float>(R.m_numCheckerboardSquares));
          P.setUniform("u_tex_T_world", U.segTexture_T_world);
          P.setUniform("u_voxel_T_world", U.segVoxel_T_world);
          P.setUniform("u_segOpacity", U.segOpacity * (R.m_modulateSegOpacityWithImageOpacity ? U.imgOpacity : 1.0f));
          P.setUniform("u_quadrants", R.m_quadrants);
          P.setUniform("u_showFix", isFixedImage); // ignored if not checkerboard or quadrants
          P.setUniform("u_renderMode", displayModeUniform);

          drawSegQuad(P, R.m_quad, *seg, view, m_appData.windowData().viewport(), worldOffsetXhairs,
                      R.m_flashlightRadius, R.m_flashlightOverlays, R.m_segOutlineStyle, R.m_segInteriorOpacity,
                      R.m_segInterpCutoff);
        }
        P.stopUse();

        unbindBufferTextures(boundBufferTextures);
        unbindTextures(boundTextures);
      }

      // Render the annotation and landmark overlays:
      renderOneImage_overlays(view, miewportViewBounds, worldOffsetXhairs, CurrentImages{imgSegPair});

      isFixedImage = false;
    }

    break;
  }

  case ShaderGroup::Metric:
  {
    // This function guarantees that I has size at least 2:
    const CurrentImages I = getImageAndSegUidsForMetricShaders(view.metricImages());

    const std::array<Image*, 2> imgs{
      I[0].first ? m_appData.image(*I[0].first) : nullptr,
      I[1].first ? m_appData.image(*I[1].first) : nullptr};

    const std::array<Image*, 2> segs{
      I[0].second ? m_appData.seg(*I[0].second) : nullptr,
      I[1].second ? m_appData.seg(*I[1].second) : nullptr};

    const std::array<RenderData::ImageUniforms, 2> U{
      (I.size() >= 1 && I[0].first) ? R.m_uniforms.at(*I[0].first) : sk_defaultImageUniforms,
      (I.size() >= 2 && I[1].first) ? R.m_uniforms.at(*I[1].first) : sk_defaultImageUniforms};

    const bool useTricubic = imgs[0] && imgs[1] &&
                             (InterpolationMode::Tricubic == imgs[0]->settings().interpolationMode()) &&
                             (InterpolationMode::Tricubic == imgs[1]->settings().interpolationMode());

    const auto boundMetricTextures = bindMetricImageTextures(I, view.renderMode());

    if (ViewRenderMode::Difference == view.renderMode())
    {
      GLShaderProgram& P = useTricubic
        ? *m_shaderPrograms.at(ShaderProgramType::DifferenceCubic)
        : *m_shaderPrograms.at(ShaderProgramType::DifferenceLinear);

      const auto& params = R.m_squaredDifferenceParams;

      P.use();
      {
        P.setSamplerUniform("u_imgTex", msk_metricImgTexSamplers);
        P.setSamplerUniform("u_metricCmapTex", msk_metricCmapTexSampler.index);

        P.setUniform("u_tex_T_world", std::vector<glm::mat4>{U[0].imgTexture_T_world, U[1].imgTexture_T_world});
        P.setUniform("img1Tex_T_img0Tex", U[1].imgTexture_T_world * glm::inverse(U[0].imgTexture_T_world));
        P.setUniform("u_imgSlopeIntercept", std::vector<glm::vec2>{U[0].largestSlopeIntercept, U[1].largestSlopeIntercept});
        P.setUniform("u_metricCmapSlopeIntercept", params.m_cmapSlopeIntercept);
        P.setUniform("u_metricSlopeIntercept", params.m_slopeIntercept);
        P.setUniform("u_useSquare", R.m_useSquare);

        renderOneImage(view, worldOffsetXhairs, P, I, false);
      }
      P.stopUse();
    }
    /*
    else if (ViewRenderMode::CrossCorrelation == renderMode)
    {
      const auto& params = renderData.m_crossCorrelationParams;
      GLShaderProgram& P = m_crossCorrelationProgram;

      P.use();
      {
        P.setSamplerUniform("u_imgTex", msk_imgTexSamplers);
        P.setSamplerUniform("u_metricCmapTex", msk_metricCmapTexSampler.index);

        P.setUniform("u_tex_T_world", std::vector<glm::mat4>{U0.imgTexture_T_world, U1.imgTexture_T_world});
        P.setUniform("u_metricCmapSlopeIntercept", params.m_cmapSlopeIntercept);
        P.setUniform("u_metricSlopeIntercept", params.m_slopeIntercept);
        P.setUniform("u_metricMasking", params.m_doMasking);
        P.setUniform("u_texture1_T_texture0", U1.imgTexture_T_world * glm::inverse(U0.imgTexture_T_world));

        renderOneImage(view, worldOffsetXhairs, P, I, false);
      }
      P.stopUse();
    }
    */
    else if (ViewRenderMode::Overlay == view.renderMode())
    {
      GLShaderProgram& P = useTricubic
        ? *m_shaderPrograms.at(ShaderProgramType::OverlapCubic)
        : *m_shaderPrograms.at(ShaderProgramType::OverlapLinear);

      P.use();
      {
        P.setSamplerUniform("u_imgTex", msk_metricImgTexSamplers);

        P.setUniform("u_tex_T_world", std::vector<glm::mat4>{U[0].imgTexture_T_world, U[1].imgTexture_T_world});
        P.setUniform("u_imgSlopeIntercept", std::vector<glm::vec2>{
          U[0].slopeIntercept_normalized_T_texture, U[1].slopeIntercept_normalized_T_texture});
        P.setUniform("u_imgMinMax", std::vector<glm::vec2>{U[0].minMax, U[1].minMax});
        P.setUniform("u_imgThresholds", std::vector<glm::vec2>{U[0].thresholds, U[1].thresholds});
        P.setUniform("u_imgOpacity", std::vector<float>{U[0].imgOpacity, U[1].imgOpacity});
        P.setUniform("u_magentaCyan", R.m_overlayMagentaCyan);

        renderOneImage(view, worldOffsetXhairs, P, I, false);
      }
      P.stopUse();
    }

    unbindTextures(boundMetricTextures);

    for (unsigned int i = 0; i < NUM_METRIC_IMAGES; ++i)
    {
      if (!segs[i]) {
        continue;
      }

      GLShaderProgram& P = (InterpolationMode::NearestNeighbor == segs[i]->settings().interpolationMode())
                             ? *m_shaderPrograms.at(ShaderProgramType::SegmentationNearest)
                             : *m_shaderPrograms.at(ShaderProgramType::SegmentationLinear);

      const auto boundTextures = bindSegTextures(I[i]);
      const auto boundBufferTextures = bindSegBufferTextures(I[i]);

      P.use();
      {
        P.setSamplerUniform("u_segTex", msk_segTexSampler.index);
        P.setSamplerUniform("u_segLabelCmapTex", msk_segLabelTableTexSampler.index);

        P.setUniform("u_numCheckers", 1.0f); // checkerboarding disabled
        P.setUniform("u_tex_T_world", U[i].segTexture_T_world);
        P.setUniform("u_voxel_T_world", U[i].segVoxel_T_world);
        P.setUniform("u_segOpacity", U[i].segOpacity * (R.m_modulateSegOpacityWithImageOpacity ? U[i].imgOpacity : 1.0f));
        P.setUniform("u_quadrants", glm::ivec2{0, 0});
        P.setUniform("u_showFix", true); // ignored if not checkerboard or quadrants
        P.setUniform("u_renderMode", 0); // disabled

        drawSegQuad(P, R.m_quad, *segs[i], view, m_appData.windowData().viewport(), worldOffsetXhairs,
                    R.m_flashlightRadius, R.m_flashlightOverlays, R.m_segOutlineStyle, R.m_segInteriorOpacity,
                    R.m_segInterpCutoff);
      }
      P.stopUse();

      unbindBufferTextures(boundBufferTextures);
      unbindTextures(boundTextures);
    }

    break;
  }

  case ShaderGroup::Volume:
  {
    const CurrentImages I = getImageAndSegUidsForImageShaders(view.renderedImages());

    if (I.empty()) {
      return;
    }

    // Only volume render the first image:

    /// @todo Either 1) let use only select one image or
    /// 2) enable rendering more than one image
    const auto& imgSegPair = I.front();

    const Image* image = m_appData.image(*imgSegPair.first);
    if (!image) {
      spdlog::warn("Null image {} when raycasting", *imgSegPair.first);
      return;
    }

    const ImageSettings& settings = image->settings();
    if (!settings.isosurfacesVisible()) {
      return; // Hide all surfaces
    }

    // Render surfaces of the active image component
    const uint32_t activeComp = image->settings().activeComponent();

    const auto isosurfaceUids = m_appData.isosurfaceUids(*imgSegPair.first, activeComp);
    if (isosurfaceUids.empty()) {
      return;
    }

    updateIsosurfaceDataFor3d(m_appData, *imgSegPair.first);

    const auto boundImageTextures = bindScalarImageTextures(imgSegPair);
    const auto boundSegBufferTextures = bindSegBufferTextures(imgSegPair);

    const auto& U = R.m_uniforms.at(*imgSegPair.first);

    GLShaderProgram& P = m_raycastIsoSurfaceProgram;

    P.use();
    {
      P.setSamplerUniform("u_imgTex", msk_imgTexSampler.index);
      P.setSamplerUniform("u_segTex", msk_segTexSampler.index);
      P.setSamplerUniform("u_jumpTex", msk_jumpTexSampler.index);

      /// @todo Put a lot of these into the uniform settings...

      P.setUniform("u_tex_T_world", U.imgTexture_T_world);
      P.setUniform("world_T_imgTexture", U.world_T_imgTexture);

      // The camera is positioned at the crosshairs:
      P.setUniform("worldEyePos", worldOffsetXhairs);

      // P.setUniform( "voxelSpacing", U.voxelSpacing );
      P.setUniform("texGrads", U.textureGradientStep);

      /// @todo Shader expects 16 values, but we're not giving that to the shader!!
      P.setUniform("u_isoValues", R.m_isosurfaceData.values);
      P.setUniform("u_isoOpacities", R.m_isosurfaceData.opacities);
      P.setUniform("isoEdges", R.m_isosurfaceData.edgeStrengths);
      P.setUniform("lightAmbient", R.m_isosurfaceData.ambientLights);
      P.setUniform("lightDiffuse", R.m_isosurfaceData.diffuseLights);
      P.setUniform("lightSpecular", R.m_isosurfaceData.specularLights);
      P.setUniform("lightShininess", R.m_isosurfaceData.shininesses);

      /// @todo Set this to larger sampling factor when the user is moving the slider
      P.setUniform("samplingFactor", R.m_raycastSamplingFactor);

      P.setUniform("renderFrontFaces", R.m_renderFrontFaces);
      P.setUniform("renderBackFaces", R.m_renderBackFaces);

      P.setUniform("segMasksIn", (RenderData::SegMaskingForRaycasting::SegMasksIn == R.m_segMasking));
      P.setUniform("segMasksOut", (RenderData::SegMaskingForRaycasting::SegMasksOut == R.m_segMasking));

      P.setUniform("bgColor", R.m_3dBackgroundColor.a * R.m_3dBackgroundColor);
      P.setUniform("noHitTransparent", R.m_3dTransparentIfNoHit);

      volumeRenderOneImage(view, P, CurrentImages{imgSegPair});
    }
    P.stopUse();

    unbindTextures(boundImageTextures);
    unbindBufferTextures(boundSegBufferTextures);

    break;
  }

  case ShaderGroup::None:
  default:
  {
    return;
  }
  }
}

void Rendering::renderAllLandmarks(
  const View& view, const FrameBounds& miewportViewBounds, const glm::vec3& worldOffsetXhairs)
{
  switch (view.renderMode())
  {
  case ViewRenderMode::Image: {
    const CurrentImages I = getImageAndSegUidsForImageShaders(view.renderedImages());
    for (const auto& imgSegPair : I) {
      drawLandmarks(m_nvg, miewportViewBounds, worldOffsetXhairs, m_appData, view, CurrentImages{imgSegPair});
      setupOpenGLState();
    }
    break;
  }
  case ViewRenderMode::Checkerboard:
  case ViewRenderMode::Quadrants:
  case ViewRenderMode::Flashlight: {
    const CurrentImages I = getImageAndSegUidsForMetricShaders(view.metricImages()); // guaranteed size 2
    for (const auto& imgSegPair : I) {
      drawLandmarks(m_nvg, miewportViewBounds, worldOffsetXhairs, m_appData, view, CurrentImages{imgSegPair});
      setupOpenGLState();
    }
    break;
  }
  case ViewRenderMode::Disabled: {
    return;
  }
  default: {
    // This function guarantees that I has size at least 2:
    drawLandmarks(m_nvg, miewportViewBounds, worldOffsetXhairs, m_appData,
                  view, getImageAndSegUidsForMetricShaders(view.metricImages()));
    setupOpenGLState();
  }
  }
}

void Rendering::renderAllAnnotations(
  const View& view, const FrameBounds& miewportViewBounds, const glm::vec3& worldOffsetXhairs)
{
  switch (view.renderMode())
  {
  case ViewRenderMode::Image: {
    const CurrentImages I = getImageAndSegUidsForImageShaders(view.renderedImages());
    for (const auto& imgSegPair : I) {
      drawAnnotations(m_nvg, miewportViewBounds, worldOffsetXhairs, m_appData, view, CurrentImages{imgSegPair});
      setupOpenGLState();
    }
    break;
  }
  case ViewRenderMode::Checkerboard:
  case ViewRenderMode::Quadrants:
  case ViewRenderMode::Flashlight: {
    const CurrentImages I = getImageAndSegUidsForMetricShaders(view.metricImages()); // guaranteed size 2
    for (const auto& imgSegPair : I) {
      drawAnnotations(m_nvg, miewportViewBounds, worldOffsetXhairs, m_appData, view, CurrentImages{imgSegPair});
      setupOpenGLState();
    }
  }
  case ViewRenderMode::Disabled: {
    return;
  }
  default: {
    // This function guarantees that I has size at least 2:
    drawAnnotations(m_nvg, miewportViewBounds, worldOffsetXhairs, m_appData,
                    view, getImageAndSegUidsForMetricShaders(view.metricImages()));
    setupOpenGLState();
  }
  }
}

void Rendering::renderImageData()
{
  if (!m_isAppDoneLoadingImages) {
    // Don't render images if the app is still loading them
    return;
  }

  const auto& R = m_appData.renderData();
  const bool renderLandmarksOnTop = R.m_globalLandmarkParams.renderOnTopOfAllImagePlanes;
  const bool renderAnnotationsOnTop = R.m_globalAnnotationParams.renderOnTopOfAllImagePlanes;

  // Render images for each view in the layout
  for (const auto& [viewUid, view] : m_appData.windowData().currentLayout().views())
  {
    if (!view) {
      continue;
    }

    // Offset the crosshairs according to the image slice in the view
    const glm::vec3 worldXhairsOffset =
      view->updateImageSlice(m_appData, m_appData.state().worldCrosshairs().worldOrigin());

    const auto miewportViewBounds = helper::computeMiewportFrameBounds(
      view->windowClipViewport(), m_appData.windowData().viewport().getAsVec4());

    renderAllImages(*view, miewportViewBounds, worldXhairsOffset);

    // Do not render landmarks and annotations in volume rendering mode
    if (ViewRenderMode::VolumeRender != view->renderMode())
    {
      if (renderLandmarksOnTop) {
        renderAllLandmarks(*view, miewportViewBounds, worldXhairsOffset);
      }

      if (renderAnnotationsOnTop) {
        renderAllAnnotations(*view, miewportViewBounds, worldXhairsOffset);
      }
    }
  }
}

void Rendering::renderVectorOverlays()
{
  if (!m_nvg) {
    return;
  }

  const WindowData& windowData = m_appData.windowData();
  const Viewport& windowVP = windowData.viewport();
  const auto& R = m_appData.renderData();

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

  if (m_appData.settings().lockAnatomicalCoordinateAxesWithReferenceImage()) {
    if (const Image* refImage = m_appData.refImage()) {
      world_T_refSubject = refImage->transformations().worldDef_T_subject();
    }
  }

  for (const auto& viewUid : windowData.currentViewUids())
  {
    const View* view = windowData.getCurrentView(viewUid);
    if (!view) {
      continue;
    }

    // Bounds of the view frame in Miewport space:
    const auto miewportViewBounds = helper::computeMiewportFrameBounds(
      view->windowClipViewport(), windowVP.getAsVec4());

    // Do not render vector overlays when view is disabled
    if (m_showOverlays && ViewRenderMode::Disabled != view->renderMode())
    {
      const auto labelPosInfo = math::computeAnatomicalLabelPosInfo(
        miewportViewBounds, windowVP, view->camera(),
        world_T_refSubject, view->windowClip_T_viewClip(),
        m_appData.state().worldCrosshairs().worldOrigin());

      // Do not render crosshairs in volume rendering mode
      if (ViewRenderMode::VolumeRender != view->renderMode()) {
        drawCrosshairs(m_nvg, miewportViewBounds, *view, R.m_crosshairsColor, labelPosInfo);
      }

      if (AnatomicalLabelType::Disabled != R.m_anatomicalLabelType)
      {
        drawAnatomicalLabels(
          m_nvg, miewportViewBounds, (ViewType::Oblique == view->viewType()),
          R.m_anatomicalLabelColor, R.m_anatomicalLabelType, labelPosInfo);
      }
    }

    ViewOutlineMode outlineMode = ViewOutlineMode::None;

    if (state::isInStateWhereViewSelectionsVisible() && ASM::current_state_ptr)
    {
      const auto hoveredViewUid = ASM::current_state_ptr->hoveredViewUid();
      const auto selectedViewUid = ASM::current_state_ptr->selectedViewUid();

      if (selectedViewUid && (viewUid == *selectedViewUid)) {
        outlineMode = ViewOutlineMode::Selected;
      }
      else if (hoveredViewUid && (viewUid == *hoveredViewUid)) {
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
  static const std::string shaderPath("src/rendering/shaders/");
  const std::string helpersRep = loadFile(shaderPath + "functions/Helpers.glsl");
  const std::string colorHelpersRep = loadFile(shaderPath + "functions/ColorHelpers.glsl");
  const std::string doRenderRep = loadFile(shaderPath + "functions/DoRender.glsl");
  const std::string texFloatingPointLinearRep = loadFile(shaderPath + "functions/TextureLookup_FloatingPoint_Linear.glsl");
  const std::string texLinearRep = loadFile(shaderPath + "functions/TextureLookup_Linear.glsl");
  const std::string texCubicRep = loadFile(shaderPath + "functions/TextureLookup_Cubic.glsl");
  const std::string uintTexLookupLinearRep = loadFile(shaderPath + "functions/UIntTextureLookup_Linear.glsl");
  const std::string segValueNearestRep = loadFile(shaderPath + "functions/SegValue_Nearest.glsl");
  const std::string segValueLinearRep = loadFile(shaderPath + "functions/SegValue_Linear.glsl");
  const std::string segInteriorAlphaWithOutlineRep = loadFile(shaderPath + "functions/SegInteriorAlpha_WithOutline.glsl");

  // All the vertex shader uniforms:
  Uniforms vsTransformUniforms;
  vsTransformUniforms.insertUniform("u_view_T_clip", UniformType::Mat4, sk_identMat4);
  vsTransformUniforms.insertUniform("u_world_T_clip", UniformType::Mat4, sk_identMat4);
  vsTransformUniforms.insertUniform("u_clipDepth", UniformType::Float, 0.0f);
  vsTransformUniforms.insertUniform("u_tex_T_world", UniformType::Mat4, sk_identMat4);

  Uniforms vsViewModeUniforms;
  vsViewModeUniforms.insertUniform("u_aspectRatio", UniformType::Float, 1.0f);
  vsViewModeUniforms.insertUniform("u_numCheckers", UniformType::Int, 1);

  Uniforms vsImageUniforms;
  vsImageUniforms.insertUniforms(vsTransformUniforms);
  vsImageUniforms.insertUniforms(vsViewModeUniforms);

  Uniforms vsSegUniforms;
  vsSegUniforms.insertUniform("u_voxel_T_world", UniformType::Mat4, sk_identMat4);

  Uniforms vsMetricUniforms;
  vsMetricUniforms.insertUniform("u_view_T_clip", UniformType::Mat4, sk_identMat4);
  vsMetricUniforms.insertUniform("u_world_T_clip", UniformType::Mat4, sk_identMat4);
  vsMetricUniforms.insertUniform("u_clipDepth", UniformType::Float, 0.0f);
  vsMetricUniforms.insertUniform("u_tex_T_world", UniformType::Mat4Vector, Mat4Vector{sk_identMat4, sk_identMat4});

  // All the fragment shader uniforms:
  Uniforms fsImageAdjustmentUniforms;
  fsImageAdjustmentUniforms.insertUniform("u_imgSlopeIntercept", UniformType::Vec2, sk_zeroVec2);
  fsImageAdjustmentUniforms.insertUniform("u_imgMinMax", UniformType::Vec2, sk_zeroVec2);
  fsImageAdjustmentUniforms.insertUniform("u_imgThresholds", UniformType::Vec2, sk_zeroVec2);
  fsImageAdjustmentUniforms.insertUniform("u_imgOpacity", UniformType::Float, 0.0f);

  Uniforms fsColorMapUniforms;
  fsColorMapUniforms.insertUniform("u_cmapSlopeIntercept", UniformType::Vec2, sk_zeroVec2);
  fsColorMapUniforms.insertUniform("u_cmapQuantLevels", UniformType::Int, 0);
  fsColorMapUniforms.insertUniform("u_cmapHsvModFactors", UniformType::Vec3, glm::vec3{0.0f, 1.0f, 1.0f});
  fsColorMapUniforms.insertUniform("u_applyHsvMod", UniformType::Bool, false);

  Uniforms fsRenderModeUniforms;
  fsRenderModeUniforms.insertUniform("u_renderMode", UniformType::Int, 0); // 0: image, 1: checkerboard, 2: quadrants, 3: flashlight
  fsRenderModeUniforms.insertUniform("u_clipCrosshairs", UniformType::Vec2, sk_zeroVec2);
  fsRenderModeUniforms.insertUniform("u_quadrants", UniformType::IVec2, sk_zeroIVec2); // For quadrants
  fsRenderModeUniforms.insertUniform("u_showFix", UniformType::Bool, true); // For checkerboarding
  fsRenderModeUniforms.insertUniform("u_flashlightRadius", UniformType::Float, 0.5f);
  fsRenderModeUniforms.insertUniform("u_flashlightMovingOnFixed", UniformType::Bool, true);

  Uniforms fsIntensityProjectionUniforms;
  fsIntensityProjectionUniforms.insertUniform("u_mipMode", UniformType::Int, 0);
  fsIntensityProjectionUniforms.insertUniform("u_halfNumMipSamples", UniformType::Int, 0);
  fsIntensityProjectionUniforms.insertUniform("u_texSamplingDirZ", UniformType::Vec3, sk_zeroVec3);

  Uniforms fsImageGrayUniforms; // image gray FS
  fsImageGrayUniforms.insertUniforms(fsImageAdjustmentUniforms);
  fsImageGrayUniforms.insertUniforms(fsColorMapUniforms);
  fsImageGrayUniforms.insertUniforms(fsRenderModeUniforms);
  fsImageGrayUniforms.insertUniforms(fsIntensityProjectionUniforms);
  fsImageGrayUniforms.insertUniform("u_imgTex", UniformType::Sampler, msk_imgTexSampler);
  fsImageGrayUniforms.insertUniform("u_cmapTex", UniformType::Sampler, msk_imgCmapTexSampler);

  Uniforms fsImageColorUniforms; // image color FS
  fsImageColorUniforms.insertUniforms(fsRenderModeUniforms);
  fsImageColorUniforms.insertUniform("u_imgTex", UniformType::SamplerVector, msk_imgRgbaTexSamplers);
  fsImageColorUniforms.insertUniform("u_imgSlopeIntercept", UniformType::Vec2Vector, Vec2Vector{sk_zeroVec2});
  fsImageColorUniforms.insertUniform("u_alphaIsOne", UniformType::Bool, true);
  fsImageColorUniforms.insertUniform("u_imgOpacity", UniformType::FloatVector, FloatVector{0.0f});
  fsImageColorUniforms.insertUniform("u_imgMinMax", UniformType::Vec2Vector, Vec2Vector{sk_zeroVec2});
  fsImageColorUniforms.insertUniform("u_imgThresholds", UniformType::Vec2Vector, Vec2Vector{sk_zeroVec2});

  Uniforms fsEdgeUniforms;
  fsEdgeUniforms.insertUniforms(fsImageAdjustmentUniforms);
  fsEdgeUniforms.insertUniforms(fsRenderModeUniforms);
  fsEdgeUniforms.insertUniform("u_imgTex", UniformType::Sampler, msk_imgTexSampler);
  fsEdgeUniforms.insertUniform("u_cmapTex", UniformType::Sampler, msk_imgCmapTexSampler);
  fsEdgeUniforms.insertUniform("u_cmapSlopeIntercept", UniformType::Vec2, sk_zeroVec2);
  fsEdgeUniforms.insertUniform("u_thresholdEdges", UniformType::Bool, true);
  fsEdgeUniforms.insertUniform("u_edgeMagnitude", UniformType::Float, 0.0f);
  fsEdgeUniforms.insertUniform("u_useFreiChen", UniformType::Bool, 0.0f);
  fsEdgeUniforms.insertUniform("u_colormapEdges", UniformType::Bool, false);
  fsEdgeUniforms.insertUniform("u_edgeColor", UniformType::Vec4, sk_zeroVec4);
  fsEdgeUniforms.insertUniform("u_texelDirs", UniformType::Vec3Vector, Vec3Vector{sk_zeroVec3});

  Uniforms fsXrayUniforms;
  fsXrayUniforms.insertUniforms(fsImageAdjustmentUniforms);
  fsXrayUniforms.insertUniforms(fsColorMapUniforms);
  fsXrayUniforms.insertUniforms(fsRenderModeUniforms);
  fsXrayUniforms.insertUniform("u_imgTex", UniformType::Sampler, msk_imgTexSampler);
  fsXrayUniforms.insertUniform("u_cmapTex", UniformType::Sampler, msk_imgCmapTexSampler);
  fsXrayUniforms.insertUniform("u_imgSlope_native_T_texture", UniformType::Float, 1.0f);

  fsXrayUniforms.insertUniforms(fsIntensityProjectionUniforms);
  fsXrayUniforms.insertUniform("u_mipSamplingDistance_cm", UniformType::Float, 0.0f);
  fsXrayUniforms.insertUniform("u_waterAttenCoeff", UniformType::Float, 0.0f);
  fsXrayUniforms.insertUniform("u_airAttenCoeff", UniformType::Float, 0.0f);

  Uniforms fsSegAdjustmentUniforms;
  fsSegAdjustmentUniforms.insertUniform("u_segOpacity", UniformType::Float, 0.0f);
  fsSegAdjustmentUniforms.insertUniform("u_segFillOpacity", UniformType::Float, 1.0f);
  fsSegAdjustmentUniforms.insertUniform("u_texSamplingDirsForSegOutline", UniformType::Vec3Vector, Vec3Vector{sk_zeroVec3});

  Uniforms fsSegNearestUniforms; // seg NN shader
  fsSegNearestUniforms.insertUniforms(fsRenderModeUniforms);
  fsSegNearestUniforms.insertUniforms(fsSegAdjustmentUniforms);
  fsSegNearestUniforms.insertUniform("u_segTex", UniformType::Sampler, msk_segTexSampler);
  fsSegNearestUniforms.insertUniform("u_segLabelCmapTex", UniformType::Sampler, msk_segLabelTableTexSampler);

  Uniforms fsSegLinearUniforms; // seg linear shader
  fsSegLinearUniforms.insertUniforms(fsSegNearestUniforms);
  fsSegLinearUniforms.insertUniform("u_segInterpCutoff", UniformType::Float, 0.5f);
  fsSegLinearUniforms.insertUniform("u_texSamplingDirsForSmoothSeg", UniformType::Vec3Vector, Vec3Vector{sk_zeroVec3});

  Uniforms fsIsoUniforms;
  fsIsoUniforms.insertUniforms(fsRenderModeUniforms);
  fsIsoUniforms.insertUniforms(fsIntensityProjectionUniforms);
  fsIsoUniforms.insertUniform("u_isoValue", UniformType::Float, 0.0f);
  fsIsoUniforms.insertUniform("u_fillOpacity", UniformType::Float, 0.0f);
  fsIsoUniforms.insertUniform("u_lineOpacity", UniformType::Float, 0.0f);
  fsIsoUniforms.insertUniform("u_color", UniformType::Vec3, sk_zeroVec3);
  fsIsoUniforms.insertUniform("u_contourWidth", UniformType::Float, 0.0f);
  fsIsoUniforms.insertUniform("u_viewSize", UniformType::Vec2, sk_zeroVec2);
  fsIsoUniforms.insertUniform("u_imgMinMax", UniformType::Vec2, sk_zeroVec2);
  fsIsoUniforms.insertUniform("u_imgThresholds", UniformType::Vec2, sk_zeroVec2);
  fsIsoUniforms.insertUniform("u_imgTex", UniformType::Sampler, msk_imgTexSampler);

  Uniforms fsDiffUniforms;
  fsDiffUniforms.insertUniforms(fsIntensityProjectionUniforms);
  fsDiffUniforms.insertUniform("u_imgTex", UniformType::SamplerVector, msk_metricImgTexSamplers);
  fsDiffUniforms.insertUniform("u_metricCmapTex", UniformType::Sampler, msk_metricCmapTexSampler);
  fsDiffUniforms.insertUniform("u_imgSlopeIntercept", UniformType::Vec2Vector, Vec2Vector{sk_zeroVec2, sk_zeroVec2});
  fsDiffUniforms.insertUniform("u_metricCmapSlopeIntercept", UniformType::Vec2, sk_zeroVec2);
  fsDiffUniforms.insertUniform("u_metricSlopeIntercept", UniformType::Vec2, sk_zeroVec2);
  fsDiffUniforms.insertUniform("u_useSquare", UniformType::Bool, true);
  fsDiffUniforms.insertUniform("img1Tex_T_img0Tex", UniformType::Mat4, sk_identMat4);

  Uniforms fsOverlayUniforms;
  fsOverlayUniforms.insertUniform("u_imgTex", UniformType::SamplerVector, msk_metricImgTexSamplers);
  fsOverlayUniforms.insertUniform("u_imgSlopeIntercept", UniformType::Vec2Vector, Vec2Vector{sk_zeroVec2, sk_zeroVec2});
  fsOverlayUniforms.insertUniform("u_imgMinMax", UniformType::Vec2Vector, Vec2Vector{sk_zeroVec2, sk_zeroVec2});
  fsOverlayUniforms.insertUniform("u_imgThresholds", UniformType::Vec2Vector, Vec2Vector{sk_zeroVec2, sk_zeroVec2});
  fsOverlayUniforms.insertUniform("u_imgOpacity", UniformType::FloatVector, FloatVector{0.0f, 0.0f});
  fsOverlayUniforms.insertUniform("u_magentaCyan", UniformType::Bool, true);

  constexpr std::array<ShaderProgramType, 18> allShaders = {
    ShaderProgramType::ImageGrayLinear,
    ShaderProgramType::ImageGrayLinearFloating,
    ShaderProgramType::ImageGrayCubic,
    ShaderProgramType::ImageColorLinear,
    ShaderProgramType::ImageColorCubic,
    ShaderProgramType::EdgeLinear,
    ShaderProgramType::EdgeCubic,
    ShaderProgramType::XrayLinear,
    ShaderProgramType::XrayCubic,
    ShaderProgramType::SegmentationNearest,
    ShaderProgramType::SegmentationLinear,
    ShaderProgramType::IsoContourLinearFloating,
    ShaderProgramType::IsoContourLinearFixed,
    ShaderProgramType::IsoContourCubicFixed,
    ShaderProgramType::DifferenceLinear,
    ShaderProgramType::DifferenceCubic,
    ShaderProgramType::OverlapLinear,
    ShaderProgramType::OverlapCubic
  };

  struct ShaderInfo
  {
    std::string vsFileName;
    std::string fsFileName;
    std::unordered_map<std::string, std::string> fsReplacements;
    Uniforms vsUniforms;
    Uniforms fsUniforms;
  };

  const std::unordered_map<ShaderProgramType, ShaderInfo> shaderTypeToInfo{
    {ShaderProgramType::ImageGrayLinear,
     {"Image.vs", "ImageGrey.fs",
      {{"{{HELPER_FUNCTIONS}}", helpersRep},
       {"{{COLOR_HELPER_FUNCTIONS}}", colorHelpersRep},
       {"{{TEXTURE_LOOKUP_FUNCTION}}", texLinearRep},
       {"{{DO_RENDER_FUNCTION}}", doRenderRep}},
      vsImageUniforms, fsImageGrayUniforms
     }
    },
    {ShaderProgramType::ImageGrayLinearFloating,
      {"Image.vs", "ImageGrey.fs",
        {{"{{HELPER_FUNCTIONS}}", helpersRep},
         {"{{COLOR_HELPER_FUNCTIONS}}", colorHelpersRep},
         {"{{TEXTURE_LOOKUP_FUNCTION}}", texFloatingPointLinearRep},
         {"{{DO_RENDER_FUNCTION}}", doRenderRep}},
        vsImageUniforms, fsImageGrayUniforms
      }
    },
    {ShaderProgramType::ImageGrayCubic,
     {"Image.vs", "ImageGrey.fs",
      {{"{{HELPER_FUNCTIONS}}", helpersRep},
       {"{{COLOR_HELPER_FUNCTIONS}}", colorHelpersRep},
       {"{{TEXTURE_LOOKUP_FUNCTION}}", texCubicRep},
       {"{{DO_RENDER_FUNCTION}}", doRenderRep}},
      vsImageUniforms, fsImageGrayUniforms
     }
    },
    {ShaderProgramType::ImageColorLinear,
     {"Image.vs", "ImageColor.fs",
      {{"{{HELPER_FUNCTIONS}}", helpersRep},
       {"{{COLOR_HELPER_FUNCTIONS}}", colorHelpersRep},
       {"{{TEXTURE_LOOKUP_FUNCTION}}", texLinearRep},
       {"{{DO_RENDER_FUNCTION}}", doRenderRep}},
       vsImageUniforms, fsImageColorUniforms
     }
    },
    {ShaderProgramType::ImageColorCubic,
     {"Image.vs", "ImageColor.fs",
      {{"{{HELPER_FUNCTIONS}}", helpersRep},
       {"{{TEXTURE_LOOKUP_FUNCTION}}", texCubicRep},
       {"{{DO_RENDER_FUNCTION}}", doRenderRep}},
      vsImageUniforms, fsImageColorUniforms
     }
    },
    {ShaderProgramType::EdgeLinear,
     {"Image.vs", "Edge.fs",
      {{"{{HELPER_FUNCTIONS}}", helpersRep},
       {"{{COLOR_HELPER_FUNCTIONS}}", colorHelpersRep},
       {"{{TEXTURE_LOOKUP_FUNCTION}}", texLinearRep},
       {"{{DO_RENDER_FUNCTION}}", doRenderRep}},
      vsImageUniforms, fsEdgeUniforms
     }
    },
    {ShaderProgramType::EdgeCubic,
     {"Image.vs", "Edge.fs",
      {{"{{HELPER_FUNCTIONS}}", helpersRep},
       {"{{COLOR_HELPER_FUNCTIONS}}", colorHelpersRep},
       {"{{TEXTURE_LOOKUP_FUNCTION}}", texCubicRep},
       {"{{DO_RENDER_FUNCTION}}", doRenderRep}},
      vsImageUniforms, fsEdgeUniforms
     }
    },
    {ShaderProgramType::XrayLinear,
     {"Image.vs", "Xray.fs",
      {{"{{HELPER_FUNCTIONS}}", helpersRep},
       {"{{COLOR_HELPER_FUNCTIONS}}", colorHelpersRep},
       {"{{TEXTURE_LOOKUP_FUNCTION}}", texLinearRep},
       {"{{DO_RENDER_FUNCTION}}", doRenderRep}},
      vsImageUniforms, fsXrayUniforms
     }
    },
    {ShaderProgramType::XrayCubic,
     {"Image.vs", "Xray.fs",
      {{"{{HELPER_FUNCTIONS}}", helpersRep},
       {"{{COLOR_HELPER_FUNCTIONS}}", colorHelpersRep},
       {"{{TEXTURE_LOOKUP_FUNCTION}}", texCubicRep},
       {"{{DO_RENDER_FUNCTION}}", doRenderRep}},
      vsImageUniforms, fsXrayUniforms
     }
    },
    {ShaderProgramType::SegmentationNearest,
     {"Seg.vs", "Seg.fs",
      {{"{{HELPER_FUNCTIONS}}", helpersRep},
       {"{{UINT_TEXTURE_LOOKUP_FUNCTION}}", uintTexLookupLinearRep},
       {"{{GET_SEG_VALUE_FUNCTION}}", segValueNearestRep},
       {"{{GET_SEG_INTERIOR_ALPHA_FUNCTION}}", segInteriorAlphaWithOutlineRep},
       {"{{DO_RENDER_FUNCTION}}", doRenderRep}},
      vsSegUniforms, fsSegNearestUniforms
     }
    },
    {ShaderProgramType::SegmentationLinear,
     {"Seg.vs", "Seg.fs",
      {{"{{HELPER_FUNCTIONS}}", helpersRep},
       {"{{UINT_TEXTURE_LOOKUP_FUNCTION}}", uintTexLookupLinearRep},
       {"{{GET_SEG_VALUE_FUNCTION}}", segValueLinearRep},
       {"{{GET_SEG_INTERIOR_ALPHA_FUNCTION}}", segInteriorAlphaWithOutlineRep},
       {"{{DO_RENDER_FUNCTION}}", doRenderRep}},
      vsSegUniforms, fsSegLinearUniforms
     }
    },
    {ShaderProgramType::IsoContourLinearFloating,
     {"Image.vs", "IsoContour.fs",
      {{"{{HELPER_FUNCTIONS}}", helpersRep},
       {"{{TEXTURE_LOOKUP_FUNCTION}}", texFloatingPointLinearRep},
       {"{{DO_RENDER_FUNCTION}}", doRenderRep}},
       vsImageUniforms, fsIsoUniforms
     }
    },
    {ShaderProgramType::IsoContourLinearFixed,
     {"Image.vs", "IsoContour.fs",
      {{"{{HELPER_FUNCTIONS}}", helpersRep},
       {"{{TEXTURE_LOOKUP_FUNCTION}}", texLinearRep},
       {"{{DO_RENDER_FUNCTION}}", doRenderRep}},
       vsImageUniforms, fsIsoUniforms
     }
    },
    {ShaderProgramType::IsoContourCubicFixed,
     {"Image.vs", "IsoContour.fs",
      {{"{{HELPER_FUNCTIONS}}", helpersRep},
       {"{{TEXTURE_LOOKUP_FUNCTION}}", texCubicRep},
       {"{{DO_RENDER_FUNCTION}}", doRenderRep}},
      vsImageUniforms, fsIsoUniforms
     }
    },
    {ShaderProgramType::DifferenceLinear,
      {"Metric.vs", "Difference.fs",
       {{"{{HELPER_FUNCTIONS}}", helpersRep},
        {"{{TEXTURE_LOOKUP_FUNCTION}}", texLinearRep}},
        vsMetricUniforms, fsDiffUniforms
      }
    },
    {ShaderProgramType::DifferenceCubic,
      {"Metric.vs", "Difference.fs",
       {{"{{HELPER_FUNCTIONS}}", helpersRep},
        {"{{TEXTURE_LOOKUP_FUNCTION}}", texCubicRep}},
        vsMetricUniforms, fsDiffUniforms
      }
    },
    {ShaderProgramType::OverlapLinear,
      {"Metric.vs", "Overlay.fs",
        {{"{{HELPER_FUNCTIONS}}", helpersRep},
         {"{{TEXTURE_LOOKUP_FUNCTION}}", texLinearRep}},
        vsMetricUniforms, fsOverlayUniforms
      }
    },
    {ShaderProgramType::OverlapCubic,
      {"Metric.vs", "Overlay.fs",
       {{"{{HELPER_FUNCTIONS}}", helpersRep},
        {"{{TEXTURE_LOOKUP_FUNCTION}}", texCubicRep}},
       vsMetricUniforms, fsOverlayUniforms
      }
    }
  };

  for (auto shaderType : allShaders)
  {
    const auto& info = shaderTypeToInfo.at(shaderType);

    auto prog = createShaderProgram(
      to_string(shaderType), info.vsFileName, info.fsFileName, info.fsReplacements,
      info.vsUniforms, info.fsUniforms);

    if (prog) {
      m_shaderPrograms.emplace(shaderType, std::move(*prog));
    } else {
      spdlog::error(prog.error());
      throw_debug(std::format("Failed to create shader program {}", to_string(shaderType)))
    }
  }

  if (!createRaycastIsoSurfaceProgram(m_raycastIsoSurfaceProgram))
  {
    throw_debug("Failed to create isosurface raycasting program")
  }
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

    fsUniforms.insertUniform("u_tex_T_world", UniformType::Mat4, sk_identMat4);
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

// For the cross-correlation shader:
/*
    fsUniforms.insertUniform("u_imgTex", UniformType::SamplerVector, msk_imgTexSamplers);
    fsUniforms.insertUniform("u_segTex", UniformType::SamplerVector, msk_segTexSamplers);
    fsUniforms.insertUniform("u_metricCmapTex", UniformType::Sampler, msk_metricCmapTexSampler);
    fsUniforms.insertUniform("u_segLabelCmapTex", UniformType::SamplerVector, msk_labelTableTexSamplers);

    fsUniforms.insertUniform("u_segOpacity", UniformType::FloatVector, FloatVector{0.0f, 0.0f});

    fsUniforms.insertUniform("u_segFillOpacity", UniformType::Float, 1.0f);
    fsUniforms.insertUniform("u_texSamplingDirsForSegOutline", UniformType::Vec3Vector, Vec3Vector{sk_zeroVec3});

    fsUniforms.insertUniform("u_metricCmapSlopeIntercept", UniformType::Vec2, sk_zeroVec2);
    fsUniforms.insertUniform("u_metricSlopeIntercept", UniformType::Vec2, sk_zeroVec2);
    fsUniforms.insertUniform("u_metricMasking", UniformType::Bool, false);

    fsUniforms.insertUniform("u_texture1_T_texture0", UniformType::Mat4, sk_identMat4);
    fsUniforms.insertUniform("u_tex0SamplingDirX", UniformType::Vec3, sk_zeroVec3);
    fsUniforms.insertUniform("u_tex0SamplingDirY", UniformType::Vec3, sk_zeroVec3);
*/

bool Rendering::showVectorOverlays() const
{
  return m_showOverlays;
}

void Rendering::setShowVectorOverlays(bool show)
{
  m_showOverlays = show;
}

void Rendering::updateIsosurfaceDataFor3d(AppData& appData, const uuid& imageUid)
{
  auto& isoData = appData.renderData().m_isosurfaceData;
  const Image* image = appData.image(imageUid);
  const ImageSettings& settings = image->settings();

  // Turn off all of the isosurfaces
  // std::fill(std::begin(isoData.opacities), std::end(isoData.opacities), 0.0f);

  if (!settings.isosurfacesVisible()) {
    return;
  }

  const uint32_t activeComp = settings.activeComponent();

  std::size_t i = 0;
  for (const auto& surfaceUid : appData.isosurfaceUids(imageUid, activeComp))
  {
    const Isosurface* surface = m_appData.isosurface(imageUid, activeComp, surfaceUid);
    if (!surface) {
      spdlog::warn("Null isosurface {} for image {}", surfaceUid, imageUid);
      continue;
    }

    if (!surface->visible) {
      continue;
    }

    // Map isovalue from native image intensity to texture intensity:
    isoData.values[i] = static_cast<float>(settings.mapNativeIntensityToTexture(surface->value));

    // The isosurfaces are hidden if the image is hidden
    isoData.opacities[i] = settings.visibility()
      ? surface->opacity * settings.isosurfaceOpacityModulator() : 0.0f;

    isoData.edgeStrengths[i] = surface->edgeStrength;
    isoData.shininesses[i] = surface->material.shininess;

    if (settings.applyImageColormapToIsosurfaces()) {
      // Color the surface using the current image colormap:
      static constexpr bool premult = false;
      const glm::vec3 cmapColor = getIsosurfaceColor(m_appData, *surface, settings, activeComp, premult);
      isoData.ambientLights[i] = surface->material.ambient * cmapColor;
      isoData.diffuseLights[i] = surface->material.diffuse * cmapColor;
      isoData.specularLights[i] = surface->material.specular * WHITE;
    }
    else {
      // Color the surface using its explicitly defined color:
      isoData.ambientLights[i] = surface->ambientColor();
      isoData.diffuseLights[i] = surface->diffuseColor();
      isoData.specularLights[i] = surface->specularColor();
    }

    ++i;
  }
}
