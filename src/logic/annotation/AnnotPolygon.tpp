#ifndef POLYGON_TPP
#define POLYGON_TPP

#include "common/AABB.h"
#include "logic/annotation/BezierHelper.h"

#include <spdlog/spdlog.h>

#include <glm/glm.hpp>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/string_cast.hpp>

#include <limits>
#include <optional>
#include <tuple>
#include <vector>

/**
 * @brief A polygon of any winding order that can have multiple holes inside of an outer boundary.
 * The planarity of the polygon is not enforced: that is the reponsibility of the user.
 *
 * @note The polygon's outer boundary can be either open or closed. This property is not specified
 * in this class: It is left up to the user of this class to decide whether the boundary is closed or open.
 * By definition, all holes must be closed boundaries.
 *
 * @note The polygon can have a triangulation that uses only its original vertices.
 *
 * @tparam TComp Vertex component type
 * @tparam Dim Vertex dimension
 */
template<typename TComp, uint32_t Dim>
class AnnotPolygon
{
public:
  /// Vertex point type
  using PointType = glm::vec<Dim, TComp, glm::highp>;

  /// Axis-aligned bounding box type
  using AABBoxType = AABB_N<Dim, TComp>;

  /// Construct empty polygon with no triangulation
  explicit AnnotPolygon()
    : m_vertices()
    , m_bezierCommands()
    , m_closed(false)
    , m_smoothed(false)
    , m_smoothingFactor(0.1f)
    , m_triangulation()
    , m_aabb(std::nullopt)
    , m_centroid(0)
  {
  }

  ~AnnotPolygon() = default;

  /// Set all vertices of the polygon. The first list defines the main (outer) polygon boundary;
  /// subsequent lists define boundaries of holes within the outer boundary.
  void setAllVertices(std::vector<std::vector<PointType> > vertices)
  {
    m_vertices = std::move(vertices);
    m_triangulation.clear();

    computeAABBox();
    computeCentroid();
    computeBezier();
  }

  /// Get all vertices from all boundaries. The first list contains vertices of the outer boundary;
  /// subsequent lists contain vertices of holes.
  const std::vector<std::vector<PointType> >& getAllVertices() const { return m_vertices; }

  void setClosed(bool closed)
  {
    m_closed = closed;
    computeBezier();
  }

  bool isClosed() const { return m_closed; }

  void setSmoothed(bool smoothed)
  {
    m_smoothed = smoothed;
    computeBezier();
  }

  bool isSmoothed() const { return m_smoothed; }

  void setSmoothingFactor(float factor)
  {
    m_smoothingFactor = factor;
    computeBezier();
  }

  float getSmoothingFactor() const { return m_smoothingFactor; }

  const std::vector<std::tuple<glm::vec2, glm::vec2, glm::vec2> >& getBezierCommands() const
  {
    return m_bezierCommands;
  }

  /// Set vertices for a given boundary, where 0 refers to the outer boundary;
  /// boundaries >= 1 are for holes.
  bool setBoundaryVertices(size_t boundary, std::vector<PointType> vertices)
  {
    if (boundary >= m_vertices.size())
    {
      spdlog::warn("Invalid polygon boundary index {}", boundary);
      return false;
    }

    m_vertices.at(boundary) = std::move(vertices);
    m_triangulation.clear();

    if (0 == boundary)
    {
      computeAABBox();
      computeCentroid();
      computeBezier();
    }

    return true;
  }

  /// Set a new vertex for a given boundary, where 0 refers to the outer boundary;
  /// boundaries >= 1 are for holes.
  bool setBoundaryVertex(size_t boundary, size_t vertexIndex, const PointType& vertex)
  {
    if (boundary >= m_vertices.size())
    {
      spdlog::warn("Invalid polygon boundary index {}", boundary);
      return false;
    }

    std::vector<PointType>& boundaryVertices = m_vertices.at(boundary);

    if (vertexIndex >= boundaryVertices.size())
    {
      spdlog::warn("Invalid vertex index {} to set for polygon boundary", vertexIndex, boundary);
      return false;
    }

    boundaryVertices[vertexIndex] = vertex;

    m_triangulation.clear();

    if (0 == boundary)
    {
      computeAABBox();
      computeCentroid();
      computeBezier();
    }

    return true;
  }

  /// Add a vertex to a given boundary, where 0 refers to the outer boundary;
  /// boundaries >= 1 are for holes
  bool addVertexToBoundary(size_t boundary, const PointType& vertex)
  {
    if (boundary >= m_vertices.size())
    {
      if (0 == boundary)
      {
        // Allow adding the outer boundary:
        m_vertices.emplace_back(std::vector<PointType>{vertex});
        spdlog::info("Added new polygon boundary with index {}", boundary);
      }
      else
      {
        spdlog::warn(
          "Unable to add vertex {} to invalid boundary {}", glm::to_string(vertex), boundary
        );
        return false;
      }
    }
    else
    {
      m_vertices[boundary].emplace_back(vertex);
    }

    m_triangulation.clear();

    if (0 == boundary)
    {
      computeAABBox();
      updateCentroid();
      computeBezier();
    }

    return true;
  }

