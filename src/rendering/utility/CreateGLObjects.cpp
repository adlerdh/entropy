#include "rendering/utility/CreateGLObjects.h"
#include "rendering/utility/vtk/PolyDataConversion.h"
#include "rendering/utility/vtk/PolyDataGenerator.h"

#include "common/Exception.hpp"
#include "common/UuidUtility.h"

#include "logic_old/annotation/PlanarPolygon.h"

#include <glm/glm.hpp>
#include <glm/gtc/packing.hpp>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/component_wise.hpp>
#include <glm/gtx/string_cast.hpp>

#include <spdlog/fmt/ostr.h>
#include <spdlog/spdlog.h>

#include <array>

namespace
{

std::shared_ptr<GLTexture> createTexture2d(const glm::i64vec2& size, const void* data)
{
  auto texture = std::make_shared<GLTexture>(tex::Target::Texture2D);
  texture->generate();

  texture->setSize(glm::uvec3{size.x, size.y, 1});

  texture->setData(
    0,
    tex::SizedInternalFormat::RGBA8_UNorm,
    tex::BufferPixelFormat::BGRA,
    tex::BufferPixelDataType::UInt8,
    data
  );

  //    static const glm::vec4 sk_transparentBlack{ 0.0f, 0.0f, 0.0f, 0.0f };
  //    texture->setBorderColor( sk_transparentBlack );
  //    texture->setWrapMode( tex::WrapMode::ClampToBorder );

  // Clamp to edge, since clamping to black border will change the color of the slide edges
  texture->setWrapMode(tex::WrapMode::ClampToEdge);

  texture->setAutoGenerateMipmaps(true);
  texture->setMinificationFilter(tex::MinificationFilter::Linear);
  texture->setMagnificationFilter(tex::MagnificationFilter::Linear);

  return texture;
}

/// @todo Remove this, as it is redundant with createMeshGpuRecordFromVtkPolyData
std::unique_ptr<MeshGpuRecord> convertPolyDataToMeshGpuRecord(vtkSmartPointer<vtkPolyData> polyData)
{
  const auto positionsArrayBuffer = vtkconvert::extractPointsToFloatArrayBuffer(polyData);
  const auto normalsArrayBuffer = vtkconvert::extractNormalsToUIntArrayBuffer(polyData);
  const auto indicesArrayBuffer = vtkconvert::extractIndicesToUIntArrayBuffer(polyData);

  if (!positionsArrayBuffer->buffer() || !normalsArrayBuffer->buffer() || !indicesArrayBuffer->buffer())
  {
    spdlog::error("Null mesh buffer data for Crosshair");
    return nullptr;
  }

  if (positionsArrayBuffer->vectorCount() != normalsArrayBuffer->vectorCount())
  {
    spdlog::error("Point and normal vector arrays for crosshair have different lengths");
    return nullptr;
  }

  VertexAttributeInfo positionsInfo(
    BufferComponentType::Float,
    BufferNormalizeValues::False,
    3,
    3 * sizeof(float),
    0,
    positionsArrayBuffer->vectorCount()
  );

  VertexAttributeInfo normalsInfo(
    BufferComponentType::Int_2_10_10_10,
    BufferNormalizeValues::True,
    4,
    sizeof(uint32_t),
    0,
    positionsArrayBuffer->vectorCount()
  );

  VertexIndicesInfo
    indexInfo(IndexType::UInt32, PrimitiveMode::Triangles, indicesArrayBuffer->length(), 0);

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

  auto gpuRecord = std::make_unique<
    MeshGpuRecord>(std::move(positionsObject), std::move(indicesObject), positionsInfo, indexInfo);

  gpuRecord->setNormals(std::move(normalsObject), normalsInfo);

  return gpuRecord;
}

} // namespace

