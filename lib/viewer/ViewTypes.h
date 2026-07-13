#pragma once

#include <array>
#include <string>

/** @brief View orientation/type. */
enum class ViewType
{
  Axial,    //!< Axial slice view
  Coronal,  //!< Coronal slice view
  Sagittal, //!< Sagittal slice view
  Oblique,  //!< Rotatable oblique slice view
  ThreeD,   //!< 3D view
  NumElements
};

/** @brief View types in UI order. */
inline std::array<ViewType, 5> const AllViewTypes{
  ViewType::Axial,
  ViewType::Coronal,
  ViewType::Sagittal,
  ViewType::Oblique,
  ViewType::ThreeD};

/**
 * @brief Return the display string for a view type.
 *
 * @param viewType View type to describe.
 * @param crosshairsRotated Whether the crosshairs are currently rotated,
 * which affects the user-facing label for oblique-capable views.
 * @return User-facing view type label, or `"Unknown"` for invalid sentinel values.
 */
std::string to_string(const ViewType& viewType, bool crosshairsRotated);
