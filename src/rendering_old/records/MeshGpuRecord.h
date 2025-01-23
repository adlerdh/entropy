#ifndef MESH_GPU_RECORD_H
#define MESH_GPU_RECORD_H

#include "rendering/utility/containers/VertexAttributeInfo.h"
#include "rendering/utility/containers/VertexIndicesInfo.h"
#include "rendering/utility/gl/GLBufferObject.h"

#include <memory>
#include <optional>

class MeshGpuRecord
{
public:
  MeshGpuRecord(
    GLBufferObject positionsObject,
    GLBufferObject indicesObject,
    VertexAttributeInfo positionsInfo,
    VertexIndicesInfo indicesInfo
  );

  MeshGpuRecord(
    GLBufferObject positionsObject,
    GLBufferObject normalsObject,
    GLBufferObject texCoordsObject,
    GLBufferObject indicesObject,
    VertexAttributeInfo positionsInfo,
    VertexAttributeInfo normalsInfo,
    VertexAttributeInfo texCoordsInfo,
    VertexIndicesInfo indicesInfo
  );

  MeshGpuRecord() = delete;

  MeshGpuRecord(const MeshGpuRecord&) = delete;
  MeshGpuRecord& operator=(const MeshGpuRecord&) = delete;

  MeshGpuRecord(MeshGpuRecord&&) = default;
  MeshGpuRecord& operator=(MeshGpuRecord&&) = default;

  ~MeshGpuRecord() = default;

  void setNormals(GLBufferObject normalsObject, VertexAttributeInfo normalsInfo);

  void setTexCoords(GLBufferObject texCoordsObject, VertexAttributeInfo texCoordsInfo);

  void setColors(GLBufferObject colorsObject, VertexAttributeInfo colorsInfo);

  // Return as non-const reference, since users need access to
  // non-const member functions of GLTexture
  GLBufferObject& positionsObject();
  std::optional<GLBufferObject>& normalsObject();
  std::optional<GLBufferObject>& texCoordsObject();
  std::optional<GLBufferObject>& colorsObject();
  GLBufferObject& indicesObject();

  const VertexAttributeInfo& positionsInfo() const;
  const std::optional<VertexAttributeInfo>& normalsInfo() const;
  const std::optional<VertexAttributeInfo>& texCoordsInfo() const;
  const std::optional<VertexAttributeInfo>& colorsInfo() const;
  const VertexIndicesInfo& indicesInfo() const;

  BufferComponentType componentType() const;
  BufferNormalizeValues normalizeValues() const;

private:
  GLBufferObject m_positionsObject;
  std::optional<GLBufferObject> m_normalsObject;
  std::optional<GLBufferObject> m_texCoordsObject;
  std::optional<GLBufferObject> m_colorsObject;
  GLBufferObject m_indicesObject;

  VertexAttributeInfo m_positionsInfo;
  std::optional<VertexAttributeInfo> m_normalsInfo;
  std::optional<VertexAttributeInfo> m_texCoordsInfo;
  std::optional<VertexAttributeInfo> m_colorsInfo;
  VertexIndicesInfo m_indicesInfo;
};

#endif // MESH_GPU_RECORD_H
