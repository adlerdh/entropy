#include "rendering/RenderData.h"
#include "rendering/physics/XrayAttenuation.h"

#include <spdlog/spdlog.h>

#include <cstddef>

namespace
{
// Define the vertices and indices of a 2D quad:
static constexpr int sk_numQuadVerts = 4;
static constexpr int sk_numQuadPosComps = 2;

// Define the vertices and indices of a 2D circle:
// static constexpr int sk_numCircleVerts = 4;
// static constexpr int sk_numCirclePosComps = 2;

static constexpr int sk_byteOffset = 0;
static constexpr int sk_indexOffset = 0;

static const std::array<float, static_cast<size_t>(sk_numQuadVerts* sk_numQuadPosComps)> sk_clipPosBuffer = {{
  -1.0f,
  -1.0f, // bottom left
  1.0f,
  -1.0f, // bottom right
  -1.0f,
  1.0f, // top left
  1.0f,
  1.0f, // top right
}};

static const std::array<uint32_t, sk_numQuadVerts> sk_indicesBuffer = {{0, 1, 2, 3}};

GLTexture createBlankRgbaTexture(tex::Target target, uint8_t value)
{
  static const ComponentType compType = ComponentType::UInt8;
  static std::array<uint8_t, 4> sk_data_U8 = {value, value, value, value};

  static constexpr GLint sk_mipmapLevel = 0; // Load image data into first mipmap level
  static constexpr GLint sk_alignment = 4;   // Pixel pack/unpack alignment is 4 bytes

  static const tex::WrapMode sk_wrapMode = tex::WrapMode::ClampToEdge;
  static const tex::MinificationFilter sk_minFilter = tex::MinificationFilter::Nearest;
  static const tex::MagnificationFilter sk_maxFilter = tex::MagnificationFilter::Nearest;

  static const glm::uvec3 sk_size{1, 1, 1};

  GLTexture::PixelStoreSettings pixelPackSettings;
  pixelPackSettings.m_alignment = sk_alignment;
  GLTexture::PixelStoreSettings pixelUnpackSettings = pixelPackSettings;

  GLTexture T(target, GLTexture::MultisampleSettings(), pixelPackSettings, pixelUnpackSettings);

  T.generate();
  T.setMinificationFilter(sk_minFilter);
  T.setMagnificationFilter(sk_maxFilter);
  T.setWrapMode(sk_wrapMode);
  T.setAutoGenerateMipmaps(false);
  T.setSize(sk_size);

  T.setData(
    sk_mipmapLevel,
    GLTexture::getSizedInternalRGBAFormat(compType),
    GLTexture::getBufferPixelRGBAFormat(compType),
    GLTexture::getBufferPixelDataType(compType),
    sk_data_U8.data());

  spdlog::debug("Created blank RGBA texture for target {}", static_cast<int>(target));

  return T;
}

GLTexture createBlank3DRgbaTexture(uint8_t value)
{
  return createBlankRgbaTexture(tex::Target::Texture3D, value);
}

GLTexture createBlank2DRgbaTexture(uint8_t value)
{
  return createBlankRgbaTexture(tex::Target::Texture2D, value);
}

} // namespace

