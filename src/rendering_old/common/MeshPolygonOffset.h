#ifndef MESH_POLYGON_OFFSET_H
#define MESH_POLYGON_OFFSET_H

#include <utility>

/// @todo Remove namespace and put in struct?

namespace PolygonOffset
{

/// Polygon offsets are pairs: { factor, units }
/// @see OpenGL specification

extern const std::pair<float, float> annotations;
extern const std::pair<float, float> crosshairs;
extern const std::pair<float, float> imageSlices;
extern const std::pair<float, float> landmarks;
extern const std::pair<float, float> slideSlices;

} // namespace PolygonOffset

#endif // MESH_POLYGON_OFFSET_H
