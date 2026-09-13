#include "rendering/mesh/MeshExtraction.h"

namespace rendering::mesh
{

MeshGeometryKey geometryKeyForRequest(const IsosurfaceMeshRequest& request)
{
  return MeshGeometryKey{
    .sourceUid = request.imageUid,
    .sourceDataVersion = request.imageDataVersion,
    .sourceGeometryVersion = request.imageGeometryVersion,
    .component = request.component,
    .labelValue = std::nullopt,
    .timePoint = request.timePoint,
    .isoValue = request.isoValue,
    .extractionAlgorithm = request.algorithm,
    .extractionAlgorithmVersion = request.algorithmVersion};
}

MeshGeometryKey geometryKeyForRequest(const SegmentationMeshRequest& request)
{
  return MeshGeometryKey{
    .sourceUid = request.segmentationUid,
    .sourceDataVersion = request.segmentationDataVersion,
    .sourceGeometryVersion = request.segmentationGeometryVersion,
    .component = std::nullopt,
    .labelValue = request.labelValue,
    .timePoint = request.timePoint,
    .isoValue = 0.0,
    .extractionAlgorithm = request.algorithm,
    .extractionAlgorithmVersion = request.algorithmVersion};
}

} // namespace rendering::mesh