RenderData::RenderData()
  : m_blankImageBlackTransparentTexture(createBlank3DRgbaTexture(0))
  , m_blankImageBlackTransparentTexture2D(createBlank2DRgbaTexture(0))
  , m_blankImageWhiteOpaqueTexture(createBlank3DRgbaTexture(255))
  , m_blankSegTexture(createBlank3DRgbaTexture(0))
  , m_blankDistMapTexture(createBlank3DRgbaTexture(0))

  ,

  m_snapCrosshairs(CrosshairsSnapping::Disabled)
  , m_modulateSegOpacityWithImageOpacity(true)
  , m_modulateIsocontourOpacityWithImageOpacity(false)
  , m_imageGrayFloatingPointInterpolationPolicy(FloatingPointLinearInterpolationPolicy::FixedFunction)
  , m_isocontourFloatingPointInterpolationPolicy(FloatingPointLinearInterpolationPolicy::Automatic)
  , m_opacityMixMode(false)
  , m_intensityProjectionSlabThickness(10.0f)
  , m_doMaxExtentIntensityProjection(false) //!< @todo Initialize based on ref image

  , m_xrayIntensityWindow(1.0f)
  , m_xrayIntensityLevel(0.5f)
  , m_xrayEnergyKeV(xray::defaultEnergyKeV())
  , m_waterMassAttenCoeff(xray::linearAttenuationCoefficients(xray::defaultEnergyKeV()).water_cmInv)
  , m_airMassAttenCoeff(xray::linearAttenuationCoefficients(xray::defaultEnergyKeV()).air_cmInv)

  , m_2dBackgroundColor(0.1f, 0.1f, 0.1f)
  , m_3dBackgroundColor(22.0f / 255.0f, 22.0f / 255.0f, 22.0f / 255.0f, 1.0f)
  , m_3dTransparentIfNoHit(true)
  , m_crosshairsColor(0.05f, 0.6f, 1.0f, 1.0f)
  , m_showCrosshairs(true)
  , m_showCrosshairsInLightboxViews(true)
  , m_anatomicalLabelColor(0.695f, 0.870f, 0.090f, 1.0f)
  , m_showAnatomicalLabels(true)
  , m_showAnatomicalLabelsInLightboxViews(true)
  , m_showScaleBars(true)
  , m_showScaleBarsInLightboxViews(false)
  , m_scaleBarColor(0.380392f, 0.858824f, 0.250980f, 1.0f)
  , m_scaleBarPosition(ScaleBarPosition::BottomRight)
  , m_scaleBarOrientation(ScaleBarOrientation::Horizontal)
  , m_scaleBarTicks(ScaleBarTicks::Automatic)
  , m_scaleBarTargetFraction(0.2f)
  , m_scaleBarMarginPx(12.0f)
  , m_showLightboxOffsetLabels(true)
  , m_lightboxOffsetLabelColor(0.75f, 0.75f, 0.75f, 0.8f)

  , m_renderFrontFaces(true)
  , m_renderBackFaces(true)
  , m_raycastSamplingFactor(0.8f)
  , m_adaptiveRaycastSamplingEnabled(false)
  , m_adaptiveRaycastTargetFrameRate(30.0f)
  , m_adaptiveRaycastEffectiveSamplingFactor(0.8f)
  , m_adaptiveRaycastMeasuredFrameRate(0.0f)
  , m_raycastBackgroundEdgeBrighteningEnabled(true)
  , m_showCrosshairsIn3D(true)
  , m_crosshairs3DGlyphDiameterVoxelDiagonals(2.0f)
  , m_showThreeDCameraFrustumIn2DViews(false)
  , m_reverseThreeDRotateAboutEye(false)
  , m_threeDCameraFrustumColor(0x7c / 255.0f, 0x5e / 255.0f, 0xd5 / 255.0f, 0xa2 / 255.0f)
  , m_lastInteractedThreeDViewUid(std::nullopt)
  , m_segMasking(SegMaskingForRaycasting::Disabled)

  , m_squaredDifferenceParams()
  , m_localNccParams()
  , m_localLinearResidualParams()
  , m_jointHistogramParams()

  , m_edgeMagnitudeSmoothing(1.0f, 1.0f)
  , m_numCheckerboardSquares(10)
  , m_overlayMagentaCyan(false)
  , m_quadrants(true, true)
  , m_useSquare(true)

  , m_flashlightRadius(0.15f)
  , m_flashlightOverlays(true)
{
}

void RenderData::setXrayEnergy(float energyKeV)
{
  const auto coefficients = xray::linearAttenuationCoefficients(energyKeV);
  if (coefficients.water_cmInv <= 0.0f || coefficients.air_cmInv <= 0.0f) {
    return;
  }

  m_xrayEnergyKeV = energyKeV;
  m_airMassAttenCoeff = coefficients.air_cmInv;
  m_waterMassAttenCoeff = coefficients.water_cmInv;
}