  /// Insert a vertex into a given boundary after a given vertex, where 0 refers to the outer boundary;
  /// boundaries >= 1 are for holes.
  bool insertVertexIntoBoundary(size_t boundary, size_t vertexIndex, const PointType& vertex)
  {
    if (boundary >= m_vertices.size())
    {
      spdlog::error(
        "Unable to insert vertex {} into invalid boundary {}", glm::to_string(vertex), boundary
      );
      return false;
    }
    else if (vertexIndex < m_vertices[boundary].size())
    {
      m_vertices[boundary].insert(std::begin(m_vertices[boundary]) + vertexIndex, vertex);
    }

    m_triangulation.clear();

    if (0 == boundary)
    {
      computeAABBox();
      computeCentroid();
      computeBezier();
    }

    return true;
  }

  /// Set the vertices of the outer boundary only.
  void setOuterBoundary(std::vector<PointType> vertices)
  {
    if (m_vertices.size() >= 1)
    {
      m_vertices[0] = std::move(vertices);
    }
    else
    {
      m_vertices.emplace_back(std::move(vertices));
    }

    m_triangulation.clear();

    computeAABBox();
    computeCentroid();
    computeBezier();
  }

  /// Add a vertex to the outer boundary
  void addVertexToOuterBoundary(PointType vertex)
  {
    if (m_vertices.size() >= 1)
    {
      m_vertices[0].emplace_back(std::move(vertex));
    }
    else
    {
      m_vertices.emplace_back(std::vector<PointType>{vertex});
    }

    m_triangulation.clear();

    computeAABBox();
    updateCentroid();
    computeBezier();
  }

  /// Remove a vertex from a boundary
  bool removeVertexFromBoundary(size_t boundary, size_t vertexIndex)
  {
    if (boundary >= m_vertices.size())
    {
      spdlog::warn("Invalid polygon boundary index {}", boundary);
      return false;
    }

    const size_t numVertices = getBoundaryVertices(boundary).size();

    if (1 == numVertices)
    {
      spdlog::warn("Cannot remove the last vertex of a boundary");
      return false;
    }

    if (vertexIndex >= numVertices)
    {
      spdlog::warn("Invalid polygon vertex {}", vertexIndex);
      return false;
    }

    auto iter = std::begin(m_vertices[boundary]);
    std::advance(iter, vertexIndex);

    // Erase the vertex
    m_vertices.at(boundary).erase(iter);

    m_triangulation.clear();

    if (0 == boundary)
    {
      computeAABBox();
      computeCentroid();
      computeBezier();
    }

    return true;
  }

  /// Add a hole to the polygon. The operation only succeeds if the polygon has at least
  /// an outer boundary.
  bool addHole(std::vector<PointType> vertices)
  {
    if (m_vertices.size() >= 1)
    {
      m_vertices.emplace_back(std::move(vertices));
      m_triangulation.clear();
      return true;
    }

    return false;
  }

  /// Get all vertices of a given boundary, where 0 refers to the outer boundary;
  /// boundaries >= 1 are holes.
  /// @return Empty list if invalid boundary
  const std::vector<PointType>& getBoundaryVertices(size_t boundary) const
  {
    static const std::vector<AnnotPolygon::PointType> sk_emptyBoundary;

    if (boundary >= m_vertices.size())
    {
      spdlog::warn("Invalid polygon boundary index {}", boundary);
      return sk_emptyBoundary;
    }

    return m_vertices.at(boundary);
  }

  /// Get the number of boundaries in the polygon, including the outer boundary and all holes.
  size_t numBoundaries() const { return m_vertices.size(); }

  /// Get the total number of vertices among all boundaries, including the outer boundary and holes.
  size_t numVertices() const
  {
    size_t N = 0;

    for (const auto& boundary : m_vertices)
    {
      N += boundary.size();
    }

    return N;
  }

  /// Get the i'th vertex of a given boundary, where 0 is the outer boundary and subsequent boundaries
  /// define holes.
  /// @return Null optional if invalid boundary or vertex index
  std::optional<AnnotPolygon::PointType> getBoundaryVertex(size_t boundary, size_t i) const
  {
    if (boundary >= m_vertices.size())
    {
      spdlog::warn("Invalid polygon boundary index {}", boundary);
      return std::nullopt;
    }

    const auto& vertices = m_vertices.at(boundary);

    if (i >= vertices.size())
    {
      spdlog::warn("Invalid vertex index {} for polygon boundary {}", i, boundary);
      return std::nullopt;
    }

    return vertices[i];
  }

  /// Get i'th vertex of the whole polygon. Here i indexes the collection of all ordered vertices
  /// of the outer boundary and all hole boundaries.
  /// @return Null optional if invalid vertex
  std::optional<AnnotPolygon::PointType> getVertex(size_t i) const
  {
    size_t j = i;

    for (const auto& boundary : m_vertices)
    {
      if (j < boundary.size())
      {
        return boundary[j];
      }
      else
      {
        j -= boundary.size();
      }
    }

    spdlog::warn("Invalid polygon vertex {}", i);
    return std::nullopt;
  }

