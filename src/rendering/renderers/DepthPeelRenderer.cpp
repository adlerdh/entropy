#include "rendering/renderers/DepthPeelRenderer.h"

#include "rendering/common/ShaderStageTypes.h"

#include "rendering/drawables/DrawableBase.h"
#include "rendering/drawables/ddp/DdpBlendPassQuad.h"
#include "rendering/drawables/ddp/DdpFinalPassQuad.h"
#include "rendering/drawables/ddp/FullScreenDebugQuad.h"

#include "rendering/utility/gl/GLErrorChecker.h"
#include "rendering/utility/gl/GLFrameBufferObject.h"
#include "rendering/utility/gl/GLTexture.h"

#include "common/Exception.hpp"

#include "logic/camera/CameraHelpers.h"

#include <spdlog/spdlog.h>

#include <glm/glm.hpp>

#include <glad/glad.h>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <array>

/// Functional returning a raw pointer to an IDrawable
using DrawableProviderType = GetterType<IDrawable*>;

namespace
{

// Color buffers used by the DDP renderer
static const std::array<GLenum, 7> sk_buffers = {
  {GL_COLOR_ATTACHMENT0,
   GL_COLOR_ATTACHMENT1,
   GL_COLOR_ATTACHMENT2,
   GL_COLOR_ATTACHMENT3,
   GL_COLOR_ATTACHMENT4,
   GL_COLOR_ATTACHMENT5,
   GL_COLOR_ATTACHMENT6}
};

} // namespace

struct DepthPeelRenderer::Impl
{
  Impl(
    const std::string& name,
    ShaderProgramActivatorType shaderProgramActivator,
    UniformsProviderType uniformsProvider,
    DrawableProviderType sceneRootProvider,
    DrawableProviderType overlayRootProvider
  )
    : m_name(std::move(name))
    , m_sceneRootProvider(sceneRootProvider)
    , m_overlayRootProvider(overlayRootProvider)
    , m_time(0.0)
    ,

    m_maxNumPeels(4)
    , m_useOccQueries(false)
    , m_occlusionRatio(1.0f)
    , m_occlusionThreshold(0.0f)
    , m_occQueryId(0u)
    ,

    m_defaultFboId(0u)
    , m_objectIdFbo("ObjectIdFbo")
    , m_opaqueRenderFbo("OpaqueRenderFbo")
    , m_opaqueResolveFbo("OpaqueResolveFbo")
    , m_depthPeelFbo("DualDepthPeelFbo")
    , m_backBlendFbo("BackBlendFbo")
    ,

    m_enableObjectBuffer(false)
    , m_objectIdBuffer(nullptr)
    , m_objectDepthBuffer(nullptr)
    , m_objectBuffersDirty(true)
    ,

    m_objectIdTexture(
      tex::Target::Texture2D,
      GLTexture::MultisampleSettings(),
      GLTexture::PixelStoreSettings(2, 0, 0, 0, 0, 0, false, false),
      GLTexture::PixelStoreSettings(2, 0, 0, 0, 0, 0, false, false)
    )
    ,

    m_objectDepthTexture(tex::Target::Texture2D)
    ,

    m_opaqueColorTexture(tex::Target::Texture2DMultisample, GLTexture::MultisampleSettings(4, true))
    , m_opaqueDepthTexture(tex::Target::Texture2DMultisample, GLTexture::MultisampleSettings(4, true))
    ,

    m_resolvedDepthTexture(tex::Target::Texture2D)
    ,

    m_depthTextures{{tex::Target::Texture2D, tex::Target::Texture2D}}
    , m_frontBlenderTextures{{tex::Target::Texture2D, tex::Target::Texture2D}}
    , m_backTempTextures{{tex::Target::Texture2D, tex::Target::Texture2D}}
    , m_backBlenderTexture(tex::Target::Texture2D)
    ,

    m_debugQuad("debugQuad", shaderProgramActivator, uniformsProvider)
    ,

    m_blendQuad("blendQuad", shaderProgramActivator, uniformsProvider, m_backTempTextures)
    ,

    m_finalQuad(
      "finalQuad",
      shaderProgramActivator,
      uniformsProvider,
      m_frontBlenderTextures,
      m_backBlenderTexture
    )
  {
  }

  void initialize();
  void render();
  void resize(const Viewport&);
  void teardown();
  void update(const camera::Camera&, const CoordinateFrame&);

