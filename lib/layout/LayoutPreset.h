#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace layout
{

/**
 * @brief Compact description of a layout preset.
 *
 * Presets store layout choices as strings so saved layout files remain readable.
 * Image indices are zero-based positions in the current image list.
 */
struct LayoutPreset
{
  std::string m_type{};                      //!< Preset family, such as `fourUp`, `oneUp`, or `grid`
  std::string m_view{};                      //!< Optional view selector
  std::string m_images{};                    //!< Optional symbolic image selector, such as `all`
  std::vector<std::size_t> m_imageIndices{}; //!< Explicit image indices when no symbolic selector is used
};

} // namespace layout
