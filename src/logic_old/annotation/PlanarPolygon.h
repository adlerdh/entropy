#ifndef PLANAR_POLYGON_H
#define PLANAR_POLYGON_H

#include "common/AABB.h"

#include <glm/vec2.hpp>

#include <uuid.h>

#include <optional>
#include <tuple>
#include <vector>

/**
 * @brief A planar, closed polygon of any winding order that can have holes. Each vertex is 2D.
 * The polygon can have a triangulation that uses only its original vertices.
 */
class PlanarPolygon
{
public:
  /// Vertex component type
  using ComponentType = float;

  /// Vertex point type (use GLM)
  using PointType = glm::vec<2, ComponentType, glm::highp>;

  /// Vertex index type, used for defining triangles by indexing vertices
  using IndexType = uint32_t;

  /// Axis-aligned bounding box type (2D bounding box)
  using AABBoxType = AABB_N<2, ComponentType>;

  /// Construct empty polygon with no triangulation
  explicit PlanarPolygon();

  ~PlanarPolygon() = default;

  /// Set all vertices of the polygon. The first vector defines the main (outer) polygon boundary;
  /// subsequent vectors define boundaries of holes within the outer boundary.
  void setAllVertices(std::vector<std::vector<PointType> > vertices);

  /// Get all vertices from all boundaries. The first vector contains vertices of the outer boundary;
  /// subsequent vectors contain vertices of holes.
  const std::vector<std::vector<PointType> >& getAllVertices() const;

  /// Set vertices for a given boundary, where 0 refers to the outer boundary; subsequent boundaries
  /// are holes.
  /// @throws For an invalid boundary
  void setBoundaryVertices(size_t boundary, std::vector<PointType> vertices);

  /// Set the vertices of the outer boundary only.
  void setOuterBoundary(std::vector<PointType> vertices);

  /// Add a hole to the polygon. The operation only succeeds if the polygon has at least
  /// an outer boundary.
  void addHole(std::vector<PointType> vertices);

  /// Get all vertices of a given boundary, where 0 refers to the outer boundary; subsequent boundaries
  /// are holes.
  /// @throws For an invalid boundary
  const std::vector<PointType>& getBoundaryVertices(size_t boundary) const;

  /// Get the number of boundaries in the polygon, including the outer boundary and all holes.
  size_t numBoundaries() const;

  /// Get the total number of vertices among all boundaries, including the outer boundary and holes.
  size_t numVertices() const;

  /// Get the i'th vertex of a given boundary, where 0 is the outer boundary and subsequent bounaries
  /// define holes.
  /// @throws For an invalid boundary
  const PointType& getBoundaryVertex(size_t boundary, size_t i) const;

  /// Get i'th vertex of the whole polygon. Here i indexes the collection of all ordered vertices
  /// of the outer boundary and all hole boundaries.
  /// @throws Invalid index
  const PointType& getVertex(size_t i) const;

  /// Get the 2D axis-aligned bounding box of the polygon.
  /// @returns std::nullopt if the polygon is empty
  std::optional<AABBoxType> getAABBox() const;

  /// Set the triangulation from a vector of indices that refer to vertices of the whole polygon.
  /// Every three consecutive indices form a triangle and triangles must be clockwise.
  void setTriangulation(std::vector<IndexType> indices);

  /// Return true iff the polygon has a valid triangulation.
  bool hasTriangulation() const;

  /// Get the polygon triangulation: a vector of indices refering to vertices of the whole polygon.
  const std::vector<IndexType>& getTriangulation() const;

  /// Get indices of the i'th triangle. The triangle is oriented clockwise.
  std::tuple<IndexType, IndexType, IndexType> getTriangle(size_t i) const;

  /// Get the number of triangles in the polygon triangulation.
  size_t numTriangles() const;

  /// Get the unique ID that is re-generated every time anything changes for this polygon,
  /// including vertices and triangulation.
  uuids::uuid getCurrentUid() const;

  /// Return true iff this polygon equals (in terms of both vertices and triangulation)
  /// another polygon. The comparison is done based on unique IDs of the polygons.
  bool equals(const uuids::uuid& otherPolygonUid) const;

private:
  /// Compute the 2D AABB of the outer polygon boundary, if it exists.
  void computeAABBox();

  /// Polygon stored as vector of vectors of points. The first vector defines the outer polygon
  /// boundary; subsequent vectors define holes in the main polygon. Any winding order for the
  /// outer boundary and holes is valid.
  std::vector<std::vector<PointType> > m_vertices;

  /// Vector of indices that refer to the vertices of the input polygon. Three consecutive indices
  /// form a clockwise triangle.
  std::vector<IndexType> m_triangulation;

  /// A unique ID that is re-generated every time anything changes for this polygon,
  /// including vertices and triangulation.
  uuids::uuid m_currentUid;

  /// 2D axis-aligned bounding box of the polygon; set to none if the polygon is empty.
  std::optional<AABBoxType> m_aabb;
};

#endif // PLANAR_POLYGON_H
