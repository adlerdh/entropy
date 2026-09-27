#pragma once

#include <glm/mat4x4.hpp>

#include <filesystem>
#include <optional>

namespace registration
{

/**
 * @brief Read a physical-space affine transform produced by a registration backend.
 * @param fileName Transform file path.
 * @return Affine matrix in Entropy's subject-to-reference/world convention, or nullopt on failure.
 *
 * Supports plain 4x4 text matrices and ITK affine transform files used by ANTs and FireANTs.
 */
std::optional<glm::dmat4> readAffineTransform(const std::filesystem::path& fileName);

/**
 * @brief Read a Greedy affine matrix and convert it to Entropy's LPS subject-space convention.
 * @param fileName Greedy 4x4 matrix path.
 * @return Affine matrix in Entropy's subject-to-reference/world convention, or nullopt on failure.
 *
 * Greedy writes plain affine matrices in physical RAS coordinates. Entropy stores subject-space transforms in LPS
 * coordinates, so the matrix is converted with the RAS/LPS basis flip before it is returned.
 */
std::optional<glm::dmat4> readGreedyAffineTransform(const std::filesystem::path& fileName);

/**
 * @brief Write an ITK affine transform file for backend initialization.
 * @param fileName Output transform path.
 * @param matrix Subject-to-reference/world affine matrix.
 * @param dimension Spatial dimension, normally 2 or 3.
 * @return True when the transform was written.
 */
bool writeItkAffineTransform(const std::filesystem::path& fileName, const glm::dmat4& matrix, int dimension);

/**
 * @brief Write a Greedy affine matrix from Entropy's LPS subject-space convention.
 * @param fileName Output transform path.
 * @param matrix Subject-to-reference/world affine matrix in Entropy LPS coordinates.
 * @return True when the transform was written.
 *
 * Greedy expects plain affine matrices in physical RAS coordinates. Entropy stores subject-space transforms in LPS
 * coordinates, so the matrix is converted with the RAS/LPS basis flip before it is written.
 */
bool writeGreedyAffineTransform(const std::filesystem::path& fileName, const glm::dmat4& matrix);

} // namespace registration
