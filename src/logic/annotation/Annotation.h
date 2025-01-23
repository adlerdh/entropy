#ifndef ANNOTATION_H
#define ANNOTATION_H

#include "common/filesystem.h"
#include "logic/annotation/AnnotPolygon.tpp"

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#include <uuid.h>

#include <memory>
#include <set>
#include <string>
#include <tuple>
#include <vector>

class AppData;

/**
 * @brief An image annotation, which (for now) is a planar polygon with vertices
 * defined with 2D coordinates. Note: Each polygon vertex is parameterized in 2D,
 * but it may represent a point in 3D.
 *
 * The annotation plane is defined in the image's Subject coordinate system.
 *
 * @todo Text and regular shape annotations
 */
class Annotation
{
public:
  explicit Annotation(
    std::string displayName, const glm::vec4& color, const glm::vec4& subjectPlaneEquation
  );

  Annotation();

  ~Annotation() = default;

  /// @brief Set/get the annotation display name
  void setDisplayName(std::string displayName);
  const std::string& getDisplayName() const;

  /// @brief Set/get the annotation file name
  void setFileName(fs::path fileName);
  const fs::path& getFileName() const;

  /// @brief Get the annotation's polygon as a const/non-const reference
  AnnotPolygon<float, 2>& polygon();
  const AnnotPolygon<float, 2>& polygon() const;

  const std::vector<std::vector<glm::vec2> >& getAllVertices() const;

  size_t numBoundaries() const;

  const std::vector<glm::vec2>& getBoundaryVertices(size_t boundary) const;

  const std::vector<std::tuple<glm::vec2, glm::vec2, glm::vec2> >& getBezierCommands() const;

  void addPlanePointToBoundary(size_t boundary, const glm::vec2& planePoint);

  /**
     * @brief insertPlanePointIntoBoundary
     * @param boundary
     * @param vertexIndex
     * @param vertex
     * @return True iff the vertex was inserted at position \c vertexIndex
     */
  bool insertPlanePointIntoBoundary(size_t boundary, size_t vertexIndex, const glm::vec2& vertex);

  /**
     * @brief Add a 3D Subject point to the annotation polygon.
     * @param[in] boundary Boundary to add point to
     * @param[in] subjectPoint 3D point in Subject space to project
     * @return Projected point in 2D Subject plane coordinates
     */
  std::optional<glm::vec2> addSubjectPointToBoundary(size_t boundary, const glm::vec3& subjectPoint);

  /// Remove the vertex highlights
  void removeVertexHighlights();

  /// Remove the edge highlights
  void removeEdgeHighlights();

  /// Get the highlighted vertices: pairs of { boundary index, vertex index },
  /// where the vertex index is for the given boundary
  const std::set<std::pair<size_t, size_t> >& highlightedVertices() const;

  /// Get the highlighted edges: { boundary index, {first edge vertex index, second edge vertex index} },
  /// where the vertex indices are for the given boundary.
  const std::set<std::pair<size_t, std::pair<size_t, size_t> > >& highlightedEdges() const;

  /// Add a highlighted vertex: { boundary index, vertex index },
  /// where the vertex index is for the given boundary.
  void setVertexHighlight(const std::pair<size_t, size_t>& vertex, bool highlight);

  /// Add a highlighted edge: pairs of { boundary index, {first edge vertex index, second edge vertex index} },
  /// where the vertex indices are for the given boundary.
  void setEdgeHighlight(const std::pair<size_t, std::pair<size_t, size_t> >& edge, bool highlight);

  /// @brief Set/get the annotation highlighted state
  void setHighlighted(bool highlighted);
  bool isHighlighted() const;

  /// @brief Set/get whether the annotation's outer boundary is closed.
  /// If closed, then it is assumed that the last vertex conntects to the first vertex.
  /// If a polygon boundary is specified as closed,  then it is assumed that its last vertex is
  /// connected by an edge to the first vertex. The user need NOT specify a final vertex that is
  /// identical to the first vertex. For example, a closed triangular polygon can be defined with
  /// exactly three vertices.
  void setClosed(bool closed);
  bool isClosed() const;

  /// @brief Set/get the annotation visibility
  void setVisible(bool visibility);
  bool isVisible() const;

