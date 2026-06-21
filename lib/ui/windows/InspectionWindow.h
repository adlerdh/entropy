#pragma once

#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#include <uuid.h>

#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <utility>
#include <vector>

class AppData;
class ParcellationLabelTable;

/**
 * @brief Render the compact voxel inspection window.
 * @param appData Application data containing GUI state and image/segmentation data.
 * @param numImages Number of loaded images.
 * @param getImageDisplayAndFileName Callback returning display and file names for an image index.
 * @param getWorldDeformedPos Callback returning the current deformed world position.
 * @param getSubjectPos Callback returning subject coordinates for an image index.
 * @param getVoxelPos Callback returning voxel coordinates for an image index.
 * @param getImageValueNN Callback returning nearest-neighbor image value at the crosshairs.
 * @param getImageValueLinear Callback returning linear-interpolated image value at the crosshairs.
 * @param getSegLabel Callback returning the segmentation label at the crosshairs.
 * @param getLabelTable Callback returning a parcellation label table by index.
 */
void renderInspectionWindow(
  AppData& appData,
  std::size_t numImages,
  const std::function<std::pair<std::string, std::string>(std::size_t index)>& getImageDisplayAndFileName,
  const std::function<glm::vec3()>& getWorldDeformedPos,
  const std::function<std::optional<glm::vec3>(std::size_t imageIndex)>& getSubjectPos,
  const std::function<std::optional<glm::ivec3>(std::size_t imageIndex)>& getVoxelPos,
  const std::function<std::optional<double>(std::size_t imageIndex)>& getImageValueNN,
  const std::function<std::optional<double>(std::size_t imageIndex)>& getImageValueLinear,
  const std::function<std::optional<int64_t>(std::size_t imageIndex)>& getSegLabel,
  const std::function<ParcellationLabelTable*(std::size_t tableIndex)>& getLabelTable);

/**
 * @brief Render the editable tabular voxel inspection window.
 * @param appData Application data containing GUI state and image/segmentation data.
 * @param getImageDisplayAndFileName Callback returning display and file names for an image index.
 * @param getSubjectPos Callback returning subject coordinates for an image index.
 * @param getVoxelPos Callback returning voxel coordinates for an image index.
 * @param setSubjectPos Callback that moves the crosshairs to subject coordinates for an image.
 * @param setVoxelPos Callback that moves the crosshairs to voxel coordinates for an image.
 * @param getImageValuesNN Callback returning nearest-neighbor image values.
 * @param getImageValuesLinear Callback returning linear-interpolated image values.
 * @param getSegLabel Callback returning the segmentation label at the crosshairs.
 * @param getLabelTable Callback returning a parcellation label table by index.
 * @param updateImageUniforms Callback that refreshes rendering uniforms after image settings change.
 */
void renderInspectionWindowWithTable(
  AppData& appData,
  const std::function<std::pair<std::string, std::string>(std::size_t index)>& getImageDisplayAndFileName,
  const std::function<std::optional<glm::vec3>(std::size_t imageIndex)>& getSubjectPos,
  const std::function<std::optional<glm::ivec3>(std::size_t imageIndex)>& getVoxelPos,
  const std::function<void(std::size_t imageIndex, const glm::vec3& subjectPos)> setSubjectPos,
  const std::function<void(std::size_t imageIndex, const glm::ivec3& voxelPos)> setVoxelPos,
  const std::function<std::vector<double>(std::size_t imageIndex, bool getOnlyActiveComponent)>& getImageValuesNN,
  const std::function<std::vector<double>(std::size_t imageIndex, bool getOnlyActiveComponent)>& getImageValuesLinear,
  const std::function<std::optional<int64_t>(std::size_t imageIndex)>& getSegLabel,
  const std::function<ParcellationLabelTable*(std::size_t tableIndex)>& getLabelTable,
  const std::function<void(const uuids::uuid& imageUid)>& updateImageUniforms);
