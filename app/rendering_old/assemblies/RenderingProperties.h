#ifndef ASSEMBLY_RENDERING_PROPERTIES_H
#define ASSEMBLY_RENDERING_PROPERTIES_H

#include <glm/vec3.hpp>

/**
 * @brief Rendering properties of an ImageSliceAssembly
 */
struct ImageSliceAssemblyRenderingProperties
{
  /// Master opacity multiplier of image slices
  float m_masterOpacityMultiplier = 1.0f;

  /// Master visibility of image slices in 2D view types
  bool m_visibleIn2dViews = true;

  /// Master visibility of image slices in 3D view types
  bool m_visibleIn3dViews = true;

  /// Auto-hiding mode modulates opacity of image slices as the cosine of the angle
  /// between the view direction and the slice normal vector. Auto-hiding only applies
  /// to 3D slices and NOT to 2D slices, which are always parallel to the viewer.
  bool m_useAutoHidingMode = true;

  /// Show line outline around image slices
  bool m_showOutline = true;

  /// Show parcellation in 2D view types
  bool m_showParcellationIn2dViews = true;

  /// Show parcellation in 3D view types
  bool m_showParcellationIn3dViews = true;

  /// Whether point-picking works on the image slices
  bool m_pickable = true;
};

/**
 * @brief Rendering properties of an AnnotationAssembly
 */
struct AnnotationAssemblyRenderingProperties
{
  /// Master opacity multiplier
  float m_masterOpacityMultiplier = 1.0f;

  /// Master visibility in 2D view types
  bool m_visibleIn2dViews = true;

  /// Master visibility in 3D view types
  bool m_visibleIn3dViews = true;

  /// Whether point-picking works
  bool m_pickable = true;
};

/**
 * @brief Rendering properties of a LandmarkAssembly
 */
struct LandmarkAssemblyRenderingProperties
{
  /// Master opacity multiplier of landmarks
  float m_masterOpacityMultiplier = 1.0f;

  /// Master visibility of landmarks in 2D view types
  bool m_visibleIn2dViews = true;

  /// Master visibility of landmarks in 3D view types
  bool m_visibleIn3dViews = true;

  /// Whether point-picking works on the landmarks
  bool m_pickable = true;

  /// Color
  /// @todo Include this and opacity in the Landmark CPU record instead
  glm::vec3 m_materialColor = {0.0f, 0.5f, 1.0f};
};

/**
 * @brief Rendering properties of a MeshAssembly
 */
struct MeshAssemblyRenderingProperties
{
  /// Master opacity multiplier of meshes
  float m_masterOpacityMultiplier = 1.0f;

  /// Master visibility of meshes in 2D view types
  bool m_visibleIn2dViews = false;

  /// Master visibility of meshes in 3D view types
  bool m_visibleIn3dViews = true;

  /// Render meshes using "x-ray mode", which highlights their borders and
  /// makes the interior transparent. Mesh fragment transparency is modulated as
  /// a positive power of the cosine of the angle between the view direction and the mesh
  /// fragment normal vector.
  bool m_useXrayMode = false;

  /// Exponent to which the cosine transparency modulation factor is raised.
  /// Higher x-ray power makes meshes more "see-through".
  float m_xrayPower = 3.0f;

  /// Enable clipping (i.e. discarding) mesh fragments in the octant nearest the viewer's eye.
  /// The origin of the clipped octant is the crosshair posisiton.
  bool m_useOctantClipPlanes = false;

  /// Opacity of 3D image and its parcellation that is textured over meshes
  float m_image3dLayerOpacity = 0.0f;

  /// Whether point-picking works on meshes
  bool m_pickable = true;
};

/**
 * @brief Rendering properties of a SlideStackAssembly
 */
struct SlideStackAssemblyRenderingProperties
{
  /// Master opacity multiplier of slides
  float m_masterOpacityMultiplier = 1.0f;

  /// Master visibility of slides in 2D view types
  bool m_visibleIn2dViews = true;

  /// Master visibility of slides in 3D view types
  bool m_visibleIn3dViews = true;

  /// Opacity of 3D image and its parcellation that is textured over the slides
  float m_image3dLayerOpacity = 0.0f;

  /// Whether point-picking works on slides
  bool m_pickable = true;

  /// Whether the active slide stack view (Stack_ActiveSlide) displays the slides as
  /// 2D sections (default) or as 3D objects (used for blending slides)
  bool m_activeSlideViewShows2dSlides = true;
};

#endif // ASSEMBLY_RENDERING_PROPERTIES_H
