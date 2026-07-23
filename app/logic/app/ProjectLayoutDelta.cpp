#include "logic/app/ProjectLayoutDelta.h"

namespace project_layout_delta
{
namespace
{

std::optional<std::size_t> nextMatchingDefaultLayout(
  const std::vector<layout::LayoutSpec>& currentLayouts,
  const std::vector<layout::LayoutSpec>& defaultLayouts,
  std::size_t currentIndex,
  std::size_t firstDefaultIndex)
{
  for (std::size_t defaultIndex = firstDefaultIndex; defaultIndex < defaultLayouts.size(); ++defaultIndex) {
    if (currentLayouts.at(currentIndex) == defaultLayouts.at(defaultIndex)) {
      return defaultIndex;
    }
  }
  return std::nullopt;
}

} // namespace

std::optional<CompactLayoutDelta> compactLayoutDelta(
  const std::vector<layout::LayoutSpec>& currentLayouts,
  const std::vector<layout::LayoutSpec>& defaultLayouts)
{
  CompactLayoutDelta delta;
  std::size_t currentIndex = 0;
  std::size_t defaultIndex = 0;

  while (defaultIndex < defaultLayouts.size()) {
    if (currentIndex >= currentLayouts.size()) {
      delta.m_removedDefaultLayoutIndices.push_back(defaultIndex);
      ++defaultIndex;
      continue;
    }

    if (currentLayouts.at(currentIndex) == defaultLayouts.at(defaultIndex)) {
      ++currentIndex;
      ++defaultIndex;
      continue;
    }

    if (
      const auto nextDefaultIndex =
        nextMatchingDefaultLayout(currentLayouts, defaultLayouts, currentIndex, defaultIndex + 1))
    {
      for (std::size_t removedIndex = defaultIndex; removedIndex < *nextDefaultIndex; ++removedIndex) {
        delta.m_removedDefaultLayoutIndices.push_back(removedIndex);
      }
      defaultIndex = *nextDefaultIndex;
      continue;
    }

    delta.m_modifiedDefaultLayouts.push_back(
      serialize::DefaultLayoutOverride{.m_index = defaultIndex, .m_layout = currentLayouts.at(currentIndex)});
    ++currentIndex;
    ++defaultIndex;
  }

  delta.m_addedLayouts.assign(currentLayouts.begin() + static_cast<std::ptrdiff_t>(currentIndex), currentLayouts.end());

  const std::size_t expectedCurrentCount =
    defaultLayouts.size() - delta.m_removedDefaultLayoutIndices.size() + delta.m_addedLayouts.size();
  if (expectedCurrentCount != currentLayouts.size()) {
    return std::nullopt;
  }

  return delta;
}

} // namespace project_layout_delta
