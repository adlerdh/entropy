#pragma once

#include <glm/mat3x3.hpp>
#include <glm/vec3.hpp>

#include <optional>
#include <string>

/**
 * @brief Image voxel axis whose direction was edited by the user.
 */
enum class ImageDirectionAxis
{
  I,
  J,
  K
};

/**
 * @brief User-provided physical geometry for standard 2D raster images that do not carry medical image headers.
 */
struct ImageSpatialMetadata
{
  glm::vec3 spacingMm{1.0f, 1.0f, 1.0f}; //!< Physical spacing along image axes
  glm::vec3 originMm{0.0f, 0.0f, 0.0f};  //!< Physical coordinates of voxel (0, 0, 0)
  glm::mat3 directions{1.0f};            //!< Unit direction vectors stored as columns
};

/**
 * @brief Validate a complete user-provided spatial metadata block.
 * @param metadata Metadata to validate.
 * @param errorMessage Optional destination for a readable validation error.
 * @return True when spacing is positive and direction vectors are finite and right handed.
 */
bool validateImageSpatialMetadata(const ImageSpatialMetadata& metadata, std::string* errorMessage = nullptr);

/**
 * @brief Normalize two image-plane direction vectors into a right-handed 3D direction matrix.
 * @param xDirection Direction of the image x axis.
 * @param yDirection Direction of the image y axis.
 * @param errorMessage Optional destination for a readable validation error.
 * @return Direction matrix with x, y, and z columns, or std::nullopt when invalid.
 */
std::optional<glm::mat3> normalizedRasterDirectionMatrix(
  const glm::vec3& xDirection,
  const glm::vec3& yDirection,
  std::string* errorMessage = nullptr);

/**
 * @brief Normalize an edited i, j, or k direction and rebuild the closest right-handed direction matrix.
 * @param directions Direction matrix being edited, with direction vectors stored as columns.
 * @param editedAxis Axis whose direction was just edited.
 * @param errorMessage Optional destination for a readable validation error.
 * @return Orthonormal right-handed direction matrix, or std::nullopt when the edited basis is invalid.
 */
std::optional<glm::mat3> normalizedRasterDirectionMatrixAfterEdit(
  const glm::mat3& directions,
  ImageDirectionAxis editedAxis,
  std::string* errorMessage = nullptr);