namespace gpuhelper
{

std::unique_ptr<ImageGpuRecord> createImageGpuRecord(
  const Image* imageCpuRecord,
  const uint32_t componentIndex,
  const tex::MinificationFilter& minFilter,
  const tex::MagnificationFilter& magFilter,
  bool useNormalizedIntegers
)
{
  static constexpr GLint sk_alignment = 1;
  static const tex::WrapMode sk_wrapMode = tex::WrapMode::ClampToEdge;
  //    static const glm::vec4 sk_borderColor( 0.0f, 0.0f, 0.0f, 0.0f );

  if (!imageCpuRecord)
  {
    spdlog::error("Null imageCpuRecord");
    return nullptr;
  }

  const ImageHeader& header = imageCpuRecord->header();
  const ComponentType componentType = header.memoryComponentType();

  GLTexture::PixelStoreSettings pixelPackSettings;
  pixelPackSettings.m_alignment = sk_alignment;

  GLTexture::PixelStoreSettings pixelUnpackSettings = pixelPackSettings;

  auto texture = std::make_shared<GLTexture>(
    tex::Target::Texture3D, GLTexture::MultisampleSettings(), pixelPackSettings, pixelUnpackSettings
  );

  texture->generate();

  texture->setMinificationFilter(minFilter);
  texture->setMagnificationFilter(magFilter);

  texture->setWrapMode(sk_wrapMode);

  static const glm::u32vec3 sk_maxDims(std::numeric_limits<uint32_t>::max());

  if (glm::any(glm::greaterThan(header.pixelDimensions(), sk_maxDims)))
  {
    spdlog::error(
      "Error: Cannot create 3D texture: The pixel dimensions of image {} exceed the uint32_t limit",
      header.fileName()
    );
    return nullptr;
  }

  texture->setSize(glm::u32vec3{header.pixelDimensions()});
  texture->setAutoGenerateMipmaps(true);

  // Load data in for the first mipmap level of the texture
  static constexpr GLint sk_mipmapLevel = 0;

  if (useNormalizedIntegers)
  {
    texture->setData(
      sk_mipmapLevel,
      GLTexture::getSizedInternalNormalizedRedFormat(componentType),
      GLTexture::getBufferPixelNormalizedRedFormat(componentType),
      GLTexture::getBufferPixelDataType(componentType),
      imageCpuRecord->bufferAsVoid(componentIndex)
    );
  }
  else
  {
    texture->setData(
      sk_mipmapLevel,
      GLTexture::getSizedInternalRedFormat(componentType),
      GLTexture::getBufferPixelRedFormat(componentType),
      GLTexture::getBufferPixelDataType(componentType),
      imageCpuRecord->bufferAsVoid(componentIndex)
    );
  }

  return std::make_unique<ImageGpuRecord>(texture);
}

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
    sk_numVerts
  );

  VertexAttributeInfo normalsInfo(
    BufferComponentType::Int_2_10_10_10,
    BufferNormalizeValues::True,
    4,
    sizeof(NormalType),
    sk_offset,
    sk_numVerts
  );

  VertexAttributeInfo texCoordsInfo(
    BufferComponentType::Float,
    BufferNormalizeValues::False,
    2,
    sizeof(TexCoord2DType),
    sk_offset,
    sk_numVerts
  );

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
    std::move(indexInfo)
  );
}

std::unique_ptr<MeshGpuRecord> createSphereMeshGpuRecord()
{
  vtkSmartPointer<vtkPolyData> polyData = vtkutils::generateSphere();
  if (!polyData)
  {
    spdlog::error("Null mesh polygon data for sphere");
    return nullptr;
  }

  return convertPolyDataToMeshGpuRecord(polyData);
}

std::unique_ptr<MeshGpuRecord> createCylinderMeshGpuRecord(
  const glm::dvec3& center, double radius, double height
)
{
  vtkSmartPointer<vtkPolyData> polyData = vtkutils::generateCylinder(center, radius, height);

  if (!polyData)
  {
    spdlog::error("Null mesh polygon data for cylinder");
    return nullptr;
  }

  return convertPolyDataToMeshGpuRecord(polyData);
}

std::unique_ptr<MeshGpuRecord> createCrosshairMeshGpuRecord(double coneToCylinderRatio)
{
  if (coneToCylinderRatio < 0.0)
  {
    spdlog::error("Invalid cone-to-cylinder ratio of {} for crosshairs", coneToCylinderRatio);
    return nullptr;
  }

  vtkSmartPointer<vtkPolyData> polyData = vtkutils::generatePointyCylinders(coneToCylinderRatio);

  if (!polyData)
  {
    spdlog::error("Null mesh polygon data for Crosshair");
    return nullptr;
  }

  return convertPolyDataToMeshGpuRecord(polyData);
}

