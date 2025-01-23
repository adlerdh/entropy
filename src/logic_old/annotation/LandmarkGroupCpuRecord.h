#ifndef LANDMARK_GROUP_CPU_RECORD_H
#define LANDMARK_GROUP_CPU_RECORD_H

#include "logic/annotation/PointList.h"
#include "logic/annotation/PointRecord.h"

#include <glm/vec3.hpp>

#include <string>

class LandmarkGroupCpuRecord
{
public:
  /// Type of position represented by landmark points
  using PositionType = glm::vec3;

  explicit LandmarkGroupCpuRecord();

  ~LandmarkGroupCpuRecord() = default;

  /// Set the group name
  void setName(std::string name);

  /// Get the group name
  const std::string& getName() const;

  /// Set the orderd list of points in the landmark group
  void setPoints(PointList<PointRecord<PositionType> > pointList);

  /// Get a non-const reference to the list of points in the landmark group
  const PointList<PointRecord<PositionType> >& getPoints() const;

  /// Get the landmark group layer, with 0 being the backmost layer and layers increasing in value
  /// closer towards the viewer
  uint32_t getLayer() const;

  /// Get the maximum landmark group layer
  uint32_t getMaxLayer() const;

  /// Set the landmark visibility
  void setVisibility(bool visibility);

  /// Get the landmark group opacity
  bool getVisibility() const;

  /// Set the landmark group opacity in range [0.0, 1.0]
  void setOpacity(float opacity);

  /// Get the landmark group opacity
  float getOpacity() const;

  /// Set the landmark group color (non-premultiplied RGB)
  void setColor(glm::vec3 color);

  /// Get the landmark group color (non-premultiplied RGB)
  const glm::vec3& getColor() const;

private:
  /// Name of landmark group
  std::string m_name;

  /// Ordered list of landmark points
  PointList<PointRecord<PositionType> > m_pointList;

  /// Internal layer of the landmark group: 0 is the backmost layer and higher layers are more frontwards.
  uint32_t m_layer;

  /// The maximum layer among all landmark groups
  uint32_t m_maxLayer;

  /// Visibility
  bool m_visibility;

  /// Landmark opacity in [0.0, 1.0] range
  float m_opacity;

  /// Landmark color (non-premultiplied RGB triple)
  glm::vec3 m_color;

  /// Set the landmark group layer, with 0 being the backmost layer.
  /// @note Use the function \c changeLandmarkGroupLayering to change layer
  void setLayer(uint32_t layer);

  /// Set the maximum landmark group layer.
  /// @note Set using the function \c changeLandmarkGroupLayering
  void setMaxLayer(uint32_t maxLayer);
};

#endif // LANDMARK_GROUP_CPU_RECORD_H
