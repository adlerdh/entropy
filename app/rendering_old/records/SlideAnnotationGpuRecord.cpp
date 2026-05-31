#include "rendering_old/records/SlideAnnotationGpuRecord.h"

SlideAnnotationGpuRecord::SlideAnnotationGpuRecord(std::shared_ptr<MeshGpuRecord> meshGpuRecord)
  : m_meshGpuRecord(meshGpuRecord)
{
}

void SlideAnnotationGpuRecord::setMeshGpuRecord(std::shared_ptr<MeshGpuRecord> meshGpuRecord)
{
  m_meshGpuRecord = std::move(meshGpuRecord);
}

std::weak_ptr<MeshGpuRecord> SlideAnnotationGpuRecord::getMeshGpuRecord()
{
  return m_meshGpuRecord;
}