std::unique_ptr<MeshGpuRecord> createMeshGpuRecord(
  size_t vertexCount,
  size_t indexCount,
  const PrimitiveMode& primitiveMode,
  const BufferUsagePattern& bufferUsagePattern
)
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
    vertexCount
  );

  VertexAttributeInfo normalsInfo(
    BufferComponentType::Int_2_10_10_10,
    BufferNormalizeValues::True,
    4,
    sizeof(NormalType),
    sk_offset,
    vertexCount
  );

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
    std::move(indexInfo)
  );

  meshGpuRecord->setNormals(std::move(normalsObject), std::move(normalsInfo));

  return meshGpuRecord;
}

std::unique_ptr<MeshGpuRecord> createMeshGpuRecordFromVtkPolyData(
  vtkSmartPointer<vtkPolyData> polyData,
  const MeshPrimitiveType& primitiveType,
  const BufferUsagePattern& bufferUsagePattern
)
{
  if (!polyData)
  {
    spdlog::error("Null mesh polygon data");
    return nullptr;
  }

  PrimitiveMode primitiveMode = PrimitiveMode::Triangles;

  switch (primitiveType)
  {
  case MeshPrimitiveType::Triangles:
  {
    primitiveMode = PrimitiveMode::Triangles;
    break;
  }
  case MeshPrimitiveType::TriangleFan:
  {
    primitiveMode = PrimitiveMode::TriangleFan;
    break;
  }
  case MeshPrimitiveType::TriangleStrip:
  {
    primitiveMode = PrimitiveMode::TriangleStrip;
    break;
  }
  }

  /// @todo It is possible to extract different formats of ArrayBuffer from the vtkPolyData.
  /// Pass in arguments describing the formats to use.
  const auto positionsArrayBuffer = vtkconvert::extractPointsToFloatArrayBuffer(polyData);
  const auto normalsArrayBuffer = vtkconvert::extractNormalsToUIntArrayBuffer(polyData);
  const auto texCoordsArrayBuffer = vtkconvert::extractTexCoordsToFloatArrayBuffer(polyData);
  const auto indicesArrayBuffer = vtkconvert::extractIndicesToUIntArrayBuffer(polyData);

  if ( ! positionsArrayBuffer || ! positionsArrayBuffer->buffer() ||
         ! normalsArrayBuffer || ! normalsArrayBuffer->buffer() ||
         ! indicesArrayBuffer || ! indicesArrayBuffer->buffer() )
  {
    spdlog::error("Null array data extracted from PolyData");
    return nullptr;
  }

  if (positionsArrayBuffer->vectorCount() != normalsArrayBuffer->vectorCount())
  {
    spdlog::error("Vector arrays of normals extracted from PolyData has incorrect length");
    return nullptr;
  }

  spdlog::info(
    "Creating GPU record for mesh with {} vertices and {} indices",
    positionsArrayBuffer->vectorCount(),
    indicesArrayBuffer->vectorCount()
  );

  VertexAttributeInfo positionsInfo(
    BufferComponentType::Float,
    BufferNormalizeValues::False,
    3,
    3 * sizeof(float),
    0,
    positionsArrayBuffer->vectorCount()
  );

  VertexAttributeInfo normalsInfo(
    BufferComponentType::Int_2_10_10_10,
    BufferNormalizeValues::True,
    4,
    sizeof(uint32_t),
    0,
    normalsArrayBuffer->vectorCount()
  );

  VertexIndicesInfo indexInfo(IndexType::UInt32, primitiveMode, indicesArrayBuffer->length(), 0);

  GLBufferObject positionsObject(BufferType::VertexArray, bufferUsagePattern);
  GLBufferObject normalsObject(BufferType::VertexArray, bufferUsagePattern);
  GLBufferObject indicesObject(BufferType::Index, BufferUsagePattern::StaticDraw);

  positionsObject.generate();
  normalsObject.generate();
  indicesObject.generate();

  positionsObject.allocate(positionsArrayBuffer->byteCount(), positionsArrayBuffer->buffer());
  normalsObject.allocate(normalsArrayBuffer->byteCount(), normalsArrayBuffer->buffer());
  indicesObject.allocate(indicesArrayBuffer->byteCount(), indicesArrayBuffer->buffer());

  auto gpuRecord = std::make_unique<MeshGpuRecord>(
    std::move(positionsObject),
    std::move(indicesObject),
    std::move(positionsInfo),
    std::move(indexInfo)
  );

  gpuRecord->setNormals(std::move(normalsObject), std::move(normalsInfo));

  if (texCoordsArrayBuffer && texCoordsArrayBuffer->buffer())
  {
    if (positionsArrayBuffer->vectorCount() != texCoordsArrayBuffer->vectorCount())
    {
      spdlog::error(
        "Vector array of texture coordinates extracted from "
        "PolyData has incorrect length {} (vs. {} required)",
        positionsArrayBuffer->vectorCount(),
        texCoordsArrayBuffer->vectorCount()
      );
      return nullptr;
    }

    VertexAttributeInfo texCoordsInfo(
      BufferComponentType::Float,
      BufferNormalizeValues::False,
      2,
      sizeof(float),
      0,
      texCoordsArrayBuffer->vectorCount()
    );

    GLBufferObject texCoordsObject(BufferType::VertexArray, bufferUsagePattern);

    texCoordsObject.generate();
    texCoordsObject.allocate(texCoordsArrayBuffer->byteCount(), texCoordsArrayBuffer->buffer());

    gpuRecord->setTexCoords(std::move(texCoordsObject), std::move(texCoordsInfo));
  }

  return gpuRecord;
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
     p010, p010, p011, p011, p100, p100, p110, p110, p101, p101, p111, p111}
  };

  static const std::array<NormalType, sk_numPoints> sk_normalsArray = {
    {nx0, nx0, nx0, nx0, nx1, nx1, nx1, nx1, ny0, nz0, ny0, nz1,
     ny1, nz0, ny1, nz1, ny0, nz0, ny1, nz0, ny0, nz1, ny1, nz1}
  };

  static const std::array<TexCoordType, sk_numPoints> sk_texCoordsArray = {
    {t00, t00, t01, t01, t10, t11, t10, t11, t00, t00, t00, t00,
     t01, t01, t01, t01, t10, t10, t11, t11, t10, t10, t11, t11}
  };

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
     {23, 15, 21}}
  };

  VertexAttributeInfo positionsInfo(
    BufferComponentType::Float, BufferNormalizeValues::False, 3, sizeof(PositionType), 0, sk_numPoints
  );

  VertexAttributeInfo normalsInfo(
    BufferComponentType::Int_2_10_10_10,
    BufferNormalizeValues::True,
    4,
    sizeof(NormalType),
    0,
    sk_numPoints
  );

  VertexAttributeInfo texCoordsInfo(
    BufferComponentType::Float, BufferNormalizeValues::False, 2, sizeof(TexCoordType), 0, sk_numPoints
  );

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
    std::move(indexInfo)
  );
}

