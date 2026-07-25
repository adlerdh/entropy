#include "rendering/utility/CreateGLObjects.h"
#include "rendering/utility/vtk/PolyDataConversion.h"
#include "rendering/utility/vtk/PolyDataGenerator.h"

#include <glm/glm.hpp>
#include <glm/gtc/packing.hpp>

#include <spdlog/spdlog.h>

#include <array>

namespace
{

std::unique_ptr<MeshGpuRecord> convertPolyDataToMeshGpuRecord(vtkSmartPointer<vtkPolyData> polyData)
{
  const auto positionsArrayBuffer = vtkconvert::extractPointsToFloatArrayBuffer(polyData);
  const auto normalsArrayBuffer = vtkconvert::extractNormalsToUIntArrayBuffer(polyData);
  const auto indicesArrayBuffer = vtkconvert::extractIndicesToUIntArrayBuffer(polyData);

  if (!positionsArrayBuffer->buffer() || !normalsArrayBuffer->buffer() || !indicesArrayBuffer->buffer()) {
    spdlog::error("Null mesh buffer data for Crosshair");
    return nullptr;
  }

  if (positionsArrayBuffer->vectorCount() != normalsArrayBuffer->vectorCount()) {
    spdlog::error("Point and normal vector arrays for crosshair have different lengths");
    return nullptr;
  }

  VertexAttributeInfo positionsInfo(
    BufferComponentType::Float,
    BufferNormalizeValues::False,
    3,
    3 * sizeof(float),
    0,
    positionsArrayBuffer->vectorCount());

  VertexAttributeInfo normalsInfo(
    BufferComponentType::Int_2_10_10_10,
    BufferNormalizeValues::True,
    4,
    sizeof(uint32_t),
    0,
    positionsArrayBuffer->vectorCount());

  VertexIndicesInfo indexInfo(IndexType::UInt32, PrimitiveMode::Triangles, indicesArrayBuffer->length(), 0);

  GLBufferObject positionsObject(BufferType::VertexArray, BufferUsagePattern::StaticDraw);
  GLBufferObject normalsObject(BufferType::VertexArray, BufferUsagePattern::StaticDraw);
  GLBufferObject indicesObject(BufferType::Index, BufferUsagePattern::StaticDraw);

  positionsObject.generate();
  normalsObject.generate();
  indicesObject.generate();

  positionsObject.allocate(positionsArrayBuffer->byteCount(), positionsArrayBuffer->buffer());
  normalsObject.allocate(normalsArrayBuffer->byteCount(), normalsArrayBuffer->buffer());
  indicesObject.allocate(indicesArrayBuffer->byteCount(), indicesArrayBuffer->buffer());

  // Note: We are not storing the polyData in the CPU record,
  // since it is never needed again and just takes up space.

  auto gpuRecord =
    std::make_unique<MeshGpuRecord>(std::move(positionsObject), std::move(indicesObject), positionsInfo, indexInfo);

  gpuRecord->setNormals(std::move(normalsObject), normalsInfo);

  return gpuRecord;
}

} // namespace

