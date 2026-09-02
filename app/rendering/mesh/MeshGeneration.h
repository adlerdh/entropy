#pragma once

#include "rendering/mesh/MeshData.h"
#include "rendering/mesh/MeshScalarGrid.h"

#include <cstddef>
#include <cstdint>
#include <optional>

namespace rendering::mesh
{

/**
 * @brief Runtime options for CPU mesh generation backends
 */
struct MeshGenerationOptions
{
  /**
   * @brief Maximum number of CPU threads the backend may use
   *
   * A value of zero chooses a conservative automatic count that leaves CPU capacity for the UI, texture uploads, and
   * other background work.
   */
  std::size_t threadCount = 0;

  bool smoothSurface = true;         //!< Apply boundary-preserving smoothing to the extracted surface
  uint32_t smoothingIterations = 25; //!< Windowed-sinc iterations when smoothing is enabled
  double smoothingPassBand = 0.1;    //!< Windowed-sinc pass band in the open interval (0, 2]
};

/**
 * @brief Generate an isosurface mesh from a scalar volume using VTK Flying Edges 3D
 *
 * The public API stays VTK-free. The implementation converts the scalar grid to `vtkImageData`, runs Flying Edges,
 * applies the grid transform, cleans/triangulates the output, and computes point normals for rendering.
 *
 * @param grid Scalar volume in i-fastest order
 * @param isoValue Scalar value to extract
 * @param options Backend runtime options
 * @return Triangle mesh, or empty when no surface can be extracted
 */
std::optional<MeshData>
generateIsoSurfaceMesh(const ScalarGrid3D& grid, double isoValue, const MeshGenerationOptions& options = {});

/**
 * @brief Generate a label-boundary mesh from a scalar segmentation volume using VTK Discrete Flying Edges 3D
 *
 * Callers adapting integer image labels should first create an exact binary mask with
 * `labelMaskGridFromImageComponent`; `ScalarGrid3D` itself stores floating-point samples.
 *
 * @param grid Scalar label volume in i-fastest order
 * @param labelValue Label value to extract
 * @param options Backend runtime options
 * @return Triangle mesh, or empty when no surface can be extracted
 */
std::optional<MeshData>
generateDiscreteLabelSurface(const ScalarGrid3D& grid, int64_t labelValue, const MeshGenerationOptions& options = {});

/** Generate the surface of a binary zero/one mask without exposing an ambiguous label-value parameter. */
std::optional<MeshData> generateBinaryMaskSurface(const ScalarGrid3D& grid, const MeshGenerationOptions& options = {});

/** Generate the canonical two-ended cylinder-and-cone mesh used for one 3D crosshair axis. */
std::optional<MeshData> generateCrosshairsAxisMesh(double coneLengthRatio = 0.15);

} // namespace rendering::mesh
