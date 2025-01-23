#ifndef POINT_LIST_H
#define POINT_LIST_H

#include "common/UuidRange.h"
#include "logic/annotation/PointRecord.h"

#include <uuid.h>

#include <algorithm>
#include <list>
#include <optional>

/**
 * @brief A list of points, each of which has a unique ID.
 */
template<class PointType>
class PointList
{
public:
  /// Construct empty list.
  explicit PointList()
    : m_points()
  {
  }

  /// Construct from list of points.
  explicit PointList(std::list<PointType> points)
    : m_points(std::move(points))
  {
  }

  ~PointList() = default;

  /// Get number of points in list.
  size_t numPoints() const { return m_points.size(); }

  /// Set all points in the list, replacing all existing points.
  void setPoints(std::list<PointType> points) { m_points = std::move(points); }

  /// Clear all points in the list.
  void clearPoints() { m_points.clear(); }

  /// Append a point to the end of the list.
  void appendPoint(PointType point) { m_points.emplace_back(std::move(point)); }

  /// Insert a point at the given list index.
  /// @return True iff the point was inserted.
  bool insertPoint(size_t index, PointType point)
  {
    if (m_points.size() < index)
    {
      return false;
    }

    auto it = std::next(std::begin(m_points), index);
    m_points.emplace(it, std::move(point));
    return true;
  }

  /// Insert a point in the list after the point with the given UID.
  /// @return True iff the point was inserted.
  bool insertPoint(const uuids::uuid& pointUid, PointType point)
  {
    if (const auto index = getPointIndex(pointUid))
    {
      return insertPoint(*index, std::move(point));
    }
    return false;
  }

  /// Replace the value of the point at the given index.
  /// @return True iff the point was replaced.
  bool replacePoint(size_t index, PointType point)
  {
    if (m_points.size() <= index)
    {
      return false;
    }

    auto it = std::next(std::begin(m_points), index);
    *it = std::move(point);
    return true;
  }

  /// Replace the value of a point with the given UID.
  /// @return True iff the point was replaced.
  bool replacePoint(const uuids::uuid& pointUid, PointType point)
  {
    if (const auto index = getPointIndex(pointUid))
    {
      return replacePoint(*index, std::move(point));
    }
    return false;
  }

  /// Erase the point at the given index.
  /// @return True iff the point was erased.
  bool erasePoint(size_t index)
  {
    if (m_points.size() <= index)
    {
      return false;
    }

    auto it = std::next(std::begin(m_points), index);
    m_points.erase(it);
    return true;
  }

  /// Erase the point with the given UID.
  /// @return True iff the point was erased.
  bool erasePoint(const uuids::uuid& pointUid)
  {
    if (const auto index = getPointIndex(pointUid))
    {
      return erasePoint(*index);
    }
    return false;
  }

  /// Get the index of the point with the given UID.
  std::optional<size_t> getPointIndex(const uuids::uuid& pointUid) const
  {
    const auto it = findPoint(pointUid);
    if (std::end(m_points) == it)
    {
      return std::nullopt;
    }

    return std::distance(std::begin(m_points), it);
  }

  /// Get the value of the point at the given index.
  std::optional<PointType> getPoint(size_t index) const
  {
    if (m_points.size() <= index)
    {
      return std::nullopt;
    }

    auto it = std::next(std::begin(m_points), index);
    return *it;
  }

  /// Get the value of the point with the given UID.
  std::optional<PointType> getPoint(const uuids::uuid& pointUid) const
  {
    const auto it = findPoint(pointUid);
    if (std::end(m_points) == it)
    {
      return std::nullopt;
    }
    return *it;
  }

  const std::list<PointType>& getPoints() const { return m_points; }

private:
  /// Get an iterator to a point with given UID.
  typename std::list<PointType>::iterator findPoint(const uuids::uuid& pointUid) const
  {
    return std::find_if(
      std::begin(m_points),
      std::end(m_points),
      [&pointUid](const PointType& p) { return (pointUid == p.uid()); }
    );
  }

  /// Points stored as list
  std::list<PointType> m_points;
};

#endif // POINT_LIST_H
