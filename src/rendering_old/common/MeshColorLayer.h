#ifndef MESH_COLOR_LAYER_H
#define MESH_COLOR_LAYER_H

#include <cstdint>

/**
 * @brief Defines the layers that are used to color a TexturedMesh. The default rendering order of
 * the layers is as listed in this enumeration. However, the layer order can be changed in the Mesh class.
 */
enum class TexturedMeshColorLayer : uint32_t
{
  /// A material color that is constant over the whole mesh.
  /// Currently used in HistoloZee to color
  /// - label meshes according to their label value
  /// - iso-surface meshes according to their iso-surface value
  /// - crosshairs
  Material = 0,

  /// Per-vertex colors (can be different for each mesh vertex).
  /// Not currently used in HistoloZee. Could be used for
  /// visualizing scalar functions on meshes, e.g. distance functions.
  Vertex,

  /// A 2D image that is textured over the mesh vertices according to
  /// 2D (s,t) texture coordinates. Used in HistoloZee to visualize
  /// histology slides.
  Image2D,

  /// A 3D image that is textured over the mesh vertices according
  /// to 3D (s,t,p) texture coordinates. Used in HistoloZee to visualize
  /// 3D reference images (e.g. MRI).
  Image3D,

  /// A 3D image that is textured over the mesh vertices according
  /// to 3D (s,t,p) texture coordinates. HistoloZee interprets this
  /// layer differently from Image3D, in that the color of the Parcel3D
  /// layer is based on a look-up in a label color table.
  /// This layer should usually be rendered on top of the Image3D layer.
  Parcellation3D,

  /// Number of layers
  NumLayers
};

/**
 * @brief Defines the layers that are used to color a BasicMesh.
 */
enum class BasicMeshColorLayer : uint32_t
{
  /// A material color that is constant over the whole mesh.
  /// Currently used in HistoloZee to color
  /// - label meshes according to their label value
  /// - iso-surface meshes according to their iso-surface value
  /// - crosshairs
  Material = 0,

  /// Per-vertex colors (can be different for each mesh vertex).
  /// Not currently used in HistoloZee. Could be used for
  /// visualizing scalar functions on meshes, e.g. distance functions.
  Vertex,

  /// Number of layers
  NumLayers
};

#endif // MESH_COLOR_LAYER_H
