#pragma once

#include <uuid.h>

class AppData;
class PlanarPolygon;

/**
 * @brief Triangulate a polygon using the Earcut algorithm. This algorithm can triangulate a simple,
 * planar polygon of any winding order that includes holes. It returns a robust, acceptable solution
 * for non-simple poygons. Earcut works on a 2D plane.
 *
 * @see https://github.com/mapbox/earcut.hpp
 *
 * @param[in,out] polygon Polygon to triangulate.
 */
void triangulatePolygon(PlanarPolygon& polygon);
