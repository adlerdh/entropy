#include "logic_old/annotation/PlanarPolygon.h"

#include "common/Exception.hpp"

#include <glm/glm.hpp>

PlanarPolygon::PlanarPolygon()
  : m_vertices()
  , m_triangulation()
  , m_currentUid()
  , m_aabb(std::nullopt)
{
}

void PlanarPolygon::setAllVertices(std::vector<std::vector<PointType> > vertices)
{
  m_vertices = std::move(vertices);
  m_triangulation.clear();
  m_currentUid = uuids::uuid();

  computeAABBox();
}

const std::vector<std::vector<PlanarPolygon::PointType> >& PlanarPolygon::getAllVertices() const
{
  return m_vertices;
}

void PlanarPolygon::setBoundaryVertices(size_t boundary, std::vector<PointType> vertices)
{
  m_vertices.at(boundary) = std::move(vertices);
  m_triangulation.clear();
  m_currentUid = uuids::uuid();

  if (0 == boundary)
  {
    computeAABBox();
  }
}

void PlanarPolygon::setOuterBoundary(std::vector<PointType> vertices)
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
  m_currentUid = uuids::uuid();

  computeAABBox();
}

void PlanarPolygon::addHole(std::vector<PointType> vertices)
{
  if (m_vertices.size() >= 1)
  {
    m_vertices.emplace_back(std::move(vertices));
  }

  m_triangulation.clear();
  m_currentUid = uuids::uuid();
}

const std::vector<PlanarPolygon::PointType>& PlanarPolygon::getBoundaryVertices(size_t boundary
) const
{
  return m_vertices.at(boundary);
}

size_t PlanarPolygon::numBoundaries() const
{
  return m_vertices.size();
}

size_t PlanarPolygon::numVertices() const
{
  size_t N = 0;

  for (const auto& boundary : m_vertices)
  {
    N += boundary.size();
  }

  return N;
}

const PlanarPolygon::PointType& PlanarPolygon::getBoundaryVertex(size_t boundary, size_t i) const
{
  return m_vertices.at(boundary).at(i);
}

const PlanarPolygon::PointType& PlanarPolygon::getVertex(size_t i) const
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

  throw_debug("Invalid vertex");
}

void PlanarPolygon::setTriangulation(std::vector<IndexType> indices)
{
  m_triangulation = std::move(indices);
  m_currentUid = uuids::uuid();
}

bool PlanarPolygon::hasTriangulation() const
{
  return (!m_triangulation.empty());
}

const std::vector<PlanarPolygon::IndexType>& PlanarPolygon::getTriangulation() const
{
  return m_triangulation;
}

std::tuple<PlanarPolygon::IndexType, PlanarPolygon::IndexType, PlanarPolygon::IndexType>
PlanarPolygon::getTriangle(size_t i) const
{
  return std::make_tuple(
    m_triangulation.at(3 * i + 0), m_triangulation.at(3 * i + 1), m_triangulation.at(3 * i + 2)
  );
}

std::optional<PlanarPolygon::AABBoxType> PlanarPolygon::getAABBox() const
{
  return m_aabb;
}

size_t PlanarPolygon::numTriangles() const
{
  // Every three indices make a triangle
  return m_triangulation.size() / 3;
}

uuids::uuid PlanarPolygon::getCurrentUid() const
{
  return m_currentUid;
}

bool PlanarPolygon::equals(const uuids::uuid& otherPlanarPolygonUid) const
{
  return (m_currentUid == otherPlanarPolygonUid);
}

void PlanarPolygon::computeAABBox()
{
  if (m_vertices.empty() || m_vertices[0].empty())
  {
    // There is no outer boundary or there are no vertices in the outer boundary.
    m_aabb = std::nullopt;
    return;
  }

  // Compute AABB of outer boundary vertices
  m_aabb = std::make_pair(
    PointType(std::numeric_limits<ComponentType>::max()),
    PointType(std::numeric_limits<ComponentType>::lowest())
  );

  for (const auto& v : m_vertices[0])
  {
    m_aabb->first = glm::min(m_aabb->first, v);
    m_aabb->second = glm::max(m_aabb->second, v);
  }
}
