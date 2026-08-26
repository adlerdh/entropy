#pragma once

#include "rendering/mesh/MeshData.h"
#include "rendering/mesh/MeshKeys.h"

#include <cstdint>
#include <optional>
#include <string>
#include <uuid.h>
#include <vector>

namespace rendering::mesh
{

/**
 * @brief State of a mesh extraction request in the CPU mesh cache
 */
enum class MeshCacheState
{
  Pending,
  Ready,
  Empty,
  Failed,
  Stale,
  Evicted
};

/**
 * @brief Request for extracting one scalar-image isosurface mesh
 *
 * The request contains only cache-relevant metadata. The image data itself is supplied to an extraction backend through
 * a separate boundary so the renderer does not depend on a specific meshing algorithm or image container.
 */
struct IsosurfaceMeshRequest
{
  uuids::uuid imageUid = {};             //!< Source image UID
  uint64_t imageDataVersion = 0;         //!< Version of source voxel values
  uint64_t imageGeometryVersion = 0;     //!< Version of image spatial metadata and transforms
  uint32_t component = 0;                //!< Scalar component used for extraction
  uint32_t timePoint = 0;                //!< Time frame used for extraction
  double isoValue = 0.0;                 //!< Isosurface value
  std::string algorithm = "unspecified"; //!< Extraction algorithm identifier
  uint64_t algorithmVersion = 0;         //!< Version of algorithm settings that change geometry
};

/**
 * @brief Request for extracting one segmentation-label mesh
 */
struct SegmentationMeshRequest
{
  uuids::uuid segmentationUid = {};         //!< Source segmentation UID
  uint64_t segmentationDataVersion = 0;     //!< Version of source label voxels
  uint64_t segmentationGeometryVersion = 0; //!< Version of segmentation spatial metadata and transforms
  int64_t labelValue = 0;                   //!< Label value to extract
  uint32_t timePoint = 0;                   //!< Time frame used for extraction
  std::string algorithm = "unspecified";    //!< Extraction algorithm identifier
  uint64_t algorithmVersion = 0;            //!< Version of algorithm settings that change geometry
};

/**
 * @brief Result produced by a mesh extraction backend
 */
struct MeshExtractionResult
{
  MeshGeometryKey key;                  //!< Geometry key represented by this result
  MeshData mesh;                        //!< Extracted CPU mesh
  std::vector<std::string> diagnostics; //!< Non-fatal extraction diagnostics
};

/**
 * @brief Build a geometry cache key for an isosurface extraction request
 * @param request Isosurface request
 * @return Geometry key
 */
MeshGeometryKey geometryKeyForRequest(const IsosurfaceMeshRequest& request);

/**
 * @brief Build a geometry cache key for a segmentation-label extraction request
 * @param request Segmentation request
 * @return Geometry key
 */
MeshGeometryKey geometryKeyForRequest(const SegmentationMeshRequest& request);

/**
 * @brief Interface implemented by isosurface extraction backends
 */
class IIsosurfaceMeshExtractor
{
public:
  IIsosurfaceMeshExtractor() = default;
  IIsosurfaceMeshExtractor(const IIsosurfaceMeshExtractor&) = delete;
  IIsosurfaceMeshExtractor& operator=(const IIsosurfaceMeshExtractor&) = delete;
  IIsosurfaceMeshExtractor(IIsosurfaceMeshExtractor&&) = delete;
  IIsosurfaceMeshExtractor& operator=(IIsosurfaceMeshExtractor&&) = delete;
  virtual ~IIsosurfaceMeshExtractor() = default;

  /**
   * @brief Extract a mesh for one isosurface request
   * @param request Request metadata and extraction parameters
   * @return Extraction result, or empty when the backend cannot produce a mesh
   * @throw Backend-specific extraction errors may propagate
   */
  virtual std::optional<MeshExtractionResult> extract(const IsosurfaceMeshRequest& request) = 0;
};

/**
 * @brief Interface implemented by segmentation mesh extraction backends
 */
class ISegmentationMeshExtractor
{
public:
  ISegmentationMeshExtractor() = default;
  ISegmentationMeshExtractor(const ISegmentationMeshExtractor&) = delete;
  ISegmentationMeshExtractor& operator=(const ISegmentationMeshExtractor&) = delete;
  ISegmentationMeshExtractor(ISegmentationMeshExtractor&&) = delete;
  ISegmentationMeshExtractor& operator=(ISegmentationMeshExtractor&&) = delete;
  virtual ~ISegmentationMeshExtractor() = default;

  /**
   * @brief Extract a mesh for one segmentation-label request
   * @param request Request metadata and extraction parameters
   * @return Extraction result, or empty when the backend cannot produce a mesh
   * @throw Backend-specific extraction errors may propagate
   */
  virtual std::optional<MeshExtractionResult> extract(const SegmentationMeshRequest& request) = 0;
};

} // namespace rendering::mesh