  void initializeFbos();
  void generateDefaultTextureAttachment(GLTexture&);
  void initializeTextureAttachments();
  void resizeTextures();

  struct BlendingStatus
  {
    bool m_blendingDone;
    std::optional<uint32_t> m_numSamplesPassed;
  };

  void renderObjectIdsAndDepths();                 // step 0
  void ddp_opaquePass();                           // step 1
  void ddp_resolveMultisampledTextures();          // step 2
  void ddp_clearTargets(bool currentId);           // step 3/5
  void ddp_initializeDepths();                     // step 4
  void ddp_peelFrontBack(bool currentId);          // step 6
  BlendingStatus ddp_blendTargets(bool currentId); // step 7
  void ddp_composeFinal(bool currentId);           // step 8
  void renderOverlays();                           // step 9

  void debugRenderPass(std::shared_ptr<GLTexture> tex);

  void renderScene(const RenderStage&, const ObjectsToRender&);
  void renderSingleOverlayLayer(int layer);

  std::pair<uint16_t, float> pickObjectIdAndNdcDepth(const glm::vec2& ndcPos);

  GLErrorChecker m_errorChecker;

  std::string m_name;
  DrawableProviderType m_sceneRootProvider;
  DrawableProviderType m_overlayRootProvider;

  double m_time;
  Viewport m_viewport;

  /// @todo Put these in RenderParameters struct
  uint32_t m_maxNumPeels;
  bool m_useOccQueries;
  float m_occlusionRatio;
  float m_occlusionThreshold;

  GLuint m_occQueryId;
  GLuint m_defaultFboId;

  GLFrameBufferObject m_objectIdFbo;
  GLFrameBufferObject m_opaqueRenderFbo;
  GLFrameBufferObject m_opaqueResolveFbo;
  GLFrameBufferObject m_depthPeelFbo;
  GLFrameBufferObject m_backBlendFbo;

  bool m_enableObjectBuffer;
  std::unique_ptr<uint16_t[]> m_objectIdBuffer;
  std::unique_ptr<float[]> m_objectDepthBuffer;

  // Flag indicating that the object ID buffer needs to be recomputed,
  // as the scene has been rendered since the object ID buffer was last rendered
  bool m_objectBuffersDirty;

  GLTexture m_objectIdTexture;
  GLTexture m_objectDepthTexture;

  GLTexture m_opaqueColorTexture;
  GLTexture m_opaqueDepthTexture;

  GLTexture m_resolvedDepthTexture;

  std::array<GLTexture, 2> m_depthTextures;
  std::array<GLTexture, 2> m_frontBlenderTextures;
  std::array<GLTexture, 2> m_backTempTextures;
  GLTexture m_backBlenderTexture;

  FullScreenDebugQuad m_debugQuad;
  DdpBlendPassQuad m_blendQuad;
  DdpFinalPassQuad m_finalQuad;
};

DepthPeelRenderer::DepthPeelRenderer(
  const std::string& name,
  ShaderProgramActivatorType programActivator,
  UniformsProviderType uniformsProvider,
  DrawableProviderType rootProvider,
  DrawableProviderType overlayProvider
)
  : m_impl(std::make_unique<Impl>(
    std::move(name), programActivator, uniformsProvider, rootProvider, overlayProvider
  ))
{
}

DepthPeelRenderer::~DepthPeelRenderer() = default;

void DepthPeelRenderer::initialize()
{
  if (!m_impl)
  {
    throw_debug("Null implementation");
  }
  m_impl->initialize();

  GLint drawFboId = 0, readFboId = 0;
  glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &drawFboId);
  glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &readFboId);

  spdlog::trace("Draw FBO ID = {}; Read FBO ID = {}", drawFboId, readFboId);

  m_impl->m_defaultFboId = static_cast<GLuint>(drawFboId);
}

void DepthPeelRenderer::render()
{
  if (!m_impl)
  {
    throw_debug("Null implementation");
  }
  m_impl->render();
}

void DepthPeelRenderer::resize(const Viewport& viewport)
{
  if (!m_impl)
  {
    throw_debug("Null implementation");
  }
  m_impl->resize(viewport);
}

void DepthPeelRenderer::teardown()
{
  if (!m_impl)
  {
    throw_debug("Null implementation");
  }
  m_impl->teardown();
}

void DepthPeelRenderer::update(const camera::Camera& camera, const CoordinateFrame& crosshairs)
{
  if (!m_impl)
  {
    throw_debug("Null implementation");
  }
  m_impl->update(camera, crosshairs);
}

