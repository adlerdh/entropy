#ifndef DATA_HELPER_H
#define DATA_HELPER_H

#include "common/AABB.h"
#include "common/CoordinateFrame.h"
#include "common/UID.h"
#include "gui/view/ViewSliderParams.h"

#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

#include <array>
#include <optional>
#include <tuple>
#include <utility>

class DataManager;

namespace camera
{
class Camera;
}

/**
 * This is an aggregation of free functions related to spatial information about the
 * current dataset.
 */
namespace data
{

/**
 * @brief World-space AABB of the reference space, which is defined as the AABB of the active
 * reference image and of the slide stack.
 *
 * @param world_O_slideStack Transformation from slide stack to World space
 */
AABB<float> refSpaceAABBox(DataManager&, const glm::mat4& world_O_slideStack);

/**
 * @brief World-space AABB of the active reference image.
 *
 * @return \c std::nullopt if there is no active reference image.
 */
std::optional<AABB<float> > activeRefImageAABBox(DataManager&);

/**
 * @brief Get coordinate frame mapping the active image Subject to World space.
 *
 * @return \c std::nullopt if there is no active reference image.
 */
std::optional<CoordinateFrame> getActiveImageSubjectToWorldFrame(DataManager&);

/**
 * @brief For the active image (if one exists), set the coordinate frame mapping Subject to World space.
 */
void setActiveImageSubjectToWorldFrame(DataManager&, const CoordinateFrame& world_O_subject);

/**
 * @brief Diagonal length of the reference space voxels in World space coordinates.
 */
float refSpaceVoxelScale(DataManager&);

/**
 * @brief The distance by which to scroll the view plane with each "tick" of the
 * mouse scroll wheel or track pad. The distance is based on the voxel spacing of the
 * base image along the view direction.
 *
 * @param worldCameraFront Normalized front direction of the camera in World space.
 */
float refSpaceSliceScrollDistance(DataManager&, const glm::vec3& worldCameraFrontDir);

/// Positive extent of the slide stack (relative to stack frame coordinates)
float slideStackPositiveExtent(DataManager&);

/// Query whether a slide is active or not
bool isSlideActive(DataManager&, const UID& slideUid);

/// Get the parameters of the horizontal and vertical scroll bars for a given view camera
std::pair<gui::ViewSliderParams, gui::ViewSliderParams> viewScrollBarParams(
  DataManager&,
  const glm::vec3& worldCrosshairsOrigin,
  const glm::mat4& world_O_slideStack,
  const camera::Camera& camera
);

/**
 * @brief Get view slice slider parameters for a given view.
 * The slice slider values are based on the dimensions of the current base image
 * and the current crosshairs position.
 *
 * @param dataManager
 * @param crosshairs
 * @param camera
 *
 * @return
 *
 * @todo this function returns NAN value for off-screen views!
 */
gui::ViewSliderParams viewSliceSliderParams(
  DataManager&,
  const glm::vec3& worldCrosshairsOrigin,
  const glm::mat4& world_O_slideStack,
  const camera::Camera& camera
);

/// Get the default view slider parameters (applies to the scroll bars and slice sliders)
gui::ViewSliderParams defaultViewSliderParams();

} // namespace data

#endif // DATA_HELPER_H
