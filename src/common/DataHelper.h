#ifndef DATA_HELPER_H
#define DATA_HELPER_H

#include "windowing/View.h"

#include "common/AABB.h"
#include "common/Types.h"

#include <glm/fwd.hpp>

#include <uuid.h>

#include <optional>
#include <string>
#include <vector>

class Annotation;
class AppData;
class Image;

namespace camera
{
class Camera;
}

/**
 * This is an aggregation of free functions for helping out with application data.
 */
namespace data
{

std::vector<uuids::uuid> selectImages(
  const AppData& data, const ImageSelection& selection, const View* view
);

/**
 * @brief Compute the distance by which to scroll the view plane with each "tick" of the
 * mouse scroll wheel or track pad. The distance is based on the minimum voxel spacing
 * of a given set of images along the view camera's direction in World space.
 *
 * @param worldCameraFront Normalized front direction of the camera in World space.
 */
float sliceScrollDistance(
  const AppData&, const glm::vec3& worldCameraFrontDir, const ImageSelection&, const View* view
);

float sliceScrollDistance(const glm::vec3& worldCameraFrontDir, const Image&);

glm::vec2 sliceMoveDistance(
  const AppData&,
  const glm::vec3& worldCameraRightDir,
  const glm::vec3& worldCameraUpDir,
  const ImageSelection&,
  const View* view
);

float computeViewOffsetDistance(
  const AppData& appData, const ViewOffsetSetting& offsetSetting, const glm::vec3& worldCameraFront
);

/**
 * @brief Compute the enclosing World-space AABB of the given image selection
 * @return AABB in World-space coordinates
 */
AABB<float> computeWorldAABBoxEnclosingImages(
  const AppData& appData, const ImageSelection& imageSelection
);

std::optional<uuids::uuid> createLabelColorTableForSegmentation(
  AppData& appData, const uuids::uuid& segUid
);

std::optional<glm::ivec3> getImageVoxelCoordsAtCrosshairs(const AppData& appData, size_t imageIndex);

std::optional<glm::vec3> getImageVoxelCoordsContinuousAtCrosshairs(
  const AppData& appData, size_t imageIndex
);

std::optional<glm::ivec3> getSegVoxelCoordsAtCrosshairs(
  const AppData& appData, const uuids::uuid& segUid, const uuids::uuid& matchingImgUid
);

/**
 * @brief Find annotation for a given image. The search is done by matching the
 * annotation plane equations. The orientation of the plane normal vector does not matter.
 *
 * @param appData Application data
 * @param imageUid UID of image to search
 * @param querySubjectPlaneEquation Plane equation (in Subject space) to search with
 * @param planeDistanceThresh Threshold distance
 * @return Vector of matching annotation UIDs
 */
std::vector<uuids::uuid> findAnnotationsForImage(
  const AppData& appData,
  const uuids::uuid& imageUid,
  const glm::vec4& querySubjectPlaneEquation,
  float planeDistanceThresh
);

glm::vec3 roundPointToNearestImageVoxelCenter(const Image& image, const glm::vec3& worldPos);

std::string getAnnotationSubjectPlaneName(const Annotation&);

std::optional<uuids::uuid> getSelectedAnnotation(const AppData& appData);

glm::vec3 snapWorldPointToImageVoxels(
  const AppData& appData,
  const glm::vec3& worldPos,
  const std::optional<CrosshairsSnapping>& force = std::nullopt
);

std::size_t computeNumImageSlicesAlongWorldDirection(const Image& image, const glm::vec3& worldDir);

} // namespace data

#endif // DATA_HELPER_H
