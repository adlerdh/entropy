#pragma once

#include <glm/vec3.hpp>

#include <expected>
#include <memory>
#include <string>
#include <vector>

namespace rendering::mesh
{

struct MeshData;

/// One line segment produced where a triangle mesh intersects a plane.
struct MeshPlaneIntersectionSegment
{
  glm::vec3 first{0.0f};  //!< First segment endpoint in the mesh coordinate space
  glm::vec3 second{0.0f}; //!< Second segment endpoint in the mesh coordinate space

  bool operator==(const MeshPlaneIntersectionSegment&) const = default;
};

/**
 * @brief Reusable, VTK-backed intersection query for one immutable triangle mesh.
 *
 * The implementation retains VTK's acceleration structure between plane changes. VTK types remain hidden behind the
 * private implementation so clients do not acquire VTK header or link dependencies through this interface.
 */
class MeshPlaneIntersector
{
public:
  MeshPlaneIntersector(MeshPlaneIntersector&&) noexcept;
  MeshPlaneIntersector& operator=(MeshPlaneIntersector&&) noexcept;
  ~MeshPlaneIntersector();

  MeshPlaneIntersector(const MeshPlaneIntersector&) = delete;
  MeshPlaneIntersector& operator=(const MeshPlaneIntersector&) = delete;

  /**
   * @brief Create an intersector from indexed triangle geometry.
   * @return An intersector, or a descriptive error when the mesh is invalid.
   */
  static std::expected<MeshPlaneIntersector, std::string> create(const MeshData& mesh);

  /**
   * @brief Intersect the retained mesh with a plane in the mesh coordinate space.
   * @param planeOrigin Any point on the plane.
   * @param planeNormal Nonzero plane normal. It need not be normalized.
   * @return Independent line segments, or a descriptive error for an invalid plane.
   */
  std::expected<std::vector<MeshPlaneIntersectionSegment>, std::string> intersect(
    const glm::vec3& planeOrigin,
    const glm::vec3& planeNormal);

private:
  struct Impl;
  explicit MeshPlaneIntersector(std::unique_ptr<Impl> implementation);

  std::unique_ptr<Impl> m_impl;
};

} // namespace rendering::mesh