void DepthPeelRenderer::setMaxNumberOfPeels(uint32_t num)
{
  if (!m_impl)
  {
    throw_debug("Null implementation");
  }
  if (num > 0)
  {
    m_impl->m_maxNumPeels = num;
  }
}

void DepthPeelRenderer::setOcclusionRatio(float ratio)
{
  if (!m_impl)
  {
    throw_debug("Null implementation");
  }

  if (0.0f <= ratio && ratio <= 1.0f)
  {
    // Only use occlusion queries if the ratio is less than one
    m_impl->m_useOccQueries = (ratio < 1.0f) ? true : false;
    m_impl->m_occlusionRatio = ratio;
  }
}

void DepthPeelRenderer::setEnablePointPicking(bool enable)
{
  if (!m_impl)
  {
    throw_debug("Null implementation");
  }
  m_impl->m_enableObjectBuffer = enable;
}

std::pair<uint16_t, float> DepthPeelRenderer::pickObjectIdAndNdcDepth(const glm::vec2& ndcPos)
{
  if (!m_impl)
  {
    throw_debug("Null implementation");
  }
  return m_impl->pickObjectIdAndNdcDepth(ndcPos);
}

/* IMPLEMENTATION FUNCTIONS */

void DepthPeelRenderer::Impl::renderScene(const RenderStage& stage, const ObjectsToRender& objects)
{
  auto root = m_sceneRootProvider();
  if (!root)
  {
    return;
  }

  // Define polygon front faces to have CCW orientation, but always draw front and back polygon
  // faces using fill mode. Back-face culling is disabled for two reasons:
  // 1) The view camera can enter the inside of objects;
  // 2) Meshes can be transparent

  glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
  glDisable(GL_CULL_FACE);
  glFrontFace(GL_CCW);

  // Disable depth clamping, which is a feature that causes GL to ignore near and far plane
  // clipping by projecting vertices onto those plances instead of being clipped. We do not
  // want depth clamping for two reasons:
  // 1) The view camera can enter the inside of objects;
  // 2) Objects can be positioned behind the camera

  glDisable(GL_DEPTH_CLAMP);

  // Render the scene by executing render() call on the top level Drawable!
  root->render(stage, objects);
}

void DepthPeelRenderer::Impl::renderSingleOverlayLayer(int /*layer*/)
{
  auto root = m_overlayRootProvider();
  if (!root)
  {
    return;
  }

  glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
  glDisable(GL_CULL_FACE);
  glFrontFace(GL_CCW);
  glDisable(GL_DEPTH_CLAMP);

  // Render the overlays by executing render() call on the top level Drawable
  root->render(RenderStage::Overlay, ObjectsToRender::All);
}

/// Step 0: Render opaque object IDs and their fragment depths
void DepthPeelRenderer::Impl::renderObjectIdsAndDepths()
{
  // Render colors, object IDs, and fragment depths to
  // { m_backBlenderTexture, m_objectIdTexture, m_objectDepthBuffer }

  m_objectIdFbo.bind(fbo::TargetType::DrawAndRead);

  glDrawBuffers(2, &sk_buffers[0]);
  glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  // The purpose of this render pass is to capture the fragment depths and object IDs for all
  // OPAQUE objects. Therefore, depth testing is enabled and blending is disabled.

  glEnable(GL_DEPTH_TEST);
  glDepthMask(GL_TRUE);
  glDisable(GL_BLEND);

  renderScene(RenderStage::Opaque, ObjectsToRender::Pickable);

  m_objectBuffersDirty = true;
}

/// Step 1: Render only all opaque geometry in order to bound the depth peels
void DepthPeelRenderer::Impl::ddp_opaquePass()
{
  m_opaqueRenderFbo.bind(fbo::TargetType::DrawAndRead);

  glDrawBuffer(sk_buffers[0]);
  glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  // This pass renders opaque geometry, so multisampling, depth testing, and writing to the
  // depth mask are enabled. Blending is disabled.

  glEnable(GL_MULTISAMPLE);
  glEnable(GL_DEPTH_TEST);
  glDepthMask(GL_TRUE);
  glDisable(GL_BLEND);

  // Enable the first clip plane
  /// @note No longer in OpenGL
  ////////// glEnable( GL_CLIP_PLANE0 );

  renderScene(RenderStage::Opaque, ObjectsToRender::Opaque);
}

