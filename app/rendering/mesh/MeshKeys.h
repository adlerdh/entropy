#pragma once

#include "rendering/mesh/MeshDrawOptions.h"
#include "rendering/mesh/MeshMaterial.h"
#include "rendering/mesh/MeshRenderable.h"

#include <glm/vec4.hpp>
#include <uuid.h>

#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>

namespace rendering::mesh
{

/**
 * @brief Inputs that change mesh vertices, normals, colors, or indices
 */
struct MeshGeometryKey
{
  uuids::uuid sourceUid = {};              //!< Source image, segmentation, annotation, or imported mesh UID
  uint64_t sourceDataVersion = 0;          //!< Version of source voxel/geometry data
  uint64_t sourceGeometryVersion = 0;      //!< Version of source spatial metadata or transforms
  std::optional<uint32_t> component;       //!< Image component for component-specific surfaces
  std::optional<int64_t> labelValue;       //!< Segmentation label for label-specific surfaces
  uint32_t timePoint = 0;                  //!< Time frame used to extract geometry
  double isoValue = 0.0;                   //!< Isosurface value when relevant
  std::string extractionAlgorithm;         //!< Algorithm identifier
  uint64_t extractionAlgorithmVersion = 0; //!< Algorithm/settings version

  bool operator==(const MeshGeometryKey&) const = default;
};

/**
 * @brief Hasher for mesh geometry keys
 */
struct MeshGeometryKeyHash
{
  /**
   * @brief Hash a geometry key
   * @param key Key to hash
   * @return Hash value
   */
  std::size_t operator()(const MeshGeometryKey& key) const;
};

/**
 * @brief Inputs that change mesh appearance without changing geometry
 */
struct MeshStyleKey
{
  MeshMaterial material;                                             //!< Material appearance
  MeshCompositingMode compositingMode = MeshCompositingMode::Opaque; //!< Compositing path
  MeshFillMode fillMode = MeshFillMode::Surface;                     //!< Surface, wireframe, overlay, or points
  bool backfaceCulling = false;                                      //!< Whether back-facing triangles may be culled

  bool operator==(const MeshStyleKey&) const = default;
};

/**
 * @brief Hasher for mesh style keys
 */
struct MeshStyleKeyHash
{
  /**
   * @brief Hash a style key
   * @param key Key to hash
   * @return Hash value
   */
  std::size_t operator()(const MeshStyleKey& key) const;
};

} // namespace rendering::mesh
