#pragma once

#include <cstdint>

namespace large_image_policy
{

inline constexpr std::uint64_t k_confirmationThresholdBytes = 2ull * 1024ull * 1024ull * 1024ull;

/** @brief Return true when an image's estimated memory use warrants confirmation before loading. */
bool requiresConfirmation(std::uint64_t estimatedMemoryBytes);

} // namespace large_image_policy