namespace gpuhelper
{

std::unique_ptr<MeshGpuRecord> createSliceMeshGpuRecord(const BufferUsagePattern& bufferUsagePattern)
{
  static constexpr int sk_numCoords = 3;
  static constexpr int sk_numVerts = 7;
  static constexpr int sk_numIndices = 8;

  // Indices for a triangle fan defining a hexagon:
  // the first vertex is the central hub;
  // the second vertex is repeated to close the hexagon.
  static const std::array<uint32_t, sk_numIndices> sk_sliceIndices = {{6, 0, 1, 2, 3, 4, 5, 0}};

  static constexpr int sk_offset = 0;

  using PositionType = glm::vec3;
  using NormalType = uint32_t;
  using TexCoord2DType = glm::vec2;
  using VertexIndexType = uint32_t;

  VertexAttributeInfo positionsInfo(
    BufferComponentType::Float,
    BufferNormalizeValues::False,
    sk_numCoords,
    sizeof(PositionType),
    sk_offset,
    sk_numVerts);

  VertexAttributeInfo normalsInfo(
    BufferComponentType::Int_2_10_10_10,
    BufferNormalizeValues::True,
    4,
    sizeof(NormalType),
    sk_offset,
    sk_numVerts);

  VertexAttributeInfo texCoordsInfo(
    BufferComponentType::Float,
    BufferNormalizeValues::False,
    2,
    sizeof(TexCoord2DType),
    sk_offset,
    sk_numVerts);

  VertexIndicesInfo indexInfo(IndexType::UInt32, PrimitiveMode::TriangleFan, sk_numIndices, 0);

  GLBufferObject positionsObject(BufferType::VertexArray, bufferUsagePattern);
  GLBufferObject normalsObject(BufferType::VertexArray, bufferUsagePattern);
  GLBufferObject texCoordsObject(BufferType::VertexArray, bufferUsagePattern);
  GLBufferObject indicesObject(BufferType::Index, BufferUsagePattern::StaticDraw);

  positionsObject.generate();
  normalsObject.generate();
  texCoordsObject.generate();
  indicesObject.generate();

  positionsObject.allocate(sk_numVerts * sizeof(PositionType), nullptr);
  normalsObject.allocate(sk_numVerts * sizeof(NormalType), nullptr);
  texCoordsObject.allocate(sk_numVerts * sizeof(TexCoord2DType), nullptr);
  indicesObject.allocate(sk_numIndices * sizeof(VertexIndexType), sk_sliceIndices.data());

  return std::make_unique<MeshGpuRecord>(
    std::move(positionsObject),
    std::move(normalsObject),
    std::move(texCoordsObject),
    std::move(indicesObject),
    std::move(positionsInfo),
    std::move(normalsInfo),
    std::move(texCoordsInfo),
    std::move(indexInfo));
}

std::unique_ptr<MeshGpuRecord> createSphereMeshGpuRecord()
{
  vtkSmartPointer<vtkPolyData> polyData = vtkutils::generateSphere();
  if (!polyData) {
    spdlog::error("Null mesh polygon data for sphere");
    return nullptr;
  }

  return convertPolyDataToMeshGpuRecord(polyData);
}

std::unique_ptr<MeshGpuRecord> createCylinderMeshGpuRecord(const glm::dvec3& center, double radius, double height)
{
  vtkSmartPointer<vtkPolyData> polyData = vtkutils::generateCylinder(center, radius, height);

  if (!polyData) {
    spdlog::error("Null mesh polygon data for cylinder");
    return nullptr;
  }

  return convertPolyDataToMeshGpuRecord(polyData);
}

std::unique_ptr<MeshGpuRecord> createCrosshairMeshGpuRecord(double coneToCylinderRatio)
{
  if (coneToCylinderRatio < 0.0) {
    spdlog::error("Invalid cone-to-cylinder ratio of {} for crosshairs", coneToCylinderRatio);
    return nullptr;
  }

  vtkSmartPointer<vtkPolyData> polyData = vtkutils::generatePointyCylinders(coneToCylinderRatio);

  if (!polyData) {
    spdlog::error("Null mesh polygon data for Crosshair");
    return nullptr;
  }

  return convertPolyDataToMeshGpuRecord(polyData);
}

std::unique_ptr<MeshGpuRecord> createMeshGpuRecord(
  size_t vertexCount,
  size_t indexCount,
  const PrimitiveMode& primitiveMode,
  const BufferUsagePattern& bufferUsagePattern)
{
  static constexpr int sk_numCoords = 3;

  static constexpr int sk_offset = 0;

  using PositionType = glm::vec3;
  using NormalType = uint32_t;
  using VertexIndexType = uint32_t;

  VertexAttributeInfo positionsInfo(
    BufferComponentType::Float,
    BufferNormalizeValues::False,
    sk_numCoords,
    sizeof(PositionType),
    sk_offset,
    vertexCount);

  VertexAttributeInfo normalsInfo(
    BufferComponentType::Int_2_10_10_10,
    BufferNormalizeValues::True,
    4,
    sizeof(NormalType),
    sk_offset,
    vertexCount);

  VertexIndicesInfo indexInfo(IndexType::UInt32, primitiveMode, indexCount, 0);

  GLBufferObject positionsObject(BufferType::VertexArray, bufferUsagePattern);
  GLBufferObject normalsObject(BufferType::VertexArray, bufferUsagePattern);
  GLBufferObject texCoordsObject(BufferType::VertexArray, bufferUsagePattern);
  GLBufferObject indicesObject(BufferType::Index, BufferUsagePattern::StaticDraw);

  positionsObject.generate();
  normalsObject.generate();
  texCoordsObject.generate();
  indicesObject.generate();

  positionsObject.allocate(vertexCount * sizeof(PositionType), nullptr);
  normalsObject.allocate(vertexCount * sizeof(NormalType), nullptr);
  indicesObject.allocate(indexCount * sizeof(VertexIndexType), nullptr);

  auto meshGpuRecord = std::make_unique<MeshGpuRecord>(
    std::move(positionsObject),
    std::move(indicesObject),
    std::move(positionsInfo),
    std::move(indexInfo));

  meshGpuRecord->setNormals(std::move(normalsObject), std::move(normalsInfo));

  return meshGpuRecord;
}

std::unique_ptr<MeshGpuRecord> createBoxMeshGpuRecord(const BufferUsagePattern& bufferUsagePattern)
{
  using PositionType = glm::vec3;
  using NormalType = uint32_t;
  using TexCoordType = glm::vec2;
  using IndexedTriangleType = glm::u8vec3;

  static constexpr int sk_numPoints = 24;
  static constexpr int sk_numTriangles = 12;

  static const PositionType p000{0.0, 0.0, 0.0};
  static const PositionType p001{0.0, 0.0, 1.0};
  static const PositionType p010{0.0, 1.0, 0.0};
  static const PositionType p011{0.0, 1.0, 1.0};
  static const PositionType p100{1.0, 0.0, 0.0};
  static const PositionType p101{1.0, 0.0, 1.0};
  static const PositionType p110{1.0, 1.0, 0.0};
  static const PositionType p111{1.0, 1.0, 1.0};

  static const NormalType nx0 = glm::packSnorm3x10_1x2({-1, 0, 0, 0});
  static const NormalType nx1 = glm::packSnorm3x10_1x2({1, 0, 0, 0});
  static const NormalType ny0 = glm::packSnorm3x10_1x2({0, -1, 0, 0});
  static const NormalType ny1 = glm::packSnorm3x10_1x2({0, 1, 0, 0});
  static const NormalType nz0 = glm::packSnorm3x10_1x2({0, 0, -1, 0});
  static const NormalType nz1 = glm::packSnorm3x10_1x2({0, 0, 1, 0});

  static const TexCoordType t00{0, 0};
  static const TexCoordType t01{0, 1};
  static const TexCoordType t10{1, 0};
  static const TexCoordType t11{1, 1};

  static const std::array<PositionType, sk_numPoints> sk_pointsArray = {
    {p000, p001, p010, p011, p100, p110, p101, p111, p000, p000, p001, p001,
     p010, p010, p011, p011, p100, p100, p110, p110, p101, p101, p111, p111}};

  static const std::array<NormalType, sk_numPoints> sk_normalsArray = {{nx0, nx0, nx0, nx0, nx1, nx1, nx1, nx1,
                                                                        ny0, nz0, ny0, nz1, ny1, nz0, ny1, nz1,
                                                                        ny0, nz0, ny1, nz0, ny0, nz1, ny1, nz1}};

  static const std::array<TexCoordType, sk_numPoints> sk_texCoordsArray = {{t00, t00, t01, t01, t10, t11, t10, t11,
                                                                            t00, t00, t00, t00, t01, t01, t01, t01,
                                                                            t10, t10, t11, t11, t10, t10, t11, t11}};

  static const std::array<IndexedTriangleType, sk_numTriangles> sk_indexArray = {
    {{0, 1, 2},
     {3, 2, 1},
     {4, 5, 6},
     {7, 6, 5},
     {8, 16, 10},
     {20, 10, 16},
     {12, 14, 18},
     {22, 18, 14},
     {9, 13, 17},
     {19, 17, 13},
     {11, 21, 15},
     {23, 15, 21}}};

  VertexAttributeInfo
    positionsInfo(BufferComponentType::Float, BufferNormalizeValues::False, 3, sizeof(PositionType), 0, sk_numPoints);

  VertexAttributeInfo normalsInfo(
    BufferComponentType::Int_2_10_10_10,
    BufferNormalizeValues::True,
    4,
    sizeof(NormalType),
    0,
    sk_numPoints);

  VertexAttributeInfo
    texCoordsInfo(BufferComponentType::Float, BufferNormalizeValues::False, 2, sizeof(TexCoordType), 0, sk_numPoints);

  VertexIndicesInfo indexInfo(IndexType::UInt8, PrimitiveMode::Triangles, 3 * sk_numTriangles, 0);

  GLBufferObject positionsObject(BufferType::VertexArray, bufferUsagePattern);
  GLBufferObject normalsObject(BufferType::VertexArray, bufferUsagePattern);
  GLBufferObject texCoordsObject(BufferType::VertexArray, bufferUsagePattern);
  GLBufferObject indicesObject(BufferType::Index, BufferUsagePattern::StaticDraw);

  positionsObject.generate();
  normalsObject.generate();
  texCoordsObject.generate();
  indicesObject.generate();

  positionsObject.allocate(sk_numPoints * sizeof(PositionType), sk_pointsArray.data());
  normalsObject.allocate(sk_numPoints * sizeof(NormalType), sk_normalsArray.data());
  texCoordsObject.allocate(sk_numPoints * sizeof(TexCoordType), sk_texCoordsArray.data());
  indicesObject.allocate(sk_numTriangles * sizeof(IndexedTriangleType), sk_indexArray.data());

  return std::make_unique<MeshGpuRecord>(
    std::move(positionsObject),
    std::move(normalsObject),
    std::move(texCoordsObject),
    std::move(indicesObject),
    std::move(positionsInfo),
    std::move(normalsInfo),
    std::move(texCoordsInfo),
    std::move(indexInfo));
}

std::unique_ptr<GLTexture> createImageColorMapTexture(const ImageColorMap* colorMap)
{
  if (!colorMap) {
    return nullptr;
  }

  auto texture = std::make_unique<GLTexture>(tex::Target::Texture1D);

  texture->generate();
  texture->setSize(glm::uvec3{colorMap->numColors(), 1, 1});

  texture->setData(
    0, // level 0
    tex::SizedInternalFormat::RGBA32F,
    tex::BufferPixelFormat::RGBA,
    tex::BufferPixelDataType::Float32,
    colorMap->data_RGBA_F32());

  // We should never sample outside the texture coordinate range [0.0, 1.0], anyway
  texture->setWrapMode(tex::WrapMode::ClampToEdge);

  // All sampling of color maps uses linearly interpolation
  texture->setAutoGenerateMipmaps(false);
  texture->setMinificationFilter(tex::MinificationFilter::Linear);
  texture->setMagnificationFilter(tex::MagnificationFilter::Linear);

  return texture;
}

std::unique_ptr<GLBufferTexture> createLabelColorTableTextureBuffer(const ParcellationLabelTable* labels)
{
  if (!labels) {
    return nullptr;
  }

  // Buffer contents will be modified once and used many times
  auto colorMapTexture =
    std::make_unique<GLBufferTexture>(labels->bufferTextureFormat_RGBA_U8(), BufferUsagePattern::StaticDraw);

  colorMapTexture->generate();
  colorMapTexture->allocate(labels->numColorBytes_RGBA_U8(), labels->colorData_RGBA_nonpremult_U8());
  colorMapTexture->attachBufferToTexture();

  return colorMapTexture;
}

GLTexture createBlankRGBATexture(const ComponentType& componentType, const tex::Target& target)
{
  static std::array<int8_t, 4> sk_data_I8 = {0, 0, 0, 0};
  static std::array<uint8_t, 4> sk_data_U8 = {0, 0, 0, 0};
  static std::array<int16_t, 4> sk_data_I16 = {0, 0, 0, 0};
  static std::array<uint16_t, 4> sk_data_U16 = {0, 0, 0, 0};
  static std::array<int32_t, 4> sk_data_I32 = {0, 0, 0, 0};
  static std::array<uint32_t, 4> sk_data_U32 = {0, 0, 0, 0};
  static std::array<float, 4> sk_data_F32 = {0.0f, 0.0f, 0.0f, 0.0f};

  static constexpr GLint sk_alignment = 1;

  if (tex::Target::TextureCubeMap == target || tex::Target::TextureBuffer == target) {
    throwDebug("Invalid texture target type ");
  }

  GLTexture::PixelStoreSettings pixelPackSettings;
  pixelPackSettings.m_alignment = sk_alignment;

  GLTexture::PixelStoreSettings pixelUnpackSettings = pixelPackSettings;

  GLTexture texture(target, GLTexture::MultisampleSettings(), pixelPackSettings, pixelUnpackSettings);

  texture.generate();
  texture.setSize(glm::uvec3{1, 1, 1});

  GLvoid* data = nullptr;

  switch (componentType) {
    case ComponentType::Int8:
      data = sk_data_I8.data();
      break;
    case ComponentType::UInt8:
      data = sk_data_U8.data();
      break;
    case ComponentType::Int16:
      data = sk_data_I16.data();
      break;
    case ComponentType::UInt16:
      data = sk_data_U16.data();
      break;
    case ComponentType::Int32:
      data = sk_data_I32.data();
      break;
    case ComponentType::UInt32:
      data = sk_data_U32.data();
      break;
    case ComponentType::Float32:
      data = sk_data_F32.data();
      break;
    case ComponentType::Long:
      throwDebug("Int64 texture not supported");
    case ComponentType::ULong:
      throwDebug("UInt64 texture not supported");
    case ComponentType::Float64:
      throwDebug("Float64 texture not supported");
    case ComponentType::LongLong:
      throwDebug("LongLong texture not supported");
    case ComponentType::ULongLong:
      throwDebug("ULongLong texture not supported");
    case ComponentType::LongDouble:
      throwDebug("LongDouble texture not supported");
    default:
    case ComponentType::Undefined:
      throwDebug("Undefined texture not supported");
  }

  texture.setData(
    0,
    GLTexture::getSizedInternalRGBAFormat(componentType),
    GLTexture::getBufferPixelRGBAFormat(componentType),
    GLTexture::getBufferPixelDataType(componentType),
    data);

  texture.setWrapMode(tex::WrapMode::ClampToEdge);

  texture.setAutoGenerateMipmaps(false);
  texture.setMinificationFilter(tex::MinificationFilter::Nearest);
  texture.setMagnificationFilter(tex::MagnificationFilter::Nearest);

  return texture;
}

} // namespace gpuhelper