/// Step 2: Resolve the multi-sampled opaque depth buffer
void DepthPeelRenderer::Impl::ddp_resolveMultisampledTextures()
{
  const auto size = m_opaqueColorTexture.size();

  m_opaqueRenderFbo.bind(fbo::TargetType::Read);
  m_opaqueResolveFbo.bind(fbo::TargetType::Draw);

  glReadBuffer(sk_buffers[0]);
  glDrawBuffer(sk_buffers[0]);

  const GLint sx = static_cast<GLint>(size.x);
  const GLint sy = static_cast<GLint>(size.y);

  // Blit operation writes to { m_backBlenderTexture, m_resolvedDepthTexture }
  glBlitFramebuffer(0, 0, sx, sy, 0, 0, sx, sy, GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT, GL_NEAREST);

  // We are now done with rendering opaque geometry, so disable multisampling, depth testing,
  // and writing to the depth mask are enabled. Blending is now enabled for depth peeling.

  glDisable(GL_MULTISAMPLE);
  glDisable(GL_DEPTH_TEST);
  glEnable(GL_BLEND);

  // Writing to the depth mask is kept enabled for the DDP algorithm.
  glDepthMask(GL_TRUE);
}

/// Steps 3 and 5: Initialize the DDP render targets
void DepthPeelRenderer::Impl::ddp_clearTargets(bool currentId)
{
  // Maximum depth that is used for clearing the depth buffer texture
  static constexpr float sk_maxDepth = 1.0f;

  const uint32_t bufferOffset = 3 * currentId;

  m_depthPeelFbo.bind(fbo::TargetType::DrawAndRead);

  // Writes to { m_depthTextures[1/0] } when peel is even/odd:
  glDrawBuffer(sk_buffers[bufferOffset]);
  glClearColor(-sk_maxDepth, -sk_maxDepth, 0.0f, 0.0f);
  glClear(GL_COLOR_BUFFER_BIT);

  // Writes to { m_frontBlenderTextures[1/0], m_backTempTextures[1/0] } when peel is even/odd:
  glDrawBuffers(2, &sk_buffers[bufferOffset + 1]);
  glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
  glClear(GL_COLOR_BUFFER_BIT);
}

/// Step 4: Render scene with the DDP depth initialization shader
void DepthPeelRenderer::Impl::ddp_initializeDepths()
{
  // Writes to { m_depthTextures[0] }
  glDrawBuffer(sk_buffers[0]);

  // DDP using MAX blending:
  glBlendEquation(GL_MAX);

  m_resolvedDepthTexture.bind(DrawableBase::OpaqueDepthTexSamplerIndex.index);

  renderScene(RenderStage::Initialize, ObjectsToRender::Translucent);
}

/// Step 6: Peel away frontmost and backmost depth layers
void DepthPeelRenderer::Impl::ddp_peelFrontBack(bool currentId)
{
  const uint32_t bufferOffset = 3 * currentId;
  const uint32_t previousId = 1 - currentId;

  // Writes to { m_depthTextures[1/0], m_frontBlenderTextures[1/0], m_backTempTextures[1/0] }
  // when peel is even/odd:
  glDrawBuffers(3, &sk_buffers[bufferOffset]);

  // DDP uses MAX blending
  glBlendEquation(GL_MAX);

  // Global texture bindings
  m_depthTextures[previousId].bind(DrawableBase::DepthBlenderTexSamplerIndex.index);
  m_frontBlenderTextures[previousId].bind(DrawableBase::FrontBlenderTexSamplerIndex.index);

  renderScene(RenderStage::DepthPeel, ObjectsToRender::Translucent);
}

