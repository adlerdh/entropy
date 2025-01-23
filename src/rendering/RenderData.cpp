#include "rendering/RenderData.h"
#include "common/MathFuncs.h"

#include <spdlog/spdlog.h>

namespace
{

// Define the vertices and indices of a 2D quad:
static constexpr int sk_numQuadVerts = 4;
static constexpr int sk_numQuadPosComps = 2;

// Define the vertices and indices of a 2D circle:
//static constexpr int sk_numCircleVerts = 4;
//static constexpr int sk_numCirclePosComps = 2;

static constexpr int sk_byteOffset = 0;
static constexpr int sk_indexOffset = 0;

static const std::array<float, sk_numQuadVerts * sk_numQuadPosComps> sk_clipPosBuffer = {{
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

// Densities of air and water in units [g/cm^3]
static constexpr float AIR_DENSITY = 1.225e-3f;
static constexpr float WATER_DENSITY = 1.0f;

static constexpr float DEFAULT_XRAY_ENERGY = 80.0f; // in KeV units

// Convert photon mass attenuation coefficient (μ/ρ) in units [cm^2/g] to units [1/cm] by
// multiplying by the density.
static constexpr float DEFAULT_MAC_AIR = 1.541E-01f * AIR_DENSITY;
static constexpr float DEFAULT_MAC_WATER = 1.707E-01f * WATER_DENSITY;

GLTexture createBlankRgbaTexture(uint8_t value)
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

  GLTexture
    T(tex::Target::Texture3D,
      GLTexture::MultisampleSettings(),
      pixelPackSettings,
      pixelUnpackSettings);

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
    sk_data_U8.data()
  );

  spdlog::debug("Created blank RGBA texture");

  return T;
}

} // namespace

const std::map<float, float> RenderData::msk_waterMassAttenCoeffs{
  {1.00000E-03, 4.078E+03}, {1.50000E-03, 1.376E+03}, {2.00000E-03, 6.173E+02},
  {3.00000E-03, 1.929E+02}, {4.00000E-03, 8.278E+01}, {5.00000E-03, 4.258E+01},
  {6.00000E-03, 2.464E+01}, {8.00000E-03, 1.037E+01}, {1.00000E-02, 5.329E+00},
  {1.50000E-02, 1.673E+00}, {2.00000E-02, 8.096E-01}, {3.00000E-02, 3.756E-01},
  {4.00000E-02, 2.683E-01}, {5.00000E-02, 2.269E-01}, {6.00000E-02, 2.059E-01},
  {8.00000E-02, 1.837E-01}, {1.00000E-01, 1.707E-01}, {1.50000E-01, 1.505E-01},
  {2.00000E-01, 1.370E-01}, {3.00000E-01, 1.186E-01}, {4.00000E-01, 1.061E-01},
  {5.00000E-01, 9.687E-02}, {6.00000E-01, 8.956E-02}, {8.00000E-01, 7.865E-02},
  {1.00000E+00, 7.072E-02}, {1.25000E+00, 6.323E-02}, {1.50000E+00, 5.754E-02},
  {2.00000E+00, 4.942E-02}, {3.00000E+00, 3.969E-02}, {4.00000E+00, 3.403E-02},
  {5.00000E+00, 3.031E-02}, {6.00000E+00, 2.770E-02}, {8.00000E+00, 2.429E-02},
  {1.00000E+01, 2.219E-02}, {1.50000E+01, 1.941E-02}, {2.00000E+01, 1.813E-02}
};

const std::map<float, float> RenderData::msk_airMassAttenCoeffs{
  {1.00000E-03, 3.606E+03}, {1.50000E-03, 1.191E+03}, {2.00000E-03, 5.279E+02},
  {3.00000E-03, 1.625E+02}, {3.20290E-03, 1.340E+02}, {3.20290E-03, 1.485E+02},
  {4.00000E-03, 7.788E+01}, {5.00000E-03, 4.027E+01}, {6.00000E-03, 2.341E+01},
  {8.00000E-03, 9.921E+00}, {1.00000E-02, 5.120E+00}, {1.50000E-02, 1.614E+00},
  {2.00000E-02, 7.779E-01}, {3.00000E-02, 3.538E-01}, {4.00000E-02, 2.485E-01},
  {5.00000E-02, 2.080E-01}, {6.00000E-02, 1.875E-01}, {8.00000E-02, 1.662E-01},
  {1.00000E-01, 1.541E-01}, {1.50000E-01, 1.356E-01}, {2.00000E-01, 1.233E-01},
  {3.00000E-01, 1.067E-01}, {4.00000E-01, 9.549E-02}, {5.00000E-01, 8.712E-02},
  {6.00000E-01, 8.055E-02}, {8.00000E-01, 7.074E-02}, {1.00000E+00, 6.358E-02},
  {1.25000E+00, 5.687E-02}, {1.50000E+00, 5.175E-02}, {2.00000E+00, 4.447E-02},
  {3.00000E+00, 3.581E-02}, {4.00000E+00, 3.079E-02}, {5.00000E+00, 2.751E-02},
  {6.00000E+00, 2.522E-02}, {8.00000E+00, 2.225E-02}, {1.00000E+01, 2.045E-02},
  {1.50000E+01, 1.810E-02}, {2.00000E+01, 1.705E-02}
};