void createTestColorBuffer(MeshRecord& meshRecord)
{
  MeshCpuRecord* cpuRecord = meshRecord.cpuData();
  MeshGpuRecord* gpuRecord = meshRecord.gpuData();

  if (!cpuRecord || !gpuRecord)
  {
    spdlog::error("Null record");
    return;
  }

  vtkSmartPointer<vtkPolyData> polyData = meshRecord.cpuData()->polyData();

  if (!polyData)
  {
    spdlog::error("Null vtkPolyData in MeshCPURecord");
    return;
  }

  const auto& positionsInfo = gpuRecord->positionsInfo();
  const auto vertexCount = positionsInfo.vertexCount();

  VertexAttributeInfo colorsInfo(
    BufferComponentType::UByte, BufferNormalizeValues::True, 4, 4 * sizeof(uint8_t), 0, vertexCount
  );

  GLBufferObject colorsBuffer(BufferType::VertexArray, BufferUsagePattern::StaticDraw);
  colorsBuffer.generate();

  const auto normalsArrayBuffer = vtkconvert::extractNormalsToUIntArrayBuffer(polyData);

  if (!normalsArrayBuffer)
  {
    spdlog::error("Null normalsArrayBuffer");
    return;
  }

  const uint32_t* normalsBuffer = static_cast<const uint32_t*>(normalsArrayBuffer->buffer());

  const size_t byteCount = 4 * vertexCount;
  auto colorBuffer = std::make_unique<uint8_t[]>(byteCount);

  for (size_t i = 0; i < vertexCount; ++i)
  {
    const glm::vec3 normal = glm::abs(glm::vec3{glm::unpackSnorm3x10_1x2(normalsBuffer[i])});
    const float max = glm::compMax(normal);
    const glm::u8vec3 N = glm::u8vec3{(255.0f / max) * normal};

    colorBuffer[4 * i + 0] = N.x;
    colorBuffer[4 * i + 1] = N.y;
    colorBuffer[4 * i + 2] = N.z;
    colorBuffer[4 * i + 3] = 255;
  }

  colorsBuffer.allocate(byteCount, colorBuffer.get());

  gpuRecord->setColors(std::move(colorsBuffer), std::move(colorsInfo));
}

