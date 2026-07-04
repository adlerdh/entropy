#pragma once

#include "common/UuidUtility.h"

#include <glm/glm.hpp>

#include <optional>

class AppData;
class Image;

namespace deformation_warp
{

/**
 * @brief Convert a homogeneous point to 3D Cartesian coordinates.
 * @param point Homogeneous point.
 * @return Cartesian point after division by w.
 */
glm::vec3 homogeneousPointToVec3(const glm::vec4& point);

/**
 * @brief Return true when a warp field grid occupies the same image domain.
 */
bool warpFieldMatchesImageDomain(const Image& warpField, const Image& image);

/**
 * @brief Sample a 3-component warp field in world coordinates.
 * @param warpField Warp-field image storing displacements in physical LPS/world units.
 * @param worldPos World position where the warp field is sampled.
 * @return Sampled displacement, or std::nullopt when the point lies outside the field.
 */
std::optional<glm::vec3> sampleWarpDisplacementWorld(const Image& warpField, const glm::vec3& worldPos);

/**
 * @brief Map a displayed reference-space point into moving-image sample space with the active inverse warp.
 * @param appData Application data store.
 * @param imageUid Image whose inverse warp is applied.
 * @param worldPos Displayed/reference-space world position.
 * @return Moving-image sample position in world coordinates. Returns \p worldPos when no valid inverse warp applies.
 */
glm::vec4
inverseWarpSampleWorldPosition(const AppData& appData, const uuids::uuid& imageUid, const glm::vec4& worldPos);

/**
 * @brief Map a moving-image point into displayed reference space with the active forward warp.
 * @param appData Application data store.
 * @param imageUid Image whose forward warp is applied.
 * @param worldPos Moving-image world position.
 * @return Display/reference-space world position. Returns \p worldPos when no valid forward warp applies.
 */
glm::vec4
forwardWarpDisplayWorldPosition(const AppData& appData, const uuids::uuid& imageUid, const glm::vec4& worldPos);

} // namespace deformation_warp