  /// @brief Set/get the vertex visibility
  void setVertexVisibility(bool visibility);
  bool getVertexVisibility() const;

  /// @brief Set/get whether the polygon is smoothed
  void setSmoothed(bool smoothed);
  bool isSmoothed() const;

  /// @brief Set/get the Bezier smoothing factor
  void setSmoothingFactor(float factor);
  float getSmoothingFactor() const;

  /// @brief Set/get the overall annotation opacity in range [0.0, 1.0], which gets modulated
  /// with the color opacities
  void setOpacity(float opacity);
  float getOpacity() const;

  /// @brief Set/get the annotation vertex color (non-premultiplied RGBA)
  void setVertexColor(glm::vec4 color);
  const glm::vec4& getVertexColor() const;

  /// @brief Set/get the annotation line color (non-premultiplied RGBA)
  void setLineColor(glm::vec4 color);
  const glm::vec4& getLineColor() const;

  /// @brief Set/get the annotation line stroke thickness
  void setLineThickness(float thickness);
  float getLineThickness() const;

  /// @brief Set/get whether the annotation interior is filled
  void setFilled(bool filled);
  bool isFilled() const;

  /// @brief Set/get the annotation fill color (non-premultiplied RGB)
  void setFillColor(glm::vec4 color);
  const glm::vec4& getFillColor() const;

  /// Set the axes of the plane in Subject space
  bool setSubjectPlane(const glm::vec4& subjectPlaneEquation);

  /// @brief Get the annotation plane equation in Subject space
  const glm::vec4& getSubjectPlaneEquation() const;

  /// @brief Get the annotation plane origin in Subject space
  const glm::vec3& getSubjectPlaneOrigin() const;

  /// @brief Get the annotation plane coordinate axes in Subject space
  const std::pair<glm::vec3, glm::vec3>& getSubjectPlaneAxes() const;

  /// @brief Compute the projection of a 3D point (in Subject space) into
  /// 2D annotation Subject plane coordinates
  glm::vec2 projectSubjectPointToAnnotationPlane(const glm::vec3& subjectPoint) const;

  /// @brief Compute the un-projected 3D coordinates (in Subject space) of a
  /// 2D point defined in annotation Subject plane coordinates
  glm::vec3 unprojectFromAnnotationPlaneToSubjectPoint(const glm::vec2& planePoint2d) const;

private:
  std::string m_displayName; //!< Annotation display name
  fs::path m_fileName;       //!< Annotation file name

  /// Annotation polygon, which can include holes
  AnnotPolygon<float, 2> m_polygon;

  /// Highlighted vertices: pairs of { boundary index, vertex index }
  std::set<std::pair<size_t, size_t> > m_highlightedVertices;

  /// Highlighted edge: pairs of { boundary index, {vertex index 1, vertex index 2} }
  std::set<std::pair<size_t, std::pair<size_t, size_t> > > m_highlightedEdges;

  bool m_highlighted;      //!< Is the annotation highlighted?
  bool m_visible;          //!< Is the annotation visible?
  bool m_filled;           //!< Is the annotation filled?
  bool m_vertexVisibility; //!< Are the annotation boundary vertices visible?

  /// Overall annotation opacity in [0.0, 1.0] range.
  /// The annotation fill and line colors opacities are modulated by this value.
  float m_opacity;

  glm::vec4 m_vertexColor; //!< Vertex color (non-premultiplied RGBA)
  glm::vec4 m_fillColor;   //!< Fill color (non-premultiplied RGBA)
  glm::vec4 m_lineColor;   //!< Line color (non-premultiplied RGBA)
  float m_lineThickness;   //!< Line thickness

  /// Equation of the 3D plane containing this annotation. The plane is defined by the
  /// coefficients (A, B, C, D) of equation Ax + By + Cz + D = 0, where (x, y, z) are
  /// coordinates in Subject space.
  glm::vec4 m_subjectPlaneEquation;

  /// 3D origin of the plane in Subject space
  glm::vec3 m_subjectPlaneOrigin;

  /// 3D axes of the plane in Subject space
  std::pair<glm::vec3, glm::vec3> m_subjectPlaneAxes;
};

#endif // ANNOTATION_H