std::unique_ptr<SlideGpuRecord> createSlideGpuRecord(const slideio::SlideCpuRecord* cpuRecord)
{
  if (!cpuRecord)
  {
    spdlog::error("Null SlideCpuRecord");
    return nullptr;
  }

  // Create the GPU texture from the smallest among all levels

  const slideio::SlideLevel* smallestLevel = nullptr;

  const size_t numCreatedLevels = cpuRecord->numCreatedLevels();
  const size_t numFileLevels = cpuRecord->numFileLevels();

  // Check created levels first
  if (numCreatedLevels > 0)
  {
    smallestLevel = &(cpuRecord->createdLevel(numCreatedLevels - 1));
  }
  else if (numFileLevels > 0)
  {
    smallestLevel = &(cpuRecord->fileLevel(numFileLevels - 1));
  }
  else
  {
    spdlog::error("No level data");
    return nullptr;
  }

  if ( ! smallestLevel || ! smallestLevel->m_data ||
         smallestLevel->m_dims.x <= 0 || smallestLevel->m_dims.y <= 0 )
  {
    spdlog::error("Null level data");
    return nullptr;
  }

  auto texture = createTexture2d(
    smallestLevel->m_dims, const_cast<const uint32_t*>(smallestLevel->m_data.get())
  );

  return std::make_unique<SlideGpuRecord>(texture);
}

