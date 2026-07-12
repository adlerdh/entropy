#pragma once

#include "logic/app/ParcellationLabelTable.h"
#include "image/ImageColorMap.h"

#include "logic/records/MeshRecord.h"
#include "logic_old/records/ImageRecord.h"
#include "logic_old/records/SlideAnnotationRecord.h"
#include "logic_old/records/SlideRecord.h"

#include "rendering/utility/gl/GLBufferTexture.h"
#include "rendering/utility/gl/GLTexture.h"
#include "rendering/utility/gl/GLTextureTypes.h"

#include <vtkPolyData.h>
#include <vtkSmartPointer.h>

#include <memory>

class PlanarPolygon;

namespace gpuhelper
{

/**
 * @brief Create a GPU texture record for one component of a CPU image record.
 *
 * The texture is uploaded as a 3D texture using the image's native component type. Integer images can be uploaded as
 * normalized or integer textures depending on `useNormalizedIntegers`.
 *
 * @param imageCpuRecord Image data source.
 * @param componentIndex Image component to upload.
 * @param minFilter Texture minification filter.
 * @param magFilter Texture magnification filter.
 * @param useNormalizedIntegers Whether integer data should be uploaded through normalized integer texture formats.
 * @return GPU image record, or null if texture creation fails.
 */
std::unique_ptr<ImageGpuRecord> createImageGpuRecord(
  const Image* imageCpuRecord,
  const uint32_t componentIndex,
  const tex::MinificationFilter& minFilter,
  const tex::MagnificationFilter& magFilter,
  bool useNormalizedIntegers);

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
 * @brief Convert VTK polygonal data into a GPU mesh record.
 * @param polyData VTK polydata containing points, normals, and primitive connectivity.
 * @param primitiveType Mesh primitive interpretation for the generated record.
 * @param bufferUsagePattern OpenGL buffer usage hint.
 * @return GPU mesh record, or null if required VTK arrays cannot be extracted.
 */
std::unique_ptr<MeshGpuRecord> createMeshGpuRecordFromVtkPolyData(
  vtkSmartPointer<vtkPolyData> polyData,
  const MeshPrimitiveType& primitiveType,
  const BufferUsagePattern& bufferUsagePattern = BufferUsagePattern::StreamDraw);

/**
 * @brief Create a GPU record for a slide image.
 * @param slideCpuRecord CPU slide record containing the source image data.
 * @return GPU slide record, or null if texture creation fails.
 */
std::unique_ptr<SlideGpuRecord> createSlideGpuRecord(const slideio::SlideCpuRecord*);

/**
 * @brief Create a GPU record for a slide annotation polygon.
 * @param polygon Planar polygon to upload.
 * @return GPU slide annotation record.
 */
std::unique_ptr<SlideAnnotationGpuRecord> createSlideAnnotationGpuRecord(const PlanarPolygon& polygon);

/**
 * @brief Create a 1D texture for an image color map.
 * @param colorMap Image color map to upload.
 * @return Texture object, or null if the input is invalid.
 */
std::unique_ptr<GLTexture> createImageColorMapTexture(const ImageColorMap*);

/**
 * @brief Create a texture buffer containing segmentation label colors.
 * @param labelTable Label color table to upload.
 * @return Buffer texture object, or null if the input is invalid.
 */
std::unique_ptr<GLBufferTexture> createLabelColorTableTextureBuffer(const ParcellationLabelTable*);

/**
 * @brief Create a blank RGBA texture for the requested component type and texture target.
 * @param compType Texture component type.
 * @param texTarget OpenGL texture target.
 * @return Blank texture object.
 */
GLTexture createBlankRGBATexture(const ComponentType& compType, const tex::Target& texTarget);

/**
 * @brief Fill a mesh record with a small test color buffer.
 * @param meshRecord Mesh record that receives the generated color buffer.
 */
void createTestColorBuffer(MeshRecord& meshRecord);

} // namespace gpuhelper
