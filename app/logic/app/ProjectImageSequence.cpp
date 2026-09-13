#include "logic/app/ProjectImageSequence.h"

#include "logic/serialization/ProjectSerialization.h"

#include <cstddef>

namespace project_image_sequence
{

std::size_t size(const serialize::EntropyProject& project)
{
  return 1u + project.m_additionalImages.size();
}

serialize::Image* at(serialize::EntropyProject& project, const std::size_t index)
{
  if (0u == index) {
    return &project.m_referenceImage;
  }

  const std::size_t additionalIndex = index - 1u;
  return additionalIndex < project.m_additionalImages.size() ? &project.m_additionalImages[additionalIndex] : nullptr;
}

const serialize::Image* at(const serialize::EntropyProject& project, const std::size_t index)
{
  if (0u == index) {
    return &project.m_referenceImage;
  }

  const std::size_t additionalIndex = index - 1u;
  return additionalIndex < project.m_additionalImages.size() ? &project.m_additionalImages[additionalIndex] : nullptr;
}

bool erase(serialize::EntropyProject& project, const std::size_t index)
{
  if (0u == index) {
    return false;
  }

  const std::size_t additionalIndex = index - 1u;
  if (additionalIndex >= project.m_additionalImages.size()) {
    return false;
  }

  project.m_additionalImages.erase(project.m_additionalImages.begin() + static_cast<std::ptrdiff_t>(additionalIndex));
  return true;
}

} // namespace project_image_sequence