std::unique_ptr<SlideAnnotationGpuRecord> createSlideAnnotationGpuRecord(const PlanarPolygon& polygon
)
{
  static const uint32_t sk_upNormal = glm::packSnorm3x10_1x2(glm::vec4{0.0f, 0.0f, 1.0f, 0.0f});
  static const uint32_t sk_downNormal = glm::packSnorm3x10_1x2(glm::vec4{0.0f, 0.0f, -1.0f, 0.0f});

  // The first polygon is the outer boundary; subsequent polygons define holes inside of it.

  if (polygon.numBoundaries() < 1)
  {
    spdlog::error("Error: Annotation must contain at least an outer boundary.");
    return nullptr;
  }

  std::vector<glm::vec3> vertices;

  // Add vertices for the bottom face (z = 0) of the mesh:
  for (const auto& boundary : polygon.getAllVertices())
  {
    if (boundary.size() < 3)
    {
      spdlog::error("Error: Polygon must have at least 3 vertices");
      return nullptr;
    }

    for (const PlanarPolygon::PointType& v : boundary)
    {
      vertices.emplace_back(glm::vec3{v.x, v.y, 0.0f});
    }
  }

  // Number of bottom/top face vertices is equal, since vertices are duplicated for bottom/top:
  const uint32_t N = static_cast<uint32_t>(vertices.size());

  // Normals for bottom face:
  std::vector<uint32_t> normals(N, sk_downNormal);

  // Duplicate the vertices for the top face (z = 1) of the mesh:
  for (size_t i = 0; i < N; ++i)
  {
    vertices.emplace_back(glm::vec3{vertices[i].x, vertices[i].y, 1.0f});
  }

  // Normals for top face:
  normals.insert(std::end(normals), N, sk_upNormal);

  // Add indices for bottom face, flipping orientation from clockwise to counter-clockwise:
  std::vector<PlanarPolygon::IndexType> indices;

  for (size_t i = 0; i < polygon.numTriangles(); ++i)
  {
    const auto triangle = polygon.getTriangle(i);
    indices.emplace_back(std::get<2>(triangle));
    indices.emplace_back(std::get<1>(triangle));
    indices.emplace_back(std::get<0>(triangle));
  }

  // Duplicate the indices for the top face, preserving the clockwise orientation,
  // which is correct for the top face:
  for (size_t i = 0; i < polygon.numTriangles(); ++i)
  {
    const auto triangle = polygon.getTriangle(i);
    indices.emplace_back(std::get<0>(triangle) + N);
    indices.emplace_back(std::get<1>(triangle) + N);
    indices.emplace_back(std::get<2>(triangle) + N);
  }

  // Create side faces:

  uint32_t offset = 0;

  // Total number of vertices added thus far:
  uint32_t vCount = 2 * N;

  // Flag for whether sides are being added for the outside boundary (true)
  // or for the holes on the inside (false):
  bool outside = true;

  for (const auto& boundary : polygon.getAllVertices())
  {
    for (uint32_t i = 0; i < boundary.size(); ++i)
    {
      const uint32_t aBot = offset + i;
      const uint32_t aTop = offset + i + N;
      const uint32_t bBot = offset + (i + 1) % boundary.size();
      const uint32_t bTop = offset + (i + 1) % boundary.size() + N;

      const glm::vec3 aBotV = vertices[aBot];
      const glm::vec3 aTopV = vertices[aTop];
      const glm::vec3 bBotV = vertices[bBot];
      const glm::vec3 bTopV = vertices[bTop];

      // Add new vertices with new face normal:
      vertices.push_back(aBotV); // M + 0
      vertices.push_back(aTopV); // M + 1
      vertices.push_back(bBotV); // M + 2
      vertices.push_back(bTopV); // M + 3

      // Add one normal per vertex:
      const glm::vec3 faceNormal = glm::normalize(glm::cross(bBotV - aBotV, aTopV - aBotV));
      const uint32_t packedNormal = glm::packSnorm3x10_1x2(glm::vec4{faceNormal, 0.0f});
      normals.insert(std::end(normals), 4, packedNormal);

      // Flip face orientations based on whether the side belongs to the outside boundary
      // or to the interior holes:
      if (outside)
      {
        indices.emplace_back(vCount + 0); // aBot
        indices.emplace_back(vCount + 2); // bBot
        indices.emplace_back(vCount + 3); // bTop

        indices.emplace_back(vCount + 3); // bTop
        indices.emplace_back(vCount + 1); // aTop
        indices.emplace_back(vCount + 0); // aBot
      }
      else
      {
        indices.emplace_back(vCount + 3); // bTop
        indices.emplace_back(vCount + 2); // bBot
        indices.emplace_back(vCount + 0); // aBot

        indices.emplace_back(vCount + 0); // aBot
        indices.emplace_back(vCount + 1); // aTop
        indices.emplace_back(vCount + 3); // bTop
      }

      // Increment total vertex count:
      vCount += 4;
    }

    offset += boundary.size();

    outside = false;
  }

  VertexAttributeInfo positionsInfo(
    BufferComponentType::Float, BufferNormalizeValues::False, 3, sizeof(glm::vec3), 0, vertices.size()
  );

  VertexAttributeInfo normalsInfo(
    BufferComponentType::Int_2_10_10_10,
    BufferNormalizeValues::True,
    4,
    sizeof(uint32_t),
    0,
    normals.size()
  );

  VertexIndicesInfo indexInfo(IndexType::UInt32, PrimitiveMode::Triangles, indices.size(), 0);

  GLBufferObject positionsObject(BufferType::VertexArray, BufferUsagePattern::StaticDraw);
  GLBufferObject normalsObject(BufferType::VertexArray, BufferUsagePattern::StaticDraw);
  GLBufferObject indicesObject(BufferType::Index, BufferUsagePattern::StaticDraw);

  positionsObject.generate();
  normalsObject.generate();
  indicesObject.generate();

  positionsObject.allocate(sizeof(glm::vec3) * vertices.size(), vertices.data());
  normalsObject.allocate(sizeof(uint32_t) * normals.size(), normals.data());
  indicesObject.allocate(sizeof(uint32_t) * indices.size(), indices.data());

  auto gpuRecord = std::make_shared<
    MeshGpuRecord>(std::move(positionsObject), std::move(indicesObject), positionsInfo, indexInfo);

  gpuRecord->setNormals(std::move(normalsObject), normalsInfo);

  return std::make_unique<SlideAnnotationGpuRecord>(gpuRecord);
}