RenderData::Quad::Quad()
  : m_positionsInfo(
      BufferComponentType::Float,
      BufferNormalizeValues::False,
      sk_numQuadPosComps,
      sk_numQuadPosComps * sizeof(float),
      sk_byteOffset,
      sk_numQuadVerts)

  , m_indicesInfo(IndexType::UInt32, PrimitiveMode::TriangleStrip, sk_numQuadVerts, sk_indexOffset)
  , m_positionsObject(BufferType::VertexArray, BufferUsagePattern::StaticDraw)
  , m_indicesObject(BufferType::Index, BufferUsagePattern::StaticDraw)
  , m_vaoParams(m_indicesInfo)
{
  static constexpr GLuint sk_positionIndex = 0;

  m_positionsObject.generate();
  m_indicesObject.generate();

  m_positionsObject.allocate(
    static_cast<unsigned long>(sk_numQuadVerts * sk_numQuadPosComps) * sizeof(float),
    sk_clipPosBuffer.data());
  m_indicesObject.allocate(sk_numQuadVerts * sizeof(uint32_t), sk_indicesBuffer.data());

  m_vao.generate();
  m_vao.bind();
  {
    m_indicesObject.bind(); // Bind EBO so that it is part of the VAO state

    // Saves binding in VAO, since GL_ARRAY_BUFFER is not part of VAO state.
    // Register position VBO with VAO and set/enable attribute pointer
    m_positionsObject.bind();
    m_vao.setAttributeBuffer(sk_positionIndex, m_positionsInfo);
    m_vao.enableVertexAttribute(sk_positionIndex);
  }
  m_vao.release();

  spdlog::debug("Created image quad vertex array object");
}

RenderData::Circle::Circle()
  : m_positionsInfo(
      BufferComponentType::Float,
      BufferNormalizeValues::False,
      sk_numQuadPosComps,
      sk_numQuadPosComps * sizeof(float),
      sk_byteOffset,
      sk_numQuadVerts)
  , m_indicesInfo(IndexType::UInt32, PrimitiveMode::TriangleStrip, sk_numQuadVerts, sk_indexOffset)
  , m_positionsObject(BufferType::VertexArray, BufferUsagePattern::StaticDraw)
  , m_indicesObject(BufferType::Index, BufferUsagePattern::StaticDraw)
  , m_vaoParams(m_indicesInfo)
{
  static constexpr GLuint sk_positionIndex = 0;

  m_positionsObject.generate();
  m_indicesObject.generate();

  m_positionsObject.allocate(
    static_cast<unsigned long>(sk_numQuadVerts * sk_numQuadPosComps) * sizeof(float),
    sk_clipPosBuffer.data());
  m_indicesObject.allocate(sk_numQuadVerts * sizeof(uint32_t), sk_indicesBuffer.data());

  m_vao.generate();
  m_vao.bind();
  {
    m_indicesObject.bind(); // Bind EBO so that it is part of the VAO state

    // Saves binding in VAO, since GL_ARRAY_BUFFER is not part of VAO state.
    // Register position VBO with VAO and set/enable attribute pointer
    m_positionsObject.bind();
    m_vao.setAttributeBuffer(sk_positionIndex, m_positionsInfo);
    m_vao.enableVertexAttribute(sk_positionIndex);
  }
  m_vao.release();

  spdlog::debug("Created image quad vertex array object");
}

RenderData::IsosurfaceData::IsosurfaceData()
  : values(8, 0.0f)
  , opacities(8, 0.0f)
  , rimOpacityStrengths(8, 0.0f)
  , rimEmissionStrengths(8, 0.0f)
  , rimPowers(8, 2.0f)
  , colors(8, glm::vec3{0.0f})
  , ambient(8, glm::vec3{0.0f})
  , diffuse(8, glm::vec3{0.0f})
  , specular(8, glm::vec3{0.0f})
  , shininesses(8, 0.0f)
{
}