RenderData::RenderData()
  : m_quad()
  , m_circle()
  ,

  m_imageTextures()
  , m_distanceMapTextures()
  , m_segTextures()
  , m_labelBufferTextures()
  , m_colormapTextures()
  ,

  m_blankImageBlackTransparentTexture(createBlankRgbaTexture(0))
  , m_blankImageWhiteOpaqueTexture(createBlankRgbaTexture(255))
  ,

  m_blankSegTexture(createBlankRgbaTexture(0))
  , m_blankDistMapTexture(createBlankRgbaTexture(0))
  ,

  m_uniforms()
  ,

  m_snapCrosshairs(CrosshairsSnapping::Disabled)
  , m_maskedImages(false)
  , m_modulateSegOpacityWithImageOpacity(true)
  , m_opacityMixMode(false)
  , m_intensityProjectionSlabThickness(10.0f)
  , /// @todo Initialize based on ref image
  m_doMaxExtentIntensityProjection(false)
  ,

  m_xrayIntensityWindow(1.0f)
  , m_xrayIntensityLevel(0.5f)
  ,

  m_xrayEnergyKeV(DEFAULT_XRAY_ENERGY)
  , m_waterMassAttenCoeff(DEFAULT_MAC_WATER)
  , m_airMassAttenCoeff(DEFAULT_MAC_AIR)
  ,

  m_2dBackgroundColor(0.1f, 0.1f, 0.1f)
  , m_3dBackgroundColor(0.0f, 0.0f, 0.0f, 0.5f)
  , m_3dTransparentIfNoHit(true)
  , m_crosshairsColor(0.05f, 0.6f, 1.0f, 1.0f)
  , m_anatomicalLabelColor(0.695f, 0.870f, 0.090f, 1.0f)
  ,

  m_anatomicalLabelType(AnatomicalLabelType::Human)
  ,

  m_renderFrontFaces(true)
  , m_renderBackFaces(true)
  ,

  m_raycastSamplingFactor(0.5f)
  ,

  m_segMasking(SegMaskingForRaycasting::Disabled)
  ,

  m_segOutlineStyle(SegmentationOutlineStyle::Disabled)
  , m_segInterpolation(SegmentationInterpolation::NearestNeighbor)
  , m_segInteriorOpacity(0.10f)
  , m_segInterpCutoff(0.50f)
  ,

  m_squaredDifferenceParams()
  , m_crossCorrelationParams()
  , m_jointHistogramParams()
  ,

  m_edgeMagnitudeSmoothing(1.0f, 1.0f)
  , m_numCheckerboardSquares(10)
  , m_overlayMagentaCyan(true)
  , m_quadrants(true, true)
  , m_useSquare(true)
  ,

  m_flashlightRadius(0.15f)
  , m_flashlightOverlays(true)
{
}

void RenderData::setXrayEnergy(float energyKeV)
{
  const float MeV = energyKeV / 1000.0f;

  if (MeV < msk_airMassAttenCoeffs.begin()->first || msk_airMassAttenCoeffs.rbegin()->first < MeV)
  {
    return;
  }

  m_xrayEnergyKeV = energyKeV;
  m_airMassAttenCoeff = math::interpolate(MeV, msk_airMassAttenCoeffs) * AIR_DENSITY;
  m_waterMassAttenCoeff = math::interpolate(MeV, msk_waterMassAttenCoeffs) * WATER_DENSITY;
}

RenderData::Quad::Quad()
  : m_positionsInfo(
    BufferComponentType::Float,
    BufferNormalizeValues::False,
    sk_numQuadPosComps,
    sk_numQuadPosComps * sizeof(float),
    sk_byteOffset,
    sk_numQuadVerts
  )
  ,

  m_indicesInfo(IndexType::UInt32, PrimitiveMode::TriangleStrip, sk_numQuadVerts, sk_indexOffset)
  ,

  m_positionsObject(BufferType::VertexArray, BufferUsagePattern::StaticDraw)
  , m_indicesObject(BufferType::Index, BufferUsagePattern::StaticDraw)
  ,

  m_vaoParams(m_indicesInfo)
{
  static constexpr GLuint sk_positionIndex = 0;

  m_positionsObject.generate();
  m_indicesObject.generate();

  m_positionsObject
    .allocate(sk_numQuadVerts * sk_numQuadPosComps * sizeof(float), sk_clipPosBuffer.data());
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
    sk_numQuadVerts
  )
  ,

  m_indicesInfo(IndexType::UInt32, PrimitiveMode::TriangleStrip, sk_numQuadVerts, sk_indexOffset)
  ,

  m_positionsObject(BufferType::VertexArray, BufferUsagePattern::StaticDraw)
  , m_indicesObject(BufferType::Index, BufferUsagePattern::StaticDraw)
  ,

  m_vaoParams(m_indicesInfo)
{
  static constexpr GLuint sk_positionIndex = 0;

  m_positionsObject.generate();
  m_indicesObject.generate();

  m_positionsObject
    .allocate(sk_numQuadVerts * sk_numQuadPosComps * sizeof(float), sk_clipPosBuffer.data());
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
  : values(MAX_NUM_ISOSURFACES, 0.0f)
  , opacities(MAX_NUM_ISOSURFACES, 0.0f)
  , edgeStrengths(MAX_NUM_ISOSURFACES, 0.0f)
  , colors(MAX_NUM_ISOSURFACES, glm::vec3{0.0f})
  , ambientLights(MAX_NUM_ISOSURFACES, glm::vec3{0.0f})
  , diffuseLights(MAX_NUM_ISOSURFACES, glm::vec3{0.0f})
  , specularLights(MAX_NUM_ISOSURFACES, glm::vec3{0.0f})
  , shininesses(MAX_NUM_ISOSURFACES, 0.0f)
  , widthIn2d(0.0f)
{
}
