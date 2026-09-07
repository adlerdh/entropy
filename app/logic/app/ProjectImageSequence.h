#pragma once

#include <cstddef>

namespace serialize
{
struct EntropyProject;
struct Image;
} // namespace serialize

namespace project_image_sequence
{

/** @brief Return the number of reference and additional images in a project. */
std::size_t size(const serialize::EntropyProject& project);

/** @brief Return the project image at a reference-first flattened index, or nullptr when out of range. */
serialize::Image* at(serialize::EntropyProject& project, std::size_t index);
const serialize::Image* at(const serialize::EntropyProject& project, std::size_t index);

/**
 * @brief Erase an additional image at a flattened project-image index.
 * @return True when an image was erased. The reference image at index zero cannot be erased.
 */
bool erase(serialize::EntropyProject& project, std::size_t index);

} // namespace project_image_sequence
