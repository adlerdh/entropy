#ifndef MESH_LOADER_H
#define MESH_LOADER_H

#include "common/AsyncTasks.h"
#include "mesh/MeshCpuRecord.h"

#include <functional>
#include <future>
#include <memory>
#include <string>

class Image;

std::future<AsyncTaskDetails> generateIsosurfaceMeshCpuRecord(
  const Image& image,
  const uuids::uuid& imageUid,
  uint32_t component,
  double isoValue,
  const uuids::uuid& isosurfaceUid,
  std::function<bool(const uuids::uuid& isosurfaceUid, std::unique_ptr<MeshCpuRecord>)>
    meshCpuRecordUpdater,
  std::function<void()> addTaskToIsosurfaceGpuMeshGenerationQueue
);

/// @todo Put this function here
//std::map< int64_t, double >
//generateImageHistogramAtLabelValues(
//        vtkImageData* imageData,
//        const std::unordered_set<int64_t>& labelValues );

bool writeMeshToFile(const MeshCpuRecord&, const std::string& fileName);

#endif // MESH_LOADER_H
