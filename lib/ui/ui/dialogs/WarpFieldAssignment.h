#pragma once

#include <string>
#include <vector>

class Image;

namespace warp_field_assignment
{

/** @brief Describe compatibility problems between an inverse warp and its reference image. */
std::vector<std::string> inverseWarnings(const Image& field, const Image& referenceImage);

/** @brief Describe compatibility problems between a forward warp and its supported image spaces. */
std::vector<std::string> forwardWarnings(const Image& field, const Image& movingImage, const Image* referenceImage);

/** @brief Confirm use of a warp field when compatibility warnings are present. */
bool confirm(const char* title, const std::vector<std::string>& warnings);

} // namespace warp_field_assignment
