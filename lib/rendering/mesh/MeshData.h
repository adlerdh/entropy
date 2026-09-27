#pragma once

#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#include <cstdint>
#include <optional>
#include <vector>

namespace rendering::mesh
{

/**
 * @brief Coordinate space of the vertex positions stored in a mesh
 */
enum class MeshCoordinateSpace
{
  ImageSubject,
  World
};

/**
 * @brief CPU-only triangle mesh data
 *
 * `MeshData` owns no OpenGL objects. It is the transfer object between extraction, caching, validation, upload, and
 * render-list construction.
 */
struct MeshData
{
  std::vector<glm::vec3> positions;                    //!< Vertex positions
  std::vector<glm::vec3> normals;                      //!< Optional vertex normals
  std::vector<uint32_t> indices;                       //!< Triangle index buffer
  std::optional<std::vector<glm::vec4>> colors;        //!< Optional per-vertex normalized RGBA colors
  std::optional<std::vector<glm::vec3>> textureCoords; //!< Optional per-vertex image texture coordinates
  MeshCoordinateSpace coordinateSpace = MeshCoordinateSpace::ImageSubject; //!< Position coordinate space
};

} // namespace rendering::mesh
