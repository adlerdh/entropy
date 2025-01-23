#ifndef SLIDE_ANNOTATION_CPU_RECORD_H
#define SLIDE_ANNOTATION_CPU_RECORD_H

#include <glm/vec3.hpp>
#include <uuid.h>

#include <memory>

class DataManager;
class PlanarPolygon;

enum class LayerChangeType;

/**
 * @brief Record for CPU storage of a slide annotation, which is a closed,
 * planar polygon with vertices defined in normalized slide coordinates [0.0, 1.0]^2.
 */
class SlideAnnotationCpuRecord
{
  /// Friend helper function that sets the layer depth of annotations
  friend void setUniqueSlideAnnotationLayers(DataManager&);

  /// Friend helper function that changes the layer depth of annotations
  friend void changeSlideAnnotationLayering(DataManager&, const uuids::uuid&, const LayerChangeType&);

public:
  /// Construct annotation with no polygon
  explicit SlideAnnotationCpuRecord();

  /// Construct annotation with a polygon
  explicit SlideAnnotationCpuRecord(std::unique_ptr<PlanarPolygon>);

  ~SlideAnnotationCpuRecord() = default;

  /// Set the annotation's polygon
  void setPolygon(std::unique_ptr<PlanarPolygon>);

  /// Get non-const access to the annotation's polygon
  PlanarPolygon* polygon();

  /// Get the annotation layer, with 0 being the backmost layer and layers increasing in value
  /// closer towards the viewer
  uint32_t getLayer() const;

  /// Get the maximum annotation layer
  uint32_t getMaxLayer() const;

  /// Set the annotation opacity in range [0.0, 1.0]
  void setOpacity(float opacity);

  /// Get the annotation opacity
  float getOpacity() const;

  /// Set the annotation color (non-premultiplied RGB)
  void setColor(glm::vec3 color);

  /// Get the annotation color (non-premultiplied RGB)
  const glm::vec3& getColor() const;

private:
  /// Annotation polygon, which can include holes
  std::unique_ptr<PlanarPolygon> m_polygon;

  /// Internal layer of the annotation: 0 is the backmost layer and higher layers are more
  /// frontwards.
  uint32_t m_layer;

  /// The maximum layer among all annotations for a given slide.
  uint32_t m_maxLayer;

  /// Annotation opacity in [0.0, 1.0] range
  float m_opacity;

  /// Annotation color (non-premultiplied RGB triple)
  glm::vec3 m_color;

  /// Set the annotation layer, with 0 being the backmost layer.
  /// @note Use the function \c changeSlideAnnotationLayering to change annotation layer
  void setLayer(uint32_t layer);

  /// Set the maximum annotation layer.
  /// @note Set using the function \c changeSlideAnnotationLayering
  void setMaxLayer(uint32_t maxLayer);
};

#endif // SLIDE_ANNOTATION_CPU_RECORD_H
