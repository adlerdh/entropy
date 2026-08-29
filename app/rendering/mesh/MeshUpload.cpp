#include "rendering/mesh/MeshUpload.h"

#include "rendering/mesh/MeshValidation.h"

#include <glad/glad.h>

#include <cstddef>
#include <cstdint>
#include <utility>

namespace rendering::mesh
{

namespace
{

constexpr GLuint sk_positionAttribute = 0;
constexpr GLuint sk_normalAttribute = 1;
constexpr GLuint sk_colorAttribute = 2;
constexpr GLuint sk_textureCoordAttribute = 3;
constexpr int sk_vec3Components = 3;
constexpr int sk_vec4Components = 4;
constexpr int sk_byteOffset = 0;

VertexAttributeInfo floatAttributeInfo(int numComponents, std::size_t stride, std::size_t vertexCount)
{
  return VertexAttributeInfo(
    BufferComponentType::Float,
    BufferNormalizeValues::False,
    numComponents,
    static_cast<int>(stride),
    sk_byteOffset,
    vertexCount);
}

} // namespace

std::optional<MeshGpuData>
uploadMeshData(const MeshData& mesh, const MeshHandle& handle, BufferUsagePattern usagePattern)
{
  if (!isValidMeshData(mesh)) {
    return std::nullopt;
  }

  const VertexAttributeInfo positionsInfo =
    floatAttributeInfo(sk_vec3Components, sizeof(glm::vec3), mesh.positions.size());
  const VertexIndicesInfo indicesInfo(IndexType::UInt32, PrimitiveMode::Triangles, mesh.indices.size(), 0);

  GLBufferObject positionsObject(BufferType::VertexArray, usagePattern);
  GLBufferObject indicesObject(BufferType::Index, usagePattern);
  positionsObject.generate();
  indicesObject.generate();
  positionsObject.allocate(mesh.positions.size() * sizeof(glm::vec3), mesh.positions.data());
  indicesObject.allocate(mesh.indices.size() * sizeof(uint32_t), mesh.indices.data());

  GLVertexArrayObject vao;
  vao.generate();
  vao.bind();
  {
    indicesObject.bind();
    positionsObject.bind();
    vao.setAttributeBuffer(sk_positionAttribute, positionsInfo);
    vao.enableVertexAttribute(sk_positionAttribute);
  }
  vao.release();

  MeshGpuData
    gpuData(handle, std::move(vao), std::move(positionsObject), std::move(indicesObject), positionsInfo, indicesInfo);

  if (!mesh.normals.empty()) {
    const VertexAttributeInfo normalsInfo =
      floatAttributeInfo(sk_vec3Components, sizeof(glm::vec3), mesh.normals.size());
    GLBufferObject normalsObject(BufferType::VertexArray, usagePattern);
    normalsObject.generate();
    normalsObject.allocate(mesh.normals.size() * sizeof(glm::vec3), mesh.normals.data());

    GLVertexArrayObject& gpuVao = gpuData.vao();
    gpuVao.bind();
    {
      normalsObject.bind();
      gpuVao.setAttributeBuffer(sk_normalAttribute, normalsInfo);
      gpuVao.enableVertexAttribute(sk_normalAttribute);
    }
    gpuVao.release();

    gpuData.setNormals(std::move(normalsObject), normalsInfo);
  }

  if (mesh.colors) {
    const VertexAttributeInfo colorsInfo =
      floatAttributeInfo(sk_vec4Components, sizeof(glm::vec4), mesh.colors->size());
    GLBufferObject colorsObject(BufferType::VertexArray, usagePattern);
    colorsObject.generate();
    colorsObject.allocate(mesh.colors->size() * sizeof(glm::vec4), mesh.colors->data());

    GLVertexArrayObject& gpuVao = gpuData.vao();
    gpuVao.bind();
    {
      colorsObject.bind();
      gpuVao.setAttributeBuffer(sk_colorAttribute, colorsInfo);
      gpuVao.enableVertexAttribute(sk_colorAttribute);
    }
    gpuVao.release();

    gpuData.setColors(std::move(colorsObject), colorsInfo);
  }

  if (mesh.textureCoords) {
    const VertexAttributeInfo textureCoordsInfo =
      floatAttributeInfo(sk_vec3Components, sizeof(glm::vec3), mesh.textureCoords->size());
    GLBufferObject textureCoordsObject(BufferType::VertexArray, usagePattern);
    textureCoordsObject.generate();
    textureCoordsObject.allocate(mesh.textureCoords->size() * sizeof(glm::vec3), mesh.textureCoords->data());

    GLVertexArrayObject& gpuVao = gpuData.vao();
    gpuVao.bind();
    {
      textureCoordsObject.bind();
      gpuVao.setAttributeBuffer(sk_textureCoordAttribute, textureCoordsInfo);
      gpuVao.enableVertexAttribute(sk_textureCoordAttribute);
    }
    gpuVao.release();

    gpuData.setTextureCoords(std::move(textureCoordsObject), textureCoordsInfo);
  }

  return gpuData;
}

} // namespace rendering::mesh
