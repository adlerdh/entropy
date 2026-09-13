#pragma once

namespace rendering::view_overlay
{

/// Transient filter applied to the vector overlays enabled in rendering settings.
enum class Visibility
{
  Configured,
  CrosshairsOnly,
  Hidden
};

/// Advance the overlay visibility cycle without changing any persistent overlay setting.
constexpr Visibility nextVisibility(const Visibility current, const bool crosshairsConfigured)
{
  switch (current) {
    case Visibility::Configured:
      return crosshairsConfigured ? Visibility::CrosshairsOnly : Visibility::Hidden;
    case Visibility::CrosshairsOnly:
      return Visibility::Hidden;
    case Visibility::Hidden:
      return Visibility::Configured;
  }
  return Visibility::Configured;
}

} // namespace rendering::view_overlay