/// Step 7: Full-screen pass to alpha-blend the back color
DepthPeelRenderer::Impl::BlendingStatus DepthPeelRenderer::Impl::ddp_blendTargets(bool currentId)
{
  // Writes to { m_backBlenderTexture }
  glDrawBuffer(sk_buffers[6]);

  // We use "OVER" compositing mode with the convention that RGB colors are pre-multipled by
  // their alpha component:
  // blendedRGBA = 1.0 * frontRGBA + (1.0 - frontA) * backRGBA

  glBlendEquation(GL_FUNC_ADD);
  glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

  if (m_useOccQueries)
  {
    /// @note Could also query GL_ANY_SAMPLES_PASSED
    glBeginQuery(GL_SAMPLES_PASSED, m_occQueryId);
  }

  m_blendQuad.setCurrentTextureId(currentId);
  m_blendQuad.render(RenderStage::QuadResolve, ObjectsToRender::All);

  bool isBlendingDone = false;
  std::optional<uint32_t> numSamplesPassed = std::nullopt;

  if (m_useOccQueries)
  {
    glEndQuery(GL_SAMPLES_PASSED);

    GLuint result;
    glGetQueryObjectuiv(m_occQueryId, GL_QUERY_RESULT, &result);
    numSamplesPassed = result;

    // std::cout << "numSamplesPassed = " << *numSamplesPassed << std::endl;

    isBlendingDone = (static_cast<float>(*numSamplesPassed) <= m_occlusionThreshold);
  }

  return {isBlendingDone, numSamplesPassed};
}

/// Step 8: Compose final front color over final back color and render to default FBO
void DepthPeelRenderer::Impl::ddp_composeFinal(bool currentId)
{
  glDisable(GL_BLEND);

  glBindFramebuffer(GL_FRAMEBUFFER, m_defaultFboId);

  glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  m_finalQuad.setCurrentTextureId(currentId);
  m_finalQuad.render(RenderStage::QuadResolve, ObjectsToRender::All);
}

/// Step 9: Render overlay layers atop the final front color.
/// Render to the default FBO, which gives us multisampling.
void DepthPeelRenderer::Impl::renderOverlays()
{
  // Enable multisampling for nice anti-aliased edges in overlays
  glEnable(GL_MULTISAMPLE);

  // Disable both depth testing and writing to depth buffer
  glDisable(GL_DEPTH_TEST);
  glDepthMask(GL_FALSE);

  // Enable "over" blending
  glEnable(GL_BLEND);
  glBlendEquation(GL_FUNC_ADD);
  glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

  glBindFramebuffer(GL_FRAMEBUFFER, m_defaultFboId);

  // Render translucent layers, sorted furthest to nearest
  renderSingleOverlayLayer(0);

  glEnable(GL_DEPTH_TEST);
  glDepthMask(GL_TRUE);
}

std::pair<uint16_t, float> DepthPeelRenderer::Impl::pickObjectIdAndNdcDepth(const glm::vec2& ndcPos)
{
  static const std::pair<uint16_t, float> sk_none(0u, -1.0f);

  if (!m_enableObjectBuffer)
  {
    return sk_none;
  }

  const glm::vec2 viewPos(camera::viewDevice_T_ndc(m_viewport, ndcPos));

  if (viewPos.x < 0.0f || viewPos.y < 0.0f)
  {
    return sk_none;
  }

  const glm::uvec2 viewPos_uint = glm::round(viewPos);

  if (viewPos_uint.x >= m_objectIdTexture.size().x || viewPos_uint.y >= m_objectIdTexture.size().y)
  {
    return sk_none;
  }

  if (m_objectBuffersDirty)
  {
    // Object IDs are stored as 16-bit integers
    m_objectIdTexture.readData(
      0,
      tex::BufferPixelFormat::Red_Integer,
      tex::BufferPixelDataType::UInt16,
      m_objectIdBuffer.get()
    );

    m_objectDepthTexture.readData(
      0,
      tex::BufferPixelFormat::DepthComponent,
      tex::BufferPixelDataType::Float32,
      m_objectDepthBuffer.get()
    );

    m_objectBuffersDirty = false;
  }

  const size_t index = viewPos_uint.x + m_objectIdTexture.size().x * viewPos_uint.y;

  const uint16_t id = m_objectIdBuffer[index];
  const float ndcZ = camera::convertOpenGlDepthToNdc(m_objectDepthBuffer[index]);

  return std::make_pair(id, ndcZ);
}

void DepthPeelRenderer::Impl::debugRenderPass(std::shared_ptr<GLTexture> texture)
{
  glDisable(GL_BLEND);

  glBindFramebuffer(GL_FRAMEBUFFER, m_defaultFboId);
  glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

  m_debugQuad.setTexture(texture);

  m_debugQuad.render(RenderStage::QuadResolve, ObjectsToRender::All);
}

void DepthPeelRenderer::Impl::generateDefaultTextureAttachment(GLTexture& texture)
{
  texture.generate();
  texture.setMinificationFilter(tex::MinificationFilter::Nearest);
  texture.setMagnificationFilter(tex::MagnificationFilter::Nearest);
  texture.setWrapMode(tex::WrapMode::ClampToEdge);
}