std::unique_ptr<GLTexture> createImageColorMapTexture(const ImageColorMap* colorMap)
{
  if (!colorMap)
  {
    return nullptr;
  }

  auto texture = std::make_unique<GLTexture>(tex::Target::Texture1D);

  texture->generate();
  texture->setSize(glm::uvec3{colorMap->numColors(), 1, 1});

  texture->setData(
    0, // level 0
    ImageColorMap::textureFormat_RGBA_F32(),
    tex::BufferPixelFormat::RGBA,
    tex::BufferPixelDataType::Float32,
    colorMap->data_RGBA_F32()
  );

  // We should never sample outside the texture coordinate range [0.0, 1.0], anyway
  texture->setWrapMode(tex::WrapMode::ClampToEdge);

  // All sampling of color maps uses linearly interpolation
  texture->setAutoGenerateMipmaps(false);
  texture->setMinificationFilter(tex::MinificationFilter::Linear);
  texture->setMagnificationFilter(tex::MagnificationFilter::Linear);

  return texture;
}

std::unique_ptr<GLBufferTexture> createLabelColorTableTextureBuffer(
  const ParcellationLabelTable* labels
)
{
  if (!labels)
  {
    return nullptr;
  }

  // Buffer contents will be modified once and used many times
  auto colorMapTexture = std::make_unique<
    GLBufferTexture>(labels->bufferTextureFormat_RGBA_U8(), BufferUsagePattern::StaticDraw);

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

  if (tex::Target::TextureCubeMap == target || tex::Target::TextureBuffer == target)
  {
    throw_debug("Invalid texture target type ");
  }

  GLTexture::PixelStoreSettings pixelPackSettings;
  pixelPackSettings.m_alignment = sk_alignment;

  GLTexture::PixelStoreSettings pixelUnpackSettings = pixelPackSettings;

  GLTexture texture(target, GLTexture::MultisampleSettings(), pixelPackSettings, pixelUnpackSettings);

  texture.generate();
  texture.setSize(glm::uvec3{1, 1, 1});

  GLvoid* data = nullptr;

  switch (componentType)
  {
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
    throw_debug("Int64 texture not supported");
  case ComponentType::ULong:
    throw_debug("UInt64 texture not supported");
  case ComponentType::Float64:
    throw_debug("Float64 texture not supported");
  case ComponentType::LongLong:
    throw_debug("LongLong texture not supported");
  case ComponentType::ULongLong:
    throw_debug("ULongLong texture not supported");
  case ComponentType::LongDouble:
    throw_debug("LongDouble texture not supported");
  default:
  case ComponentType::Undefined:
    throw_debug("Undefined texture not supported");
  }

  texture.setData(
    0,
    GLTexture::getSizedInternalRGBAFormat(componentType),
    GLTexture::getBufferPixelRGBAFormat(componentType),
    GLTexture::getBufferPixelDataType(componentType),
    data
  );

  texture.setWrapMode(tex::WrapMode::ClampToEdge);

  texture.setAutoGenerateMipmaps(false);
  texture.setMinificationFilter(tex::MinificationFilter::Nearest);
  texture.setMagnificationFilter(tex::MagnificationFilter::Nearest);

  return texture;
}

} // namespace gpuhelper
