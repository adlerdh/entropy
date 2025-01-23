#include "logic/annotation/AnnotationHelper.h"
#include "logic/annotation/AnnotationGroup.h"
#include "logic/annotation/PlanarPolygon.h"

#include "logic/AppData.h"

#include <mapbox/earcut.hpp>

#include <glm/glm.hpp>

#include <algorithm>
#include <iostream>
#include <list>
#include <utility>

namespace mapbox
{
namespace util
{

template<>
struct nth<0, PlanarPolygon::PointType>
{
  inline static auto get(const PlanarPolygon::PointType& point) { return point[0]; }
};

template<>
struct nth<1, Polygon::PointType>
{
  inline static auto get(const PlanarPolygon::PointType& point) { return point[1]; }
};

} // namespace util
} // namespace mapbox

void triangulatePolygon(PlanarPolygon& polygon)
{
  polygon.setTriangulation(mapbox::earcut<PlanarPolygon::IndexType>(polygon.getAllVertices()));
}
