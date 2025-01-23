#include "rendering_old/records/MeshGpuRecord.h"

MeshGpuRecord::MeshGpuRecord(
  GLBufferObject positionsObject,
  GLBufferObject indicesObject,
  VertexAttributeInfo positionsInfo,
  VertexIndicesInfo indicesInfo
)
  : m_positionsObject(std::move(positionsObject))
  , m_normalsObject(std::nullopt)
  , m_texCoordsObject(std::nullopt)
  , m_colorsObject(std::nullopt)
  , m_indicesObject(std::move(indicesObject))
  ,

  m_positionsInfo(std::move(positionsInfo))
  , m_normalsInfo(std::nullopt)
  , m_texCoordsInfo(std::nullopt)
  , m_colorsInfo(std::nullopt)
  , m_indicesInfo(std::move(indicesInfo))
{
}

MeshGpuRecord::MeshGpuRecord(
  GLBufferObject positionsObject,
  GLBufferObject normalsObject,
  GLBufferObject texCoordsObject,
  GLBufferObject indicesObject,
  VertexAttributeInfo positionsInfo,
  VertexAttributeInfo normalsInfo,
  VertexAttributeInfo texCoordsInfo,
  VertexIndicesInfo indicesInfo
)
  : MeshGpuRecord(
    std::move(positionsObject),
    std::move(indicesObject),
    std::move(positionsInfo),
    std::move(indicesInfo)
  )
{
  m_normalsObject = std::move(normalsObject);
  m_texCoordsObject = std::move(texCoordsObject);
  m_normalsInfo = std::move(normalsInfo);
  m_texCoordsInfo = std::move(texCoordsInfo);
}

void MeshGpuRecord::setNormals(GLBufferObject normalsObject, VertexAttributeInfo normalsInfo)
{
  m_normalsObject = std::move(normalsObject);
  m_normalsInfo = std::move(normalsInfo);
}

void MeshGpuRecord::setTexCoords(GLBufferObject texCoordsObject, VertexAttributeInfo texCoordsInfo)
{
  m_texCoordsObject = std::move(texCoordsObject);
  m_texCoordsInfo = std::move(texCoordsInfo);
}

void MeshGpuRecord::setColors(GLBufferObject colorsObject, VertexAttributeInfo colorsInfo)
{
  m_colorsObject = std::move(colorsObject);
  m_colorsInfo = std::move(colorsInfo);
}

GLBufferObject& MeshGpuRecord::positionsObject()
{
  return m_positionsObject;
}

std::optional<GLBufferObject>& MeshGpuRecord::normalsObject()
{
  return m_normalsObject;
}

std::optional<GLBufferObject>& MeshGpuRecord::texCoordsObject()
{
  return m_texCoordsObject;
}

std::optional<GLBufferObject>& MeshGpuRecord::colorsObject()
{
  return m_colorsObject;
}

GLBufferObject& MeshGpuRecord::indicesObject()
{
  return m_indicesObject;
}

const VertexAttributeInfo& MeshGpuRecord::positionsInfo() const
{
  return m_positionsInfo;
}

const std::optional<VertexAttributeInfo>& MeshGpuRecord::normalsInfo() const
{
  return m_normalsInfo;
}

const std::optional<VertexAttributeInfo>& MeshGpuRecord::texCoordsInfo() const
{
  return m_texCoordsInfo;
}

const std::optional<VertexAttributeInfo>& MeshGpuRecord::colorsInfo() const
{
  return m_colorsInfo;
}

const VertexIndicesInfo& MeshGpuRecord::indicesInfo() const
{
  return m_indicesInfo;
}
