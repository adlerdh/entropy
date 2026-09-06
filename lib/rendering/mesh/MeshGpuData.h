#pragma once

#include "rendering/mesh/MeshHandle.h"
#include "rendering/utility/containers/VertexAttributeInfo.h"
#include "rendering/utility/containers/VertexIndicesInfo.h"
#include "rendering/utility/gl/GLBufferObject.h"
#include "rendering/utility/gl/GLVertexArrayObject.h"

#include <optional>

namespace rendering::mesh
{

/**
 * @brief OpenGL resources for one uploaded mesh geometry version
 *
 * `MeshGpuData` owns VAO/VBO/EBO state and is valid only for the OpenGL context that created it. It does not own CPU
 * mesh data and does not store material, compositing, or visibility policy.
 */
class MeshGpuData
{
public:
  /**
   * @param handle Mesh geometry handle represented by the uploaded buffers
   * @param vao Vertex array object with index and attribute bindings
   * @param positionsObject Vertex position buffer
   * @param indicesObject Triangle index buffer
   * @param positionsInfo Position attribute descriptor
   * @param indicesInfo Indexed draw descriptor
   */
  MeshGpuData(
    MeshHandle handle,
    GLVertexArrayObject vao,
    GLBufferObject positionsObject,
    GLBufferObject indicesObject,
    VertexAttributeInfo positionsInfo,
    VertexIndicesInfo indicesInfo);

  MeshGpuData(const MeshGpuData&) = delete;
  MeshGpuData& operator=(const MeshGpuData&) = delete;

  MeshGpuData(MeshGpuData&&) = default;
  MeshGpuData& operator=(MeshGpuData&&) = default;

  /**
   * @brief Attach an uploaded normal buffer
   * @param normalsObject Vertex normal buffer
   * @param normalsInfo Normal attribute descriptor
   */
  void setNormals(GLBufferObject normalsObject, const VertexAttributeInfo& normalsInfoArg);

  /**
   * @brief Attach an uploaded color buffer
   * @param colorsObject Vertex color buffer
   * @param colorsInfo Color attribute descriptor
   */
  void setColors(GLBufferObject colorsObject, const VertexAttributeInfo& colorsInfoArg);

  /**
   * @brief Attach an uploaded texture-coordinate buffer
   * @param textureCoordsObject Vertex texture-coordinate buffer
   * @param textureCoordsInfo Texture-coordinate attribute descriptor
   */
  void setTextureCoords(GLBufferObject textureCoordsObject, const VertexAttributeInfo& textureCoordsInfoArg);

  /**
   * @brief Return the mesh geometry handle represented by this upload
   * @return Mesh handle
   */
  const MeshHandle& handle() const noexcept;

  /**
   * @brief Return the vertex array object used for drawing
   * @return Vertex array object
   */
  const GLVertexArrayObject& vao() const noexcept;

  /**
   * @brief Return the vertex array object for upload-time attribute setup
   * @return Vertex array object
   */
  GLVertexArrayObject& vao() noexcept;

  /**
   * @brief Return indexed draw parameters
   * @return Draw parameters for `glDrawElements`
   */
  const GLVertexArrayObject::IndexedDrawParams& drawParams() const noexcept;

  /**
   * @brief Return whether a normal attribute buffer was uploaded
   * @return Whether normals are available
   */
  bool hasNormals() const noexcept;

  /**
   * @brief Return whether a color attribute buffer was uploaded
   * @return Whether colors are available
   */
  bool hasColors() const noexcept;

  /**
   * @brief Return whether a texture-coordinate attribute buffer was uploaded
   * @return Whether image texture coordinates are available
   */
  bool hasTextureCoords() const noexcept;

  /**
   * @brief Return the position attribute descriptor
   * @return Position attribute descriptor
   */
  const VertexAttributeInfo& positionsInfo() const noexcept;

  /**
   * @brief Return the optional normal attribute descriptor
   * @return Normal attribute descriptor, if present
   */
  const std::optional<VertexAttributeInfo>& normalsInfo() const noexcept;

  /**
   * @brief Return the optional color attribute descriptor
   * @return Color attribute descriptor, if present
   */
  const std::optional<VertexAttributeInfo>& colorsInfo() const noexcept;

  /**
   * @brief Return the optional texture-coordinate attribute descriptor
   * @return Texture-coordinate attribute descriptor, if present
   */
  const std::optional<VertexAttributeInfo>& textureCoordsInfo() const noexcept;

  /**
   * @brief Return the index draw descriptor
   * @return Index draw descriptor
   */
  const VertexIndicesInfo& indicesInfo() const noexcept;

private:
  MeshHandle m_handle;
  GLVertexArrayObject m_vao;
  GLBufferObject m_positionsObject;
  std::optional<GLBufferObject> m_normalsObject;
  std::optional<GLBufferObject> m_colorsObject;
  std::optional<GLBufferObject> m_textureCoordsObject;
  GLBufferObject m_indicesObject;
  VertexAttributeInfo m_positionsInfo;
  std::optional<VertexAttributeInfo> m_normalsInfo;
  std::optional<VertexAttributeInfo> m_colorsInfo;
  std::optional<VertexAttributeInfo> m_textureCoordsInfo;
  VertexIndicesInfo m_indicesInfo;
  GLVertexArrayObject::IndexedDrawParams m_drawParams;
};

} // namespace rendering::mesh
