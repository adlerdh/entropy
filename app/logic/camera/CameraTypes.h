#pragma once

#include "viewer/ViewModes.h"

#include <string>

/**
 * @brief Types of camera projections
 */
enum class ProjectionType
{
  Orthographic, //!< Parallel projection without perspective foreshortening

  Perspective //!< Perspective projection with distance-dependent foreshortening
};

/**
 * @brief Shader group
 */
enum class ShaderGroup
{
  Image,
  Metric,
  None,
  NumElements
};

/**
 * @brief Get the display string of a projection type
 * @param[in] projectionType
 * @return Type string
 */
std::string typeString(const ProjectionType& projectionType);

/**
 * @brief Get the 2D image shader group for a view render mode
 * @param[in] renderMode
 * @return Shader group
 */
ShaderGroup getTwoDShaderGroup(const ViewRenderMode& renderMode);
