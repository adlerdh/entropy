#ifndef SCENE_TYPE_H
#define SCENE_TYPE_H

/**
 * @brief Types of scenes that can be rendered in views. Each scene contains a collection
 * of "assemblies" to be rendered in a manner specific to the scene type.
 */
enum class SceneType
{
  /// Primarily used for viewing planar cross-sections of the reference image and slide stack.
  /// Reference image slices face the viewer in these scenes.
  ReferenceImage2d,

  /// Primarily used for viewing the reference image and slide stack in 3D.
  /// Reference image slices are show in tri-planar orthogonal configuration.
  ReferenceImage3d,

  /// Primarily used for viewing planar cross-sections of the reference image and slide stack.
  SlideStack2d,

  /// Used for viewing the slides head-on. No reference imagery is rendered.
  /// Slides are rendered as 3D boxes.
  SlideStack3d,

  /// Used for viewing only the reference image (and no slides) in Registration views.
  Registration_Image2d,

  /// Used for viewing only the slides (and no reference image) in Registration views.
  Registration_Slide2d,

  /// No scene.
  None
};

#endif // SCENE_TYPE_H
