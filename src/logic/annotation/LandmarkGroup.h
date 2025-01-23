#ifndef LANDMARK_GROUP_H
#define LANDMARK_GROUP_H

#include "common/filesystem.h"
#include "logic/annotation/PointRecord.h"

#include <glm/vec3.hpp>

#include <map>
#include <optional>
#include <string>

/**
 * @brief Represents a grouping of landmark points
 */
class LandmarkGroup
{
public:
  /// Type of position represented by landmark points
  using PositionType = glm::vec3;

  /// Construct an empty landmark group
  explicit LandmarkGroup();

  ~LandmarkGroup() = default;

  /// Set/get the file name of the file from which landmarks were loaded
  void setFileName(const fs::path& fileName);
  const fs::path& getFileName() const;

  /// Set/get the group name
  void setName(std::string name);
  const std::string& getName() const;

  /// Set/get the points in the landmark group.
  /// Each point is keyed by an index that specifies its order.
  void setPoints(std::map<size_t, PointRecord<PositionType> > pointMap);
  const std::map<size_t, PointRecord<PositionType> >& getPoints() const;
  std::map<size_t, PointRecord<PositionType> >& getPoints();

  /// Add a new point to the landmark group.
  /// The new point's index is one greater than the largest existing index in the group.
  /// Return the index
  size_t addPoint(PointRecord<PositionType> point);

  /// Add a new point to the landmark group with the given index.
  void addPoint(size_t index, PointRecord<PositionType> point);

  /// Remove the point at a given index from the landmark group
  bool removePoint(size_t index);

  /// Get the maximum landmark index in the landmark group
  size_t maxIndex() const;

  /// Set/get whether the landmarks are in Voxel space (true) or Subject space (false)
  /// @todo Create enum: LandmarkSpace{ ImageVoxels, ImagePhysicalSubject }
  void setInVoxelSpace(bool);
  bool getInVoxelSpace() const;

  /// Set/get the landmark visibility
  void setVisibility(bool visibility);
  bool getVisibility() const;

  /// Set/get the landmark group opacity in range [0.0, 1.0]
  void setOpacity(float opacity);
  float getOpacity() const;

  /// Set/get the landmark group color (non-premultiplied RGB)
  void setColor(glm::vec3 color);
  const glm::vec3& getColor() const;

  /// Set/get whether the landmark group color overrides the landmark color
  void setColorOverride(bool set);
  bool getColorOverride() const;

  /// Set/get the landmark group text color (non-premultiplied RGB)
  void setTextColor(std::optional<glm::vec3> color);
  const std::optional<glm::vec3>& getTextColor() const;

  /// Set/get whether to render indices for the landmarks in the group
  void setRenderLandmarkIndices(bool render);
  bool getRenderLandmarkIndices() const;

  /// Set/get whether to render names for the landmarks in the group
  void setRenderLandmarkNames(bool render);
  bool getRenderLandmarkNames() const;

  /// Set/get the circle radius factor for landmarks in the group
  void setRadiusFactor(float factor);
  float getRadiusFactor() const;

  /// Get the landmark group layer, with 0 being the backmost layer and layers increasing in value
  /// closer towards the viewer
  uint32_t getLayer() const;

  /// Get the maximum landmark group layer
  uint32_t getMaxLayer() const;

private:
  /// Name of the CSV file with the landmarks
  fs::path m_fileName;

  /// Name of landmark group
  std::string m_name;

  /// Map of landmark points. Each landmark point is keyed by an index
  /// that specifies its order.
  std::map<size_t, PointRecord<PositionType> > m_pointMap;

  /// Are the landmark points defined in Voxel (true) or Subject space?
  bool m_inVoxelSpace;

  /// Internal layer of the landmark group: 0 is the backmost layer and higher layers are more frontwards.
  uint32_t m_layer;

  /// The maximum layer among all landmark groups
  uint32_t m_maxLayer;

  /// Visibility of the landmark group
  bool m_visibility;

  /// Opacity of the landmark group, in [0.0, 1.0] range
  float m_opacity;

  /// Color of the landmark group (non-premultiplied RGB triple).
  /// When non-null, this color overrides the individual landmark colors
  glm::vec3 m_color;

  bool m_colorOverride;

  /// Color of the landmark text (non-premultiplied RGB triple).
  /// When non-null, this color overrides the individual landmark colors
  std::optional<glm::vec3> m_textColor;

  /// Flag to render the landmark indices
  bool m_renderLandmarkIndices;

  /// Flag to render the landmark names
  bool m_renderLandmarkNames;

  /// Landmark radius as a multiple of the view size
  float m_landmarkRadiusFactor;

  /// Set the landmark group layer, with 0 being the backmost layer.
  /// @note Use the function \c changeLandmarkGroupLayering to change layer
  void setLayer(uint32_t layer);

  /// Set the maximum landmark group layer.
  /// @note Set using the function \c changeLandmarkGroupLayering
  void setMaxLayer(uint32_t maxLayer);
};

#endif // LANDMARK_GROUP_H
