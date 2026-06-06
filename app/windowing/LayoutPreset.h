#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace layout
{

struct LayoutPreset
{
  std::string m_type;
  std::string m_view;
  std::string m_images;
  std::vector<std::size_t> m_imageIndices;
};

} // namespace layout
