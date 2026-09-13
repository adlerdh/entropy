#include "logic/app/LargeImagePolicy.h"

namespace large_image_policy
{

bool requiresConfirmation(const std::uint64_t estimatedMemoryBytes)
{
  return estimatedMemoryBytes >= k_confirmationThresholdBytes;
}

} // namespace large_image_policy
