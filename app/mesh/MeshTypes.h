#pragma once

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
