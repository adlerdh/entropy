#ifndef POINT_RECORD_H
#define POINT_RECORD_H

#include "common/UuidUtility.h"

#include <glm/vec3.hpp>
#include <string>
#include <uuid.h>

/**
 * @brief Record for a point that represents a position in space.
 * The point has a unique ID.
 *
 * @tparam PositionType Position type
 */
template<class PositionType>
class PointRecord
{
public:
  /// Construct with a point from a position with an automatically generated unique ID
  explicit PointRecord(PositionType position, std::string name = "")
    : m_uid(generateRandomUuid())
    , m_point(std::move(position))
    , m_name(std::move(name))
    , m_description("")
    , m_visibility(true)
    , m_color(glm::vec3{0.5f, 0.5f, 0.5f})
  {
  }

  ~PointRecord() = default;

  /// Get the point's UID
  const uuids::uuid& uid() const { return m_uid; }

  /// Set position of the point
  void setPosition(PositionType position) { m_point = std::move(position); }

  /// Get the point's position
  const PositionType& getPosition() const { return m_point; }

  /// Set the point name
  void setName(std::string name) { m_name = std::move(name); }

  /// Get the point name
  const std::string& getName() const { return m_name; }

  /// Set the point description
  void setDescription(std::string desc) { m_description = std::move(desc); }

  /// Get the point description
  const std::string& getDescription() const { return m_description; }

  /// Set the point color
  void setColor(glm::vec3 color) { m_color = std::move(color); }

  /// Get the point color
  const glm::vec3& getColor() const { return m_color; }

  /// Set visibility of the point
  void setVisibility(bool visibility) { m_visibility = std::move(visibility); }

  /// Get the point's visibility
  bool getVisibility() const { return m_visibility; }

private:
  uuids::uuid m_uid;         //!< Unique ID
  PositionType m_point;      //!< Point position
  std::string m_name;        //!< Name
  std::string m_description; //!< Description
  bool m_visibility;         //!< Visibility
  glm::vec3 m_color;         //!< RGB color
};

#endif // POINT_RECORD_H