  /// Get the axis-aligned bounding box of the polygon.
  /// @returns Null optional if the polygon is empty
  std::optional<AABBoxType> getAABBox() const { return m_aabb; }

  /// Get the centroid of the polygon's outer boundary.
  /// @returns Origin if the outer boundary has no points.
  const PointType& getCentroid() const { return m_centroid; }

  /// Set the triangulation from a vector of indices that refer to vertices of the whole polygon.
  /// Every three consecutive indices form a triangle and triangles must be clockwise.
  void setTriangulation(std::vector<size_t> indices) { m_triangulation = std::move(indices); }

  /// Return true iff the polygon has a valid triangulation.
  bool hasTriangulation() const { return (!m_triangulation.empty()); }

  /// Get the polygon triangulation: a vector of indices refering to vertices of the whole polygon.
  const std::vector<size_t>& getTriangulation() const { return m_triangulation; }

  /// Get indices of the i'th triangle. The triangle is oriented clockwise.
  std::optional<std::tuple<size_t, size_t, size_t> > getTriangle(size_t i) const
  {
    if (3 * i + 2 >= m_triangulation.size())
    {
      spdlog::warn("Invalid triangle index {}", i);
      return std::nullopt;
    }

    return std::make_tuple(
      m_triangulation.at(3 * i + 0), m_triangulation.at(3 * i + 1), m_triangulation.at(3 * i + 2)
    );
  }

  /// Get the number of triangles in the polygon triangulation.
  size_t numTriangles() const
  {
    // Every three indices make a triangle
    return m_triangulation.size() / 3;
  }

private:
  /// Compute the 2D AABB of the outer polygon boundary, if it exists.
  void computeAABBox()
  {
    if (m_vertices.empty() || m_vertices[0].empty())
    {
      // There is no outer boundary or there are no vertices in the outer boundary.
      m_aabb = std::nullopt;
      return;
    }

    // Compute AABB of outer boundary vertices
    m_aabb = std::make_pair(
      PointType(std::numeric_limits<TComp>::max()), PointType(std::numeric_limits<TComp>::lowest())
    );

    for (const auto& v : m_vertices[0])
    {
      m_aabb->first = glm::min(m_aabb->first, v);
      m_aabb->second = glm::max(m_aabb->second, v);
    }
  }

  /// Update the centroid of the outer boundary with a new point.
  /// Call this function AFTER adding the new point to the boundary.
  void updateCentroid()
  {
    if (m_vertices.empty())
    {
      // No outer boundary
      m_centroid = PointType{0};
      return;
    }

    const auto& outerBoundary = m_vertices[0];
    const size_t N = outerBoundary.size();

    if (0 == N)
    {
      m_centroid = PointType{0};
      return;
    }
    else if (1 == N)
    {
      m_centroid = outerBoundary.front();
      return;
    }

    m_centroid += (outerBoundary.back() - m_centroid) / static_cast<TComp>(N);
  }

  /// Compute the centroid of the outer boundary from scratch
  void computeCentroid()
  {
    m_centroid = PointType{0};

    if (m_vertices.empty())
    {
      // No outer boundary
      return;
    }

    const auto& outerBoundary = m_vertices[0];
    const size_t N = outerBoundary.size();

    if (0 == N)
    {
      return;
    }

    for (const auto& p : outerBoundary)
    {
      m_centroid += p;
    }

    m_centroid /= static_cast<TComp>(N);
    ;
  }

  /// Compute the bezier commands for the outer boundary.
  /// Only applies to 2D polygons.
  void computeBezier()
  {
    if (2 == Dim && !m_vertices.empty() && m_smoothed)
    {
      m_bezierCommands = computeBezierCommands(m_vertices[0], m_smoothingFactor, m_closed);
    }
  }

  /// Polygon stored as vector of vectors of points. The first vector defines the outer polygon
  /// boundary; subsequent vectors define holes in the main polygon. Any winding order for the
  /// outer boundary and holes is valid.
  std::vector<std::vector<PointType> > m_vertices;

  /// Bezier commands for the outer boundary. Only updated if \c m_smoothingFactor > 0
  std::vector<std::tuple<glm::vec2, glm::vec2, glm::vec2> > m_bezierCommands;

  bool m_closed;           //!< Is the outer boundary closed?
  bool m_smoothed;         //!< Flag to smooth the outer boundary curve
  float m_smoothingFactor; //!< Bezier smoothing factor

  /// Vector of indices that refer to the vertices of the input polygon.
  /// Three consecutive indices form a clockwise triangle.
  std::vector<size_t> m_triangulation;

  /// Axis-aligned bounding box of the polygon; set to none if the polygon is empty.
  std::optional<AABBoxType> m_aabb;

  /// Centroid of the polygon's outer boundary. Set to origin if the outer boundary is empty.
  PointType m_centroid;
};

#endif // POLYGON_TPP