void DepthPeelRenderer::Impl::initialize()
{
  //    initializeOpenGLFunctions();

  initializeTextureAttachments();
  initializeFbos();

  glGenQueries(1, &m_occQueryId);
}

void DepthPeelRenderer::Impl::initializeTextureAttachments()
{
  generateDefaultTextureAttachment(m_objectIdTexture);

  // The opaque color/depth textures are multisampled targets, and so take no
  // minification or magnification filter parameters
  m_opaqueColorTexture.generate();
  m_opaqueDepthTexture.generate();

  generateDefaultTextureAttachment(m_objectDepthTexture);
  generateDefaultTextureAttachment(m_resolvedDepthTexture);

  for (uint32_t i = 0; i < 2; ++i)
  {
    generateDefaultTextureAttachment(m_depthTextures[i]);
    generateDefaultTextureAttachment(m_frontBlenderTextures[i]);
    generateDefaultTextureAttachment(m_backTempTextures[i]);
  }

  generateDefaultTextureAttachment(m_backBlenderTexture);

  resizeTextures();
}

void DepthPeelRenderer::Impl::resizeTextures()
{
  using namespace tex;

  // Float32 buffers take 112 bytes per pixel // recount!
  // Int8 buffers take 46 bytes per pixel // recount!
  static constexpr bool k_useF32Buffers = false;

  static constexpr auto k_rgbaInternalFormat = (k_useF32Buffers) ? SizedInternalFormat::RGBA32F
                                                                 : SizedInternalFormat::RGBA8_UNorm;

  static constexpr auto k_pixelDataType = (k_useF32Buffers) ? BufferPixelDataType::Float32
                                                            : BufferPixelDataType::UInt8;

  static constexpr GLint k_level = 0;

  const glm::uvec3 k_textureSize{m_viewport.deviceWidth(), m_viewport.deviceHeight(), 1};

  /// @todo uint8_t data must be changed to float if F32 buffers are used
  const std::vector<float> sk_emptyDepthData(2 * k_textureSize.x * k_textureSize.y, 0.0f);
  const std::vector<uint8_t> sk_emptyColorData(4 * k_textureSize.x * k_textureSize.y, 0);
  const std::vector<uint16_t> sk_emptyObjectIDData(k_textureSize.x * k_textureSize.y, 0);
  const std::vector<float> sk_emptyObjectDepthData(k_textureSize.x * k_textureSize.y, 1.0f);

  // Total: F32: 640 bits; U8: 256 bits
  for (uint32_t i = 0; i < 2; ++i)
  {
    m_depthTextures[i].setSize(k_textureSize);
    m_frontBlenderTextures[i].setSize(k_textureSize);
    m_backTempTextures[i].setSize(k_textureSize);

    // F32: 2 * 2 * 32 bits; U8: 2 * 2 * 32 bits
    m_depthTextures[i].setData(
      k_level,
      SizedInternalFormat::RG32F,
      BufferPixelFormat::RG,
      BufferPixelDataType::Float32,
      sk_emptyDepthData.data()
    );

    // F32: 2 * 4 * 32 bits; U8: 2 * 4 * 8 bits
    m_frontBlenderTextures[i].setData(
      k_level,
      k_rgbaInternalFormat,
      BufferPixelFormat::RGBA,
      k_pixelDataType,
      sk_emptyColorData.data()
    );

    // F32: 2 * 4 * 32 bits; U8: 2 * 4 * 8 bits
    m_backTempTextures[i].setData(
      k_level,
      k_rgbaInternalFormat,
      BufferPixelFormat::RGBA,
      k_pixelDataType,
      sk_emptyColorData.data()
    );
  }

  m_backBlenderTexture.setSize(k_textureSize);

  // F32: 4 * 32 bits; U8: 4 * 8
  m_backBlenderTexture.setData(
    k_level, k_rgbaInternalFormat, BufferPixelFormat::RGBA, k_pixelDataType, sk_emptyColorData.data()
  );

  m_objectIdTexture.setSize(k_textureSize);
  m_objectDepthTexture.setSize(k_textureSize);

  m_opaqueColorTexture.setSize(k_textureSize);
  m_opaqueDepthTexture.setSize(k_textureSize);
  m_resolvedDepthTexture.setSize(k_textureSize);

  // 16 bits
  m_objectIdTexture.setData(
    k_level,
    SizedInternalFormat::R16U,
    BufferPixelFormat::Red_Integer,
    BufferPixelDataType::UInt16,
    sk_emptyObjectIDData.data()
  );

  // 32 bits
  m_objectDepthTexture.setData(
    k_level,
    SizedInternalFormat::Depth32F,
    BufferPixelFormat::DepthComponent,
    BufferPixelDataType::Float32,
    sk_emptyObjectDepthData.data()
  );

  // 4 * 32 bits; U8: 4 * 8
  m_opaqueColorTexture.setData(
    k_level,
    k_rgbaInternalFormat,
    BufferPixelFormat::RGBA,
    k_pixelDataType,
    sk_emptyObjectDepthData.data()
  );

  // 32 bits
  m_opaqueDepthTexture.setData(
    k_level,
    SizedInternalFormat::Depth32F,
    BufferPixelFormat::DepthComponent,
    BufferPixelDataType::Float32,
    sk_emptyObjectDepthData.data()
  );

  // 32 bits
  m_resolvedDepthTexture.setData(
    k_level,
    SizedInternalFormat::Depth32F,
    BufferPixelFormat::DepthComponent,
    BufferPixelDataType::Float32,
    nullptr
  );

  m_objectIdBuffer = std::make_unique<uint16_t[]>(k_textureSize.x * k_textureSize.y);
  m_objectDepthBuffer = std::make_unique<float[]>(k_textureSize.x * k_textureSize.y);
}

