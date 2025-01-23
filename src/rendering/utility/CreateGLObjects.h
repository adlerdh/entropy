#ifndef CREATE_GL_OBJECTS_H
#define CREATE_GL_OBJECTS_H

#include "common/ParcellationLabelTable.h"
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

std::unique_ptr<ImageGpuRecord> createImageGpuRecord(
  const Image* imageCpuRecord,
  const uint32_t componentIndex,
  const tex::MinificationFilter& minFilter,
  const tex::MagnificationFilter& magFilter,
  bool useNormalizedIntegers
);

std::unique_ptr<MeshGpuRecord> createSliceMeshGpuRecord(
  const BufferUsagePattern& bufferUsagePattern = BufferUsagePattern::DynamicDraw
);

std::unique_ptr<MeshGpuRecord> createBoxMeshGpuRecord(
  const BufferUsagePattern& bufferUsagePattern = BufferUsagePattern::StreamDraw
);

std::unique_ptr<MeshGpuRecord> createSphereMeshGpuRecord();

std::unique_ptr<MeshGpuRecord> createCylinderMeshGpuRecord(
  const glm::dvec3& center, double radius, double height
);

std::unique_ptr<MeshGpuRecord> createCrosshairMeshGpuRecord(double coneToCylinderRatio);

std::unique_ptr<MeshGpuRecord> createMeshGpuRecord(
  std::size_t vertexCount,
  std::size_t indexCount,
  const PrimitiveMode& primitiveMode,
  const BufferUsagePattern& bufferUsagePattern = BufferUsagePattern::DynamicDraw
);

std::unique_ptr<MeshGpuRecord> createMeshGpuRecordFromVtkPolyData(
  vtkSmartPointer<vtkPolyData> polyData,
  const MeshPrimitiveType& primitiveType,
  const BufferUsagePattern& bufferUsagePattern = BufferUsagePattern::StreamDraw
);

std::unique_ptr<SlideGpuRecord> createSlideGpuRecord(const slideio::SlideCpuRecord*);

std::unique_ptr<SlideAnnotationGpuRecord> createSlideAnnotationGpuRecord(const PlanarPolygon& polygon
);

std::unique_ptr<GLTexture> createImageColorMapTexture(const ImageColorMap*);

std::unique_ptr<GLBufferTexture> createLabelColorTableTextureBuffer(const ParcellationLabelTable*);

GLTexture createBlankRGBATexture(const ComponentType& compType, const tex::Target& texTarget);

void createTestColorBuffer(MeshRecord& meshRecord);

} // namespace gpuhelper

#endif // CREATE_GL_OBJECTS_H
