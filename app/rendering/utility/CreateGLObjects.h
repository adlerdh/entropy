#pragma once

#include "logic/app/ParcellationLabelTable.h"
#include "image/ImageColorMap.h"

#include "rendering/records/MeshGpuRecord.h"

#include "rendering/utility/gl/GLBufferTexture.h"
#include "rendering/utility/gl/GLTexture.h"
#include "rendering/utility/gl/GLTextureTypes.h"

#include <memory>

namespace gpuhelper
{

/**
 * @brief Create GPU mesh buffers for the canonical slice mesh.
 * @param bufferUsagePattern OpenGL buffer usage hint.
 * @return GPU mesh record containing positions and indices.
 */
std::unique_ptr<MeshGpuRecord> createSliceMeshGpuRecord(
  const BufferUsagePattern& bufferUsagePattern = BufferUsagePattern::DynamicDraw);

/**
 * @brief Create GPU mesh buffers for a box mesh.
 * @param bufferUsagePattern OpenGL buffer usage hint.
 * @return GPU mesh record containing positions, normals, and indices.
 */
std::unique_ptr<MeshGpuRecord> createBoxMeshGpuRecord(
  const BufferUsagePattern& bufferUsagePattern = BufferUsagePattern::StreamDraw);

/**
 * @brief Create GPU mesh buffers for a unit sphere mesh.
 * @return GPU mesh record containing positions, normals, and indices.
 */
std::unique_ptr<MeshGpuRecord> createSphereMeshGpuRecord();

/**
 * @brief Create GPU mesh buffers for a cylinder mesh.
 * @param center Cylinder center in model coordinates.
 * @param radius Cylinder radius.
 * @param height Cylinder height.
 * @return GPU mesh record containing positions, normals, and indices.
 */
std::unique_ptr<MeshGpuRecord> createCylinderMeshGpuRecord(const glm::dvec3& center, double radius, double height);

/**
 * @brief Create GPU mesh buffers for the 3D crosshair glyph.
 * @param coneToCylinderRatio Ratio controlling cone size relative to the cylinder shaft.
 * @return GPU mesh record containing positions, normals, and indices.
 */
std::unique_ptr<MeshGpuRecord> createCrosshairMeshGpuRecord(double coneToCylinderRatio);

/**
 * @brief Create an empty GPU mesh record with allocated vertex and index buffers.
 * @param vertexCount Number of vertices to allocate.
 * @param indexCount Number of indices to allocate.
 * @param primitiveMode OpenGL primitive mode used to draw the mesh.
 * @param bufferUsagePattern OpenGL buffer usage hint.
 * @return GPU mesh record with allocated buffers.
 */
std::unique_ptr<MeshGpuRecord> createMeshGpuRecord(
  std::size_t vertexCount,
  std::size_t indexCount,
  const PrimitiveMode& primitiveMode,
  const BufferUsagePattern& bufferUsagePattern = BufferUsagePattern::DynamicDraw);

/**
 * @brief Create a 1D texture for an image color map.
 * @param colorMap Image color map to upload.
 * @return Texture object, or null if the input is invalid.
 */
std::unique_ptr<GLTexture> createImageColorMapTexture(const ImageColorMap* colorMap);

/**
 * @brief Create a texture buffer containing segmentation label colors.
 * @param labelTable Label color table to upload.
 * @return Buffer texture object, or null if the input is invalid.
 */
std::unique_ptr<GLBufferTexture> createLabelColorTableTextureBuffer(const ParcellationLabelTable* labelTable);

/**
 * @brief Create a blank RGBA texture for the requested component type and texture target.
 * @param compType Texture component type.
 * @param texTarget OpenGL texture target.
 * @return Blank texture object.
 */
GLTexture createBlankRGBATexture(const ComponentType& compType, const tex::Target& texTarget);

} // namespace gpuhelper