void DepthPeelRenderer::Impl::initializeFbos()
{
  using namespace fbo;

  m_objectIdFbo.generate();
  m_objectIdFbo.bind(TargetType::DrawAndRead);
  m_objectIdFbo.attach2DTexture(TargetType::Draw, AttachmentType::Color, m_backBlenderTexture, 0);
  m_objectIdFbo.attach2DTexture(TargetType::Draw, AttachmentType::Color, m_objectIdTexture, 1);
  m_objectIdFbo.attach2DTexture(TargetType::Draw, AttachmentType::Depth, m_objectDepthTexture);

  // bind color and depth attachments for the opaque object render pass
  m_opaqueRenderFbo.generate();
  m_opaqueRenderFbo.bind(TargetType::DrawAndRead);
  m_opaqueRenderFbo.attach2DTexture(TargetType::Draw, AttachmentType::Color, m_opaqueColorTexture, 0);
  m_opaqueRenderFbo.attach2DTexture(TargetType::Draw, AttachmentType::Depth, m_opaqueDepthTexture);

  // bind color and depth attachments for the opaque object resolve pass
  m_opaqueResolveFbo.generate();
  m_opaqueResolveFbo.bind(TargetType::DrawAndRead);
  m_opaqueResolveFbo
    .attach2DTexture(TargetType::Draw, AttachmentType::Color, m_backBlenderTexture, 0);
  m_opaqueResolveFbo.attach2DTexture(TargetType::Draw, AttachmentType::Depth, m_resolvedDepthTexture);

  // bind the seven color attachments for the depth peeling pass
  m_depthPeelFbo.generate();
  m_depthPeelFbo.bind(TargetType::DrawAndRead);
  m_depthPeelFbo.attach2DTexture(TargetType::Draw, AttachmentType::Color, m_depthTextures[0], 0);
  m_depthPeelFbo
    .attach2DTexture(TargetType::Draw, AttachmentType::Color, m_frontBlenderTextures[0], 1);
  m_depthPeelFbo.attach2DTexture(TargetType::Draw, AttachmentType::Color, m_backTempTextures[0], 2);
  m_depthPeelFbo.attach2DTexture(TargetType::Draw, AttachmentType::Color, m_depthTextures[1], 3);
  m_depthPeelFbo
    .attach2DTexture(TargetType::Draw, AttachmentType::Color, m_frontBlenderTextures[1], 4);
  m_depthPeelFbo.attach2DTexture(TargetType::Draw, AttachmentType::Color, m_backTempTextures[1], 5);
  m_depthPeelFbo.attach2DTexture(TargetType::Draw, AttachmentType::Color, m_backBlenderTexture, 6);

  // set the texture attachment to the color blend pass
  m_backBlendFbo.generate();
  m_backBlendFbo.bind(TargetType::DrawAndRead);
  m_backBlendFbo.attach2DTexture(TargetType::Draw, AttachmentType::Color, m_backBlenderTexture, 0);

  glBindFramebuffer(GL_FRAMEBUFFER, m_defaultFboId);
}

