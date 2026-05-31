#ifndef SLIDE_ANNOTATION_GPU_RECORD_H
#define SLIDE_ANNOTATION_GPU_RECORD_H

#include "rendering_old/records/MeshGpuRecord.h"

#include <memory>

class SlideAnnotationGpuRecord
{
public:
  explicit SlideAnnotationGpuRecord(std::shared_ptr<MeshGpuRecord>);

  SlideAnnotationGpuRecord(const SlideAnnotationGpuRecord&) = default;
  SlideAnnotationGpuRecord& operator=(const SlideAnnotationGpuRecord&) = default;

  SlideAnnotationGpuRecord(SlideAnnotationGpuRecord&&) = default;
  SlideAnnotationGpuRecord& operator=(SlideAnnotationGpuRecord&&) = default;

  ~SlideAnnotationGpuRecord() = default;

  void setMeshGpuRecord(std::shared_ptr<MeshGpuRecord>);

  std::weak_ptr<MeshGpuRecord> getMeshGpuRecord();

private:
  std::shared_ptr<MeshGpuRecord> m_meshGpuRecord;
};

#endif // SLIDE_ANNOTATION_GPU_RECORD_H
