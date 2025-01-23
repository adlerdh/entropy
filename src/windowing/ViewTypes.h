#ifndef VIEW_TYPES_H
#define VIEW_TYPES_H

#include <array>
#include <string>

/**
 * @brief Types of views. Each view type is assigned a starting orientation transformation,
 * a projection transformation, and orientation alignment rules.
 */
enum class ViewType
{
  Axial,    //!< Orthogonal slice view aligned with reference subject axial plane
  Coronal,  //!< Orthogonal slice view with reference subject coronal plane
  Sagittal, //!< Orthogonal slice view with reference subject sagittal plane
  Oblique,  //!< Oblique slice view, whose orientation can be rotated
  ThreeD,   //!< 3D view that is nominally aligned with reference subject coronal plane
  NumElements
};

/**
 * @brief Array of all view types accessible to the application
 */
inline std::array<ViewType, 5> const AllViewTypes
  = {ViewType::Axial, ViewType::Coronal, ViewType::Sagittal, ViewType::Oblique, ViewType::ThreeD};

/**
 * @brief Get the display string of a view type
 * @param[in] viewType
 * @param[in] crosshairsRotated
 * @return Type string
 */
std::string typeString(const ViewType& viewType, bool crosshairsRotated);

#endif // VIEW_TYPES_H
