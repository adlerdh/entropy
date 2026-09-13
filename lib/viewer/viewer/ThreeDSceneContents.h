#pragma once

#include <set>

/** @brief Independently selectable categories of renderable content in a 3D scene. */
enum class ThreeDSceneContent
{
  Segmentations,
  Isosurfaces,
  ImportedMeshes
};

/** @brief Set of content categories enabled in a 3D view or layout. */
using ThreeDSceneContents = std::set<ThreeDSceneContent>;

/** @brief Default 3D scene contents. */
inline const ThreeDSceneContents DefaultThreeDSceneContents{
  ThreeDSceneContent::Segmentations,
  ThreeDSceneContent::Isosurfaces,
  ThreeDSceneContent::ImportedMeshes};
