#ifndef MESH_TYPES_H
#define MESH_TYPES_H

enum class MeshSource
{
  IsoSurface,
  Label,
  Segmentation,
  Other
};

enum class MeshPrimitiveType
{
  TriangleStrip,
  TriangleFan,
  Triangles //!< Indexed triangles
};

#endif // MESH_TYPES_H
