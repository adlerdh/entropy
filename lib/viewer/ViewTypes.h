#pragma once

#include <array>
#include <string>

enum class AnatomicalLabelType;

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
 * @brief Return the display string for a view type under a direction-label convention.
 *
 * @param viewType View type to describe.
 * @param anatomicalLabelType Direction convention used for canonical planes.
 * @param crosshairsRotated Whether the crosshairs frame is rotated from the world axes.
 * @return User-facing view type label, or `"Unknown"` for invalid sentinel values.
 */
std::string viewTypeDisplayName(ViewType viewType, AnatomicalLabelType anatomicalLabelType, bool crosshairsRotated);
