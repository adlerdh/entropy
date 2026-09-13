#include "rendering/mesh/MeshGpuData.h"

#include <utility>

namespace rendering::mesh
{

MeshGpuData::MeshGpuData(
  MeshHandle handle,
  GLVertexArrayObject vao,
  GLBufferObject positionsObject,
  GLBufferObject indicesObject,
  VertexAttributeInfo positionsInfo,
  VertexIndicesInfo indicesInfo)
  : m_handle(handle)
  , m_vao(std::move(vao))
  , m_positionsObject(std::move(positionsObject))
  , m_normalsObject(std::nullopt)
  , m_colorsObject(std::nullopt)
  , m_textureCoordsObject(std::nullopt)
  , m_indicesObject(std::move(indicesObject))
  , m_positionsInfo(positionsInfo)
  , m_normalsInfo(std::nullopt)
  , m_colorsInfo(std::nullopt)
  , m_textureCoordsInfo(std::nullopt)
  , m_indicesInfo(indicesInfo)
  , m_drawParams(m_indicesInfo)
{
}

void MeshGpuData::setNormals(GLBufferObject normalsObject, const VertexAttributeInfo& normalsInfoArg)
{
  m_normalsObject = std::move(normalsObject);
  m_normalsInfo = normalsInfoArg;
}

void MeshGpuData::setColors(GLBufferObject colorsObject, const VertexAttributeInfo& colorsInfoArg)
{
  m_colorsObject = std::move(colorsObject);
  m_colorsInfo = colorsInfoArg;
}

void MeshGpuData::setTextureCoords(GLBufferObject textureCoordsObject, const VertexAttributeInfo& textureCoordsInfoArg)
{
  m_textureCoordsObject = std::move(textureCoordsObject);
  m_textureCoordsInfo = textureCoordsInfoArg;
}

const MeshHandle& MeshGpuData::handle() const noexcept
{
  return m_handle;
}

const GLVertexArrayObject& MeshGpuData::vao() const noexcept
{
  return m_vao;
}

GLVertexArrayObject& MeshGpuData::vao() noexcept
{
  return m_vao;
}

const GLVertexArrayObject::IndexedDrawParams& MeshGpuData::drawParams() const noexcept
{
  return m_drawParams;
}

bool MeshGpuData::hasNormals() const noexcept
{
  return m_normalsObject.has_value();
}

bool MeshGpuData::hasColors() const noexcept
{
  return m_colorsObject.has_value();
}

bool MeshGpuData::hasTextureCoords() const noexcept
{
  return m_textureCoordsObject.has_value();
}

const VertexAttributeInfo& MeshGpuData::positionsInfo() const noexcept
{
  return m_positionsInfo;
}

const std::optional<VertexAttributeInfo>& MeshGpuData::normalsInfo() const noexcept
{
  return m_normalsInfo;
}

const std::optional<VertexAttributeInfo>& MeshGpuData::colorsInfo() const noexcept
{
  return m_colorsInfo;
}

const std::optional<VertexAttributeInfo>& MeshGpuData::textureCoordsInfo() const noexcept
{
  return m_textureCoordsInfo;
}

const VertexIndicesInfo& MeshGpuData::indicesInfo() const noexcept
{
  return m_indicesInfo;
}

} // namespace rendering::mesh