void DepthPeelRenderer::Impl::render()
{
  // Get the OpenGL ID of the default FBO used by Qt.
  // Do this every render call, in case it changes for some reason.
  //////////////////////////m_defaultFboId = QOpenGLContext::currentContext()->defaultFramebufferObject();
  //    m_defaultFboId = 0; /// @note is this correct?
  //    GLFWwindow* window = glfwGetCurrentContext();

  glViewport(
    static_cast<GLint>(m_viewport.deviceLeft()),
    static_cast<GLint>(m_viewport.deviceBottom()),
    static_cast<GLint>(m_viewport.deviceWidth()),
    static_cast<GLint>(m_viewport.deviceHeight())
  );

  // This forces a render of object ID every time!
  // We were having problems with not rendering prior to mouse press in crosshairs mode
  if (m_enableObjectBuffer)
  {
    /// @todo Optimize by not re-rendering object buffers if scene hasn't changed!

    // STEP 0: Render object IDs and depths
    renderObjectIdsAndDepths();
  }

  // STEP 1: Render color and depth of opaque objects to multisampled texture buffers
  ddp_opaquePass();

  // STEP 2: Resolve multisampled texture buffers to non-multisampled textures by blitting
  ddp_resolveMultisampledTextures();

  // STEP 3: Initialize the DDP render targets
  ddp_clearTargets(0);

  // STEP 4: Render scene with the DDP depth initialization shader
  ddp_initializeDepths();

  // STEPS 5, 6, 7: Iterative dual depth peeling and blending loop
  bool currentId = 0;

  uint32_t lastNumSamplesPassed = std::numeric_limits<uint32_t>::max();

  //    std::cout << "\nname = " << m_name << std::endl;

  for (uint32_t peel = 0; (m_useOccQueries || peel < m_maxNumPeels); ++peel)
  {
    //        std::cout << "peel = " << peel << std::endl;

    // Alternate the draw color attachments between peels
    currentId = (peel + 1) % 2;

    // STEP 5: Initialize buffers
    ddp_clearTargets(currentId);

    // STEP 6: Peel away frontmost and backmost depth layers
    ddp_peelFrontBack(currentId);

    // STEP 7: Full-screen pass to alpha-blend the back color
    const BlendingStatus status = ddp_blendTargets(currentId);

    if (true == status.m_blendingDone)
    {
      break;
    }
    else
    {
      if (status.m_numSamplesPassed)
      {
        if (lastNumSamplesPassed <= *status.m_numSamplesPassed)
        {
          // If we are using occlusion queries, then perform a check on the
          // number of samples that have passed the query. If the number of
          // passed samples have increased this render peel compared to last peel,
          // then something has gone wrong and we should stop peeling.
          break;
        }

        lastNumSamplesPassed = *status.m_numSamplesPassed;
      }
    }
  }

  // STEP 8: Compose final front color over final back color to the view's default FBO
  ddp_composeFinal(currentId);

  // STEP 9: Render overlay layers
  renderOverlays();
}

void DepthPeelRenderer::Impl::teardown()
{
  glDeleteQueries(1, &m_occQueryId);
}

void DepthPeelRenderer::Impl::resize(const Viewport& viewport)
{
  m_viewport = viewport;
  m_occlusionThreshold = m_occlusionRatio * m_viewport.deviceArea();

  resizeTextures();

  m_objectBuffersDirty = true;
}

void DepthPeelRenderer::Impl::update(const camera::Camera& camera, const CoordinateFrame& crosshairs)
{
  // Start traversing the drawable tree with identity transformation, full opacity multiplier,
  // and pickable property set to true.
  static const glm::mat4 sk_ident{1.0f};
  static constexpr float sk_fullOpacity = 1.0f;
  static constexpr bool sk_pickable = true;

  static const AccumulatedRenderingData rootData{sk_ident, sk_fullOpacity, sk_pickable};

  if (!m_sceneRootProvider() || !m_overlayRootProvider())
  {
    return;
  }

  if (auto sceneRoot = m_sceneRootProvider())
  {
    sceneRoot->update(m_time, m_viewport, camera, crosshairs, rootData);
  }

  if (auto overlayRoot = m_overlayRootProvider())
  {
    overlayRoot->update(m_time, m_viewport, camera, crosshairs, rootData);
  }
}
