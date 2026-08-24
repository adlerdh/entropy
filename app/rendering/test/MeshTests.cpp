#include "common/UuidUtility.h"
#include "image/Image.h"
#include "image/ImageHeader.h"
#include "image/ImageIoInfo.h"
#include "image/ImageTypes.h"
#include "rendering/mesh/MeshAdvancedLighting.h"
#include "rendering/mesh/MeshBounds.h"
#include "rendering/mesh/MeshCache.h"
#include "rendering/mesh/MeshClipPlanes.h"
#include "rendering/mesh/MeshCrosshairsPolicy.h"
#include "rendering/mesh/MeshDdpPolicy.h"
#include "rendering/mesh/MeshExtraction.h"
#include "rendering/mesh/MeshExtractionQueue.h"
#include "rendering/mesh/MeshExtractionRunner.h"
#include "rendering/mesh/MeshGeneration.h"
#include "rendering/mesh/MeshGlyphs.h"
#include "rendering/mesh/MeshImageAdapter.h"
#include "rendering/mesh/MeshImagePlane.h"
#include "rendering/mesh/MeshImagePlaneRenderList.h"
#include "rendering/mesh/MeshImagePlaneRenderable.h"
#include "rendering/mesh/MeshImagePlaneScene.h"
#include "rendering/mesh/MeshIsosurfacePolicy.h"
#include "rendering/mesh/MeshLandmarkPolicy.h"
#include "rendering/mesh/MeshKeys.h"
#include "rendering/mesh/MeshPicking.h"
#include "rendering/mesh/MeshPrimitives.h"
#include "rendering/mesh/MeshRenderableFactory.h"
#include "rendering/mesh/MeshRenderList.h"
#include "rendering/mesh/MeshResourceLifecycle.h"
#include "rendering/mesh/MeshScalarGrid.h"
#include "rendering/mesh/MeshScene.h"
#include "rendering/mesh/MeshSegmentationPolicy.h"
#include "rendering/mesh/MeshShadowMapProjection.h"
#include "rendering/mesh/MeshValidation.h"
#include "rendering/mesh/MeshViewViewport.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <glm/gtc/matrix_transform.hpp>

#include <array>
#include <chrono>
#include <limits>
#include <ranges>
#include <stdexcept>
#include <string>
#include <thread>
#include <unordered_set>
#include <vector>

namespace mesh = rendering::mesh;

namespace
{

mesh::MeshData makeTriangleMesh()
{
  return mesh::MeshData{
    .positions = {glm::vec3{-1.0f, -1.0f, 0.0f}, glm::vec3{1.0f, -1.0f, 0.0f}, glm::vec3{0.0f, 1.0f, 0.0f}},
    .normals = {glm::vec3{0.0f, 0.0f, 1.0f}, glm::vec3{0.0f, 0.0f, 1.0f}, glm::vec3{0.0f, 0.0f, 1.0f}},
    .indices = {0, 1, 2}};
}

mesh::ScalarGrid3D makePlanarScalarGrid()
{
  mesh::ScalarGrid3D grid;
  grid.dimensions = glm::uvec3{2, 2, 2};
  grid.values.resize(8);

  for (uint32_t k = 0; k < grid.dimensions.z; ++k) {
    for (uint32_t j = 0; j < grid.dimensions.y; ++j) {
      for (uint32_t i = 0; i < grid.dimensions.x; ++i) {
        grid.values[mesh::scalarGridValueIndex(grid.dimensions, i, j, k)] = static_cast<float>(i);
      }
    }
  }

  return grid;
}

mesh::ScalarGrid3D makeBinaryLabelGrid()
{
  mesh::ScalarGrid3D grid;
  grid.dimensions = glm::uvec3{2, 2, 2};
  grid.values.resize(8);

  for (uint32_t k = 0; k < grid.dimensions.z; ++k) {
    for (uint32_t j = 0; j < grid.dimensions.y; ++j) {
      for (uint32_t i = 0; i < grid.dimensions.x; ++i) {
        grid.values[mesh::scalarGridValueIndex(grid.dimensions, i, j, k)] = i == 0 ? 7.0f : 3.0f;
      }
    }
  }

  return grid;
}

ImageIoInfo
makeMeshIoInfo(const ComponentType componentType, const uint32_t numComponents, const glm::uvec3& dimensions)
{
  ImageIoInfo info;
  info.m_fileInfo.m_fileName = "mesh-grid-test.nii";
  info.m_componentInfo.m_componentType = componentType;
  info.m_componentInfo.m_componentTypeString = componentTypeString(componentType);
  info.m_componentInfo.m_componentSizeInBytes = sizeof(float);
  info.m_pixelInfo.m_pixelType = numComponents == 1 ? PixelType::Scalar : PixelType::Vector;
  info.m_pixelInfo.m_pixelTypeString = numComponents == 1 ? "Scalar" : "Vector";
  info.m_pixelInfo.m_numComponents = numComponents;
  info.m_pixelInfo.m_pixelStrideInBytes = numComponents * info.m_componentInfo.m_componentSizeInBytes;

  info.m_sizeInfo.m_imageSizeInPixels = static_cast<std::size_t>(dimensions.x) * dimensions.y * dimensions.z;
  info.m_sizeInfo.m_imageSizeInComponents = info.m_sizeInfo.m_imageSizeInPixels * numComponents;
  info.m_sizeInfo.m_imageSizeInBytes =
    info.m_sizeInfo.m_imageSizeInComponents * info.m_componentInfo.m_componentSizeInBytes;

  info.m_spaceInfo.m_numDimensions = 3;
  info.m_spaceInfo.m_dimensions = {dimensions.x, dimensions.y, dimensions.z};
  info.m_spaceInfo.m_origin = {10.0, 20.0, 30.0};
  info.m_spaceInfo.m_spacing = {2.0, 3.0, 4.0};
  info.m_spaceInfo.m_directions = {{0.0, 1.0, 0.0}, {-1.0, 0.0, 0.0}, {0.0, 0.0, 1.0}};
  return info;
}

Image makeMeshScalarImage()
{
  const glm::uvec3 dimensions{2, 2, 2};
  const ImageIoInfo ioInfo = makeMeshIoInfo(ComponentType::Float32, 1, dimensions);
  ImageHeader header(ioInfo, ioInfo, false);

  std::vector<float> values(8);
  for (uint32_t k = 0; k < dimensions.z; ++k) {
    for (uint32_t j = 0; j < dimensions.y; ++j) {
      for (uint32_t i = 0; i < dimensions.x; ++i) {
        values[mesh::scalarGridValueIndex(dimensions, i, j, k)] =
          100.0f * static_cast<float>(k) + 10.0f * static_cast<float>(j) + static_cast<float>(i);
      }
    }
  }

  const std::vector<const void*> buffers{values.data()};
  return Image(
    header,
    "mesh-grid-test",
    Image::ImageRepresentation::Image,
    Image::MultiComponentBufferType::SeparateImages,
    buffers);
}

Image makeMeshLabelImage()
{
  const glm::uvec3 dimensions{2, 2, 2};
  const ImageIoInfo ioInfo = makeMeshIoInfo(ComponentType::UInt32, 1, dimensions);
  ImageHeader header(ioInfo, ioInfo, false);
  constexpr uint32_t targetLabel = 16'777'217u;
  constexpr uint32_t adjacentLabel = 16'777'216u;
  const std::vector<uint32_t> values{
    targetLabel,
    adjacentLabel,
    targetLabel,
    adjacentLabel,
    targetLabel,
    adjacentLabel,
    targetLabel,
    adjacentLabel};
  const std::vector<const void*> buffers{values.data()};
  return Image(
    header,
    "mesh-label-test",
    Image::ImageRepresentation::Segmentation,
    Image::MultiComponentBufferType::SeparateImages,
    buffers);
}

void requireVec3(const glm::vec3& value, float x, float y, float z)
{
  CHECK(value.x == Catch::Approx(x));
  CHECK(value.y == Catch::Approx(y));
  CHECK(value.z == Catch::Approx(z));
}

std::vector<mesh::MeshExtractionJobResult> waitForCompleted(mesh::MeshExtractionQueue& queue)
{
  for (int attempt = 0; attempt < 100; ++attempt) {
    std::vector<mesh::MeshExtractionJobResult> completed = queue.takeCompleted();
    if (!completed.empty()) {
      return completed;
    }

    std::this_thread::sleep_for(std::chrono::milliseconds{1});
  }

  return {};
}

class FakeIsosurfaceExtractor : public mesh::IIsosurfaceMeshExtractor
{
public:
  std::optional<mesh::MeshExtractionResult> result;
  bool staleBeforeReturning = false;
  mesh::MeshCache* cache = nullptr;

  std::optional<mesh::MeshExtractionResult> extract(const mesh::IsosurfaceMeshRequest& request) override
  {
    if (staleBeforeReturning && cache) {
      cache->markSourceStale(request.imageUid);
    }
    return result;
  }
};

class FakeSegmentationExtractor : public mesh::ISegmentationMeshExtractor
{
public:
  std::optional<mesh::MeshExtractionResult> result;

  std::optional<mesh::MeshExtractionResult> extract(const mesh::SegmentationMeshRequest&) override
  {
    return result;
  }
};

} // namespace

TEST_CASE("mesh validation accepts a finite triangle mesh", "[rendering][mesh]")
{
  const mesh::MeshData data = makeTriangleMesh();

  CHECK(mesh::validateMeshData(data).empty());
  CHECK(mesh::isValidMeshData(data));
}

TEST_CASE("mesh validation reports array and index errors", "[rendering][mesh]")
{
  mesh::MeshData data;
  data.positions = {glm::vec3{0.0f}, glm::vec3{1.0f}};
  data.normals = {glm::vec3{0.0f}};
  data.indices = {0, 1, 4, 0};
  data.colors = std::vector<glm::vec4>{glm::vec4{1.0f}};
  data.textureCoords = std::vector<glm::vec3>{glm::vec3{0.0f}};

  const std::vector<mesh::MeshValidationError> errors = mesh::validateMeshData(data);

  CHECK(std::ranges::find(errors, mesh::MeshValidationError::IndicesNotTriangles) != errors.end());
  CHECK(std::ranges::find(errors, mesh::MeshValidationError::IndexOutOfRange) != errors.end());
  CHECK(std::ranges::find(errors, mesh::MeshValidationError::NormalCountMismatch) != errors.end());
  CHECK(std::ranges::find(errors, mesh::MeshValidationError::ColorCountMismatch) != errors.end());
  CHECK(std::ranges::find(errors, mesh::MeshValidationError::TextureCoordCountMismatch) != errors.end());
}

TEST_CASE("mesh validation reports non-finite vertex data", "[rendering][mesh]")
{
  mesh::MeshData data = makeTriangleMesh();
  data.positions[1].x = std::numeric_limits<float>::infinity();
  data.normals[2].z = std::numeric_limits<float>::quiet_NaN();
  data.colors = std::vector<glm::vec4>{
    glm::vec4{1.0f},
    glm::vec4{1.0f},
    glm::vec4{0.0f, 0.0f, 0.0f, std::numeric_limits<float>::quiet_NaN()}};
  data.textureCoords = std::vector<glm::vec3>{
    glm::vec3{0.0f},
    glm::vec3{std::numeric_limits<float>::quiet_NaN(), 0.0f, 0.0f},
    glm::vec3{1.0f}};

  const std::vector<mesh::MeshValidationError> errors = mesh::validateMeshData(data);

  CHECK(std::ranges::find(errors, mesh::MeshValidationError::NonFinitePosition) != errors.end());
  CHECK(std::ranges::find(errors, mesh::MeshValidationError::NonFiniteNormal) != errors.end());
  CHECK(std::ranges::find(errors, mesh::MeshValidationError::NonFiniteColor) != errors.end());
  CHECK(std::ranges::find(errors, mesh::MeshValidationError::NonFiniteTextureCoord) != errors.end());
}

TEST_CASE("mesh validation rejects repeated and collinear triangle vertices", "[rendering][mesh]")
{
  mesh::MeshData repeated = makeTriangleMesh();
  repeated.indices = {0u, 1u, 1u};
  mesh::MeshData collinear = makeTriangleMesh();
  collinear.positions = {glm::vec3{0.0f}, glm::vec3{1.0f, 0.0f, 0.0f}, glm::vec3{2.0f, 0.0f, 0.0f}};

  const auto repeatedErrors = mesh::validateMeshData(repeated);
  const auto collinearErrors = mesh::validateMeshData(collinear);

  CHECK(std::ranges::find(repeatedErrors, mesh::MeshValidationError::DegenerateTriangle) != repeatedErrors.end());
  CHECK(std::ranges::find(collinearErrors, mesh::MeshValidationError::DegenerateTriangle) != collinearErrors.end());
}

TEST_CASE("mesh bounds ignore non-finite positions", "[rendering][mesh]")
{
  const std::vector positions{
    glm::vec3{2.0f, -1.0f, 4.0f},
    glm::vec3{-3.0f, 5.0f, 1.0f},
    glm::vec3{std::numeric_limits<float>::quiet_NaN(), 0.0f, 0.0f}};

  const std::optional<mesh::MeshBounds> bounds = mesh::computeBounds(positions);

  REQUIRE(bounds);
  requireVec3(bounds->min, -3.0f, -1.0f, 1.0f);
  requireVec3(bounds->max, 2.0f, 5.0f, 4.0f);
  requireVec3(mesh::center(*bounds), -0.5f, 2.0f, 2.5f);
  requireVec3(mesh::diagonal(*bounds), 5.0f, 6.0f, 3.0f);
}

TEST_CASE("mesh bounds transform all corners into world space", "[rendering][mesh]")
{
  const mesh::MeshBounds bounds{glm::vec3{-1.0f, -2.0f, -3.0f}, glm::vec3{1.0f, 2.0f, 3.0f}};
  const glm::mat4 world_T_mesh = glm::translate(glm::mat4{1.0f}, glm::vec3{10.0f, 20.0f, 30.0f}) *
                                 glm::rotate(glm::mat4{1.0f}, glm::half_pi<float>(), glm::vec3{0.0f, 0.0f, 1.0f});

  const mesh::MeshBounds transformed = mesh::transformedBounds(bounds, world_T_mesh);

  requireVec3(transformed.min, 8.0f, 19.0f, 27.0f);
  requireVec3(transformed.max, 12.0f, 21.0f, 33.0f);
}

TEST_CASE("mesh renderable world bounds use renderable transform", "[rendering][mesh]")
{
  const mesh::MeshData triangle = makeTriangleMesh();
  mesh::MeshRenderable renderable;
  renderable.world_T_mesh = glm::translate(glm::mat4{1.0f}, glm::vec3{5.0f, 6.0f, 7.0f});

  const std::optional<mesh::MeshBounds> bounds = mesh::computeWorldBounds(renderable, triangle);

  REQUIRE(bounds);
  requireVec3(bounds->min, 4.0f, 5.0f, 7.0f);
  requireVec3(bounds->max, 6.0f, 7.0f, 7.0f);
}

TEST_CASE("mesh render-list bounds combine visible CPU mesh bounds", "[rendering][mesh]")
{
  mesh::MeshData first = makeTriangleMesh();
  mesh::MeshData second = makeTriangleMesh();
  second.positions = {glm::vec3{2.0f, 3.0f, 4.0f}, glm::vec3{5.0f, 3.0f, 4.0f}, glm::vec3{2.0f, 7.0f, 4.0f}};

  const mesh::MeshHandle firstHandle{.uid = generateRandomUuid(), .geometryVersion = 1};
  const mesh::MeshHandle secondHandle{.uid = generateRandomUuid(), .geometryVersion = 1};

  mesh::MeshRenderable firstRenderable;
  firstRenderable.mesh = firstHandle;
  firstRenderable.compositingMode = mesh::MeshCompositingMode::Opaque;

  mesh::MeshRenderable secondRenderable;
  secondRenderable.mesh = secondHandle;
  secondRenderable.world_T_mesh = glm::translate(glm::mat4{1.0f}, glm::vec3{10.0f, 0.0f, -2.0f});
  secondRenderable.compositingMode = mesh::MeshCompositingMode::Additive;

  const std::vector renderables{firstRenderable, secondRenderable};
  const mesh::MeshRenderList list = mesh::buildRenderList(renderables);
  const std::optional<mesh::MeshBounds> bounds =
    mesh::computeWorldBounds(list, [&](const mesh::MeshHandle& handle) -> const mesh::MeshData* {
      if (handle == firstHandle) {
        return &first;
      }
      if (handle == secondHandle) {
        return &second;
      }
      return nullptr;
    });

  REQUIRE(bounds);
  requireVec3(bounds->min, -1.0f, -1.0f, 0.0f);
  requireVec3(bounds->max, 15.0f, 7.0f, 2.0f);
}

TEST_CASE("mesh shadow-map projection encloses scene bounds", "[rendering][mesh]")
{
  const mesh::MeshBounds bounds{.min = glm::vec3{-2.0f, -1.0f, 3.0f}, .max = glm::vec3{4.0f, 5.0f, 9.0f}};
  const std::optional<mesh::MeshShadowMapProjection> projection =
    mesh::meshShadowMapProjectionForBounds(bounds, glm::vec3{0.4f, 0.6f, 0.7f});

  REQUIRE(projection);
  CHECK(glm::length(projection->lightDirectionWorld) == Catch::Approx(1.0f));

  const std::array corners{
    glm::vec3{bounds.min.x, bounds.min.y, bounds.min.z},
    glm::vec3{bounds.max.x, bounds.min.y, bounds.min.z},
    glm::vec3{bounds.min.x, bounds.max.y, bounds.min.z},
    glm::vec3{bounds.max.x, bounds.max.y, bounds.min.z},
    glm::vec3{bounds.min.x, bounds.min.y, bounds.max.z},
    glm::vec3{bounds.max.x, bounds.min.y, bounds.max.z},
    glm::vec3{bounds.min.x, bounds.max.y, bounds.max.z},
    glm::vec3{bounds.max.x, bounds.max.y, bounds.max.z}};

  for (const glm::vec3& corner : corners) {
    const glm::vec4 clip = projection->lightClip_T_world * glm::vec4{corner, 1.0f};
    const glm::vec3 ndc = glm::vec3{clip} / clip.w;
    CHECK(ndc.x >= -1.0001f);
    CHECK(ndc.x <= 1.0001f);
    CHECK(ndc.y >= -1.0001f);
    CHECK(ndc.y <= 1.0001f);
    CHECK(ndc.z >= -1.0001f);
    CHECK(ndc.z <= 1.0001f);
  }
}

TEST_CASE("clip planes normalize and classify points", "[rendering][mesh]")
{
  const std::optional<glm::vec4> plane = mesh::normalizedClipPlane(glm::vec4{0.0f, 0.0f, 2.0f, -4.0f});

  REQUIRE(plane);
  CHECK(plane->z == Catch::Approx(1.0f));
  CHECK(plane->w == Catch::Approx(-2.0f));
  CHECK(mesh::signedDistanceToPlane(*plane, glm::vec3{0.0f, 0.0f, 5.0f}) == Catch::Approx(3.0f));
}

TEST_CASE("clip planes classify bounds", "[rendering][mesh]")
{
  const mesh::MeshBounds bounds{glm::vec3{-1.0f}, glm::vec3{1.0f}};
  const mesh::MeshClipPlane insidePlane{.worldPlane = glm::vec4{1.0f, 0.0f, 0.0f, 2.0f}};
  const mesh::MeshClipPlane intersectingPlane{.worldPlane = glm::vec4{1.0f, 0.0f, 0.0f, 0.0f}};
  const mesh::MeshClipPlane outsidePlane{.worldPlane = glm::vec4{1.0f, 0.0f, 0.0f, -2.0f}};

  CHECK(mesh::classifyBoundsAgainstClipPlane(bounds, insidePlane) == mesh::ClipPlaneBoxClassification::Inside);
  CHECK(
    mesh::classifyBoundsAgainstClipPlane(bounds, intersectingPlane) == mesh::ClipPlaneBoxClassification::Intersecting);
  CHECK(mesh::classifyBoundsAgainstClipPlane(bounds, outsidePlane) == mesh::ClipPlaneBoxClassification::Outside);
}

TEST_CASE("enabled normalized clip planes omit disabled, invalid, and excess planes", "[rendering][mesh]")
{
  std::vector<mesh::MeshClipPlane> planes;
  planes.push_back(mesh::MeshClipPlane{.worldPlane = glm::vec4{0.0f, 0.0f, 0.0f, 0.0f}});
  planes.push_back(mesh::MeshClipPlane{.worldPlane = glm::vec4{0.0f, 0.0f, 2.0f, -4.0f}});
  planes.push_back(mesh::MeshClipPlane{.worldPlane = glm::vec4{1.0f, 0.0f, 0.0f, 0.0f}, .enabled = false});

  for (int i = 0; i < mesh::MaxMeshClipPlanes + 3; ++i) {
    planes.push_back(mesh::MeshClipPlane{.worldPlane = glm::vec4{1.0f, 0.0f, 0.0f, static_cast<float>(i)}});
  }

  const std::vector<glm::vec4> normalizedPlanes = mesh::enabledNormalizedClipPlanes(planes);

  REQUIRE(normalizedPlanes.size() == static_cast<std::size_t>(mesh::MaxMeshClipPlanes));
  CHECK(normalizedPlanes.front().z == Catch::Approx(1.0f));
  CHECK(normalizedPlanes.front().w == Catch::Approx(-2.0f));
  CHECK(normalizedPlanes.back().w == Catch::Approx(static_cast<float>(mesh::MaxMeshClipPlanes - 2)));
}

TEST_CASE("ray AABB intersection handles outside and inside origins", "[rendering][mesh]")
{
  const mesh::MeshPickRay outsideRay{glm::vec3{0.0f, 0.0f, 5.0f}, glm::vec3{0.0f, 0.0f, -1.0f}};
  const mesh::MeshPickRay insideRay{glm::vec3{0.0f, 0.0f, 0.0f}, glm::vec3{1.0f, 0.0f, 0.0f}};

  const std::optional<float> outsideHit = mesh::intersectRayAabb(outsideRay, glm::vec3{-1.0f}, glm::vec3{1.0f});
  const std::optional<float> insideHit = mesh::intersectRayAabb(insideRay, glm::vec3{-1.0f}, glm::vec3{1.0f});

  REQUIRE(outsideHit);
  CHECK(*outsideHit == Catch::Approx(4.0f));
  REQUIRE(insideHit);
  CHECK(*insideHit == Catch::Approx(0.0f));
}

TEST_CASE("ray triangle picking returns nearest unclipped hit", "[rendering][mesh]")
{
  const mesh::MeshData data = makeTriangleMesh();
  const mesh::MeshPickRay ray{glm::vec3{0.0f, 0.0f, 4.0f}, glm::vec3{0.0f, 0.0f, -1.0f}};

  const std::optional<mesh::MeshTriangleHit> hit = mesh::pickNearestTriangle(data, ray);

  REQUIRE(hit);
  CHECK(hit->distance == Catch::Approx(4.0f));
  CHECK(hit->triangleIndex == 0);
  requireVec3(hit->worldPosition, 0.0f, 0.0f, 0.0f);
  CHECK(hit->barycentric.x + hit->barycentric.y + hit->barycentric.z == Catch::Approx(1.0f));
}

TEST_CASE("ray triangle picking honors enabled clip planes", "[rendering][mesh]")
{
  const mesh::MeshData data = makeTriangleMesh();
  const mesh::MeshPickRay ray{glm::vec3{0.0f, 0.0f, 4.0f}, glm::vec3{0.0f, 0.0f, -1.0f}};
  const std::vector clipPlanes{mesh::MeshClipPlane{.worldPlane = glm::vec4{0.0f, 0.0f, 1.0f, 1.0f}}};
  const std::vector rejectingClipPlanes{mesh::MeshClipPlane{.worldPlane = glm::vec4{0.0f, 0.0f, 1.0f, -1.0f}}};

  CHECK(mesh::pickNearestTriangle(data, ray, clipPlanes).has_value());
  CHECK(!mesh::pickNearestTriangle(data, ray, rejectingClipPlanes).has_value());
}

TEST_CASE("scene picking chooses nearest visible transformed renderable", "[rendering][mesh]")
{
  const mesh::MeshData triangle = makeTriangleMesh();
  mesh::MeshRenderable nearRenderable;
  nearRenderable.mesh.geometryVersion = 1;
  nearRenderable.world_T_mesh = glm::translate(glm::mat4{1.0f}, glm::vec3{0.0f, 0.0f, 1.0f});
  nearRenderable.drawOptions.pickingMode = mesh::MeshPickingMode::Triangle;

  mesh::MeshRenderable farRenderable = nearRenderable;
  farRenderable.mesh.geometryVersion = 2;
  farRenderable.world_T_mesh = glm::translate(glm::mat4{1.0f}, glm::vec3{0.0f, 0.0f, -1.0f});

  const std::vector renderables{nearRenderable, farRenderable};
  const mesh::MeshScenePickRequest request{
    .worldRay = mesh::MeshPickRay{glm::vec3{0.0f, 0.0f, 4.0f}, glm::vec3{0.0f, 0.0f, -1.0f}},
    .renderables = renderables,
    .meshLookup = [&triangle](const mesh::MeshHandle&) -> const mesh::MeshData* {
      return &triangle;
    }};

  const std::optional<mesh::MeshScenePickHit> hit = mesh::pickNearestRenderable(request);

  REQUIRE(hit);
  CHECK(hit->mesh.geometryVersion == 1);
  CHECK(hit->triangleHit.distance == Catch::Approx(3.0f));
  requireVec3(hit->triangleHit.worldPosition, 0.0f, 0.0f, 1.0f);
}

TEST_CASE("scene picking skips hidden, disabled, missing, and clipped renderables", "[rendering][mesh]")
{
  const mesh::MeshData triangle = makeTriangleMesh();

  mesh::MeshRenderable hidden;
  hidden.mesh.geometryVersion = 1;
  hidden.visible = false;
  hidden.drawOptions.pickingMode = mesh::MeshPickingMode::Triangle;

  mesh::MeshRenderable disabled = hidden;
  disabled.mesh.geometryVersion = 2;
  disabled.visible = true;
  disabled.drawOptions.pickingMode = mesh::MeshPickingMode::Disabled;

  mesh::MeshRenderable missing = hidden;
  missing.mesh.geometryVersion = 3;
  missing.visible = true;

  mesh::MeshRenderable clipped = hidden;
  clipped.mesh.geometryVersion = 4;
  clipped.visible = true;
  clipped.drawOptions.clipPlanes = {mesh::MeshClipPlane{.worldPlane = glm::vec4{0.0f, 0.0f, 1.0f, -1.0f}}};

  mesh::MeshRenderable picked = hidden;
  picked.mesh.geometryVersion = 5;
  picked.visible = true;

  const std::vector renderables{hidden, disabled, missing, clipped, picked};
  const mesh::MeshScenePickRequest request{
    .worldRay = mesh::MeshPickRay{glm::vec3{0.0f, 0.0f, 4.0f}, glm::vec3{0.0f, 0.0f, -1.0f}},
    .renderables = renderables,
    .meshLookup = [&triangle](const mesh::MeshHandle& handle) -> const mesh::MeshData* {
      return handle.geometryVersion == 3 ? nullptr : &triangle;
    }};

  const std::optional<mesh::MeshScenePickHit> hit = mesh::pickNearestRenderable(request);

  REQUIRE(hit);
  CHECK(hit->mesh.geometryVersion == 5);
}

TEST_CASE("mesh scene stores replacement renderables", "[rendering][mesh]")
{
  mesh::MeshRenderable first;
  first.mesh.geometryVersion = 1;
  mesh::MeshRenderable second;
  second.mesh.geometryVersion = 2;
  mesh::MeshImagePlaneRenderable imagePlane;
  imagePlane.mesh.geometryVersion = 3;

  mesh::MeshScene scene;
  scene.setRenderables({first});
  scene.setImagePlaneRenderables({imagePlane});
  REQUIRE(scene.renderables().size() == 1);
  CHECK(scene.renderables().front().mesh.geometryVersion == 1);
  REQUIRE(scene.imagePlaneRenderables().size() == 1);
  CHECK(scene.imagePlaneRenderables().front().mesh.geometryVersion == 3);

  mesh::applySceneUpdate(scene, mesh::MeshSceneUpdate{.renderables = {second}, .imagePlaneRenderables = {}});

  REQUIRE(scene.renderables().size() == 1);
  CHECK(scene.renderables().front().mesh.geometryVersion == 2);
  CHECK(scene.imagePlaneRenderables().empty());

  scene.clear();
  CHECK(scene.renderables().empty());
  CHECK(scene.imagePlaneRenderables().empty());
}

TEST_CASE("mesh render list filters invisible renderables and partitions compositing modes", "[rendering][mesh]")
{
  mesh::MeshRenderable opaque;
  opaque.mesh.geometryVersion = 1;
  opaque.compositingMode = mesh::MeshCompositingMode::Opaque;

  mesh::MeshRenderable hidden;
  hidden.mesh.geometryVersion = 2;
  hidden.visible = false;

  mesh::MeshRenderable ddp;
  ddp.mesh.geometryVersion = 3;
  ddp.compositingMode = mesh::MeshCompositingMode::AlphaOverDdp;

  mesh::MeshRenderable additive;
  additive.mesh.geometryVersion = 4;
  additive.compositingMode = mesh::MeshCompositingMode::Additive;

  mesh::MeshRenderable multiplicative;
  multiplicative.mesh.geometryVersion = 5;
  multiplicative.compositingMode = mesh::MeshCompositingMode::Multiplicative;

  const std::vector renderables{opaque, hidden, ddp, additive, multiplicative};
  const mesh::MeshRenderList list = mesh::buildRenderList(renderables);

  REQUIRE(list.opaque.size() == 1);
  REQUIRE(list.alphaOverDdp.size() == 1);
  REQUIRE(list.additive.size() == 1);
  REQUIRE(list.multiplicative.size() == 1);
  CHECK(list.opaque.front().get().mesh.geometryVersion == 1);
  CHECK(list.alphaOverDdp.front().get().mesh.geometryVersion == 3);
  CHECK(list.additive.front().get().mesh.geometryVersion == 4);
  CHECK(list.multiplicative.front().get().mesh.geometryVersion == 5);
  CHECK(mesh::visibleRenderableCount(list) == 4);
  CHECK(mesh::requiresDdp(list));
}

TEST_CASE("mesh DDP policy activates only for visible alpha-over renderables", "[rendering][mesh]")
{
  mesh::MeshRenderable opaque;
  opaque.compositingMode = mesh::MeshCompositingMode::Opaque;

  mesh::MeshRenderable ddp;
  ddp.compositingMode = mesh::MeshCompositingMode::AlphaOverDdp;

  const std::vector opaqueRenderables{opaque};
  const std::vector ddpRenderables{opaque, ddp};
  const mesh::MeshRenderList opaqueOnly = mesh::buildRenderList(opaqueRenderables);
  const mesh::MeshRenderList withDdp = mesh::buildRenderList(ddpRenderables);

  CHECK_FALSE(mesh::meshDdpPlanForRenderList(opaqueOnly, {}).active);

  const mesh::MeshDdpPlan activePlan = mesh::meshDdpPlanForRenderList(withDdp, {});
  CHECK(activePlan.active);
  CHECK(activePlan.peelPasses == 8);
  CHECK(activePlan.renderableCount == 1);

  const mesh::MeshDdpPlan disabledPlan = mesh::meshDdpPlanForRenderList(withDdp, {.enabled = false});
  CHECK_FALSE(disabledPlan.active);
  CHECK(disabledPlan.peelPasses == 0);
  CHECK(disabledPlan.renderableCount == 1);
}

TEST_CASE("mesh DDP policy clamps peel pass counts", "[rendering][mesh]")
{
  CHECK(mesh::sanitizedDdpPeelPasses(0, 8) == 1);
  CHECK(mesh::sanitizedDdpPeelPasses(4, 8) == 4);
  CHECK(mesh::sanitizedDdpPeelPasses(20, 8) == 8);
  CHECK(mesh::sanitizedDdpPeelPasses(4, 0) == 0);

  mesh::MeshRenderable ddp;
  ddp.compositingMode = mesh::MeshCompositingMode::AlphaOverDdp;
  const std::vector renderables{ddp};
  const mesh::MeshRenderList list = mesh::buildRenderList(renderables);

  const mesh::MeshDdpPlan defaultPlan = mesh::meshDdpPlanForRenderList(list, {});
  CHECK(defaultPlan.untilComplete);
  CHECK(defaultPlan.peelPasses == 8);

  const mesh::MeshDdpPlan fixedPlan =
    mesh::meshDdpPlanForRenderList(list, {.untilComplete = false, .maxPeelPasses = 4});
  CHECK_FALSE(fixedPlan.untilComplete);
  CHECK(fixedPlan.peelPasses == 4);
  CHECK(mesh::meshDdpPlanForRenderList(list, {.maxPeelPasses = 0}).peelPasses == 1);
  CHECK(mesh::meshDdpPlanForRenderList(list, {.maxPeelPasses = 40}).peelPasses == 32);
}

TEST_CASE("mesh DDP remains active when image planes are its only renderables", "[rendering][mesh][ddp]")
{
  const mesh::MeshRenderList emptyList;
  const mesh::MeshDdpSettings settings{};
  const mesh::MeshDdpPlan emptyPlan = mesh::meshDdpPlanForRenderList(emptyList, settings);
  REQUIRE_FALSE(emptyPlan.active);

  const mesh::MeshDdpPlan imagePlanesOnly = mesh::meshDdpPlanWithExtraRenderables(emptyPlan, settings, 3u);
  CHECK(imagePlanesOnly.active);
  CHECK(imagePlanesOnly.untilComplete);
  CHECK(imagePlanesOnly.peelPasses == 8u);
  CHECK(imagePlanesOnly.renderableCount == 3u);

  const mesh::MeshDdpPlan disabled =
    mesh::meshDdpPlanWithExtraRenderables(emptyPlan, mesh::MeshDdpSettings{.enabled = false}, 3u);
  CHECK_FALSE(disabled.active);
  CHECK(disabled.peelPasses == 0u);
  CHECK(disabled.renderableCount == 3u);
}

TEST_CASE("mesh DDP adaptive peeling stops on completion or its safety limit", "[rendering][mesh]")
{
  const mesh::MeshDdpPlan adaptivePlan{.active = true, .untilComplete = true, .peelPasses = 8, .renderableCount = 1};

  CHECK(mesh::shouldContinueDdpPeeling(0, adaptivePlan, true));
  CHECK(mesh::shouldContinueDdpPeeling(7, adaptivePlan, true));
  CHECK_FALSE(mesh::shouldContinueDdpPeeling(8, adaptivePlan, true));
  CHECK_FALSE(mesh::shouldContinueDdpPeeling(1, adaptivePlan, false));
}

TEST_CASE("mesh DDP fixed peeling ignores completion until its pass limit", "[rendering][mesh]")
{
  const mesh::MeshDdpPlan fixedPlan{.active = true, .untilComplete = false, .peelPasses = 4, .renderableCount = 1};

  CHECK(mesh::shouldContinueDdpPeeling(0, fixedPlan, false));
  CHECK(mesh::shouldContinueDdpPeeling(3, fixedPlan, false));
  CHECK_FALSE(mesh::shouldContinueDdpPeeling(4, fixedPlan, true));

  mesh::MeshDdpPlan inactivePlan = fixedPlan;
  inactivePlan.active = false;
  CHECK_FALSE(mesh::shouldContinueDdpPeeling(0, inactivePlan, true));
}

TEST_CASE("mesh DDP diagnostics explain inactive and clamped plans", "[rendering][mesh]")
{
  mesh::MeshRenderable ddp;
  ddp.compositingMode = mesh::MeshCompositingMode::AlphaOverDdp;
  const std::vector renderables{ddp};
  const mesh::MeshRenderList list = mesh::buildRenderList(renderables);

  const mesh::MeshDdpSettings disabledSettings{.enabled = false};
  const mesh::MeshDdpPlan disabledPlan = mesh::meshDdpPlanForRenderList(list, disabledSettings);
  CHECK(
    mesh::meshDdpDiagnostics(disabledPlan, disabledSettings) ==
    std::vector<std::string>{"Mesh DDP is disabled; alpha-over mesh surfaces will not use order independent blending"});

  const mesh::MeshDdpSettings clampedSettings{.maxPeelPasses = 40};
  const mesh::MeshDdpPlan clampedPlan = mesh::meshDdpPlanForRenderList(list, clampedSettings);
  CHECK(
    mesh::meshDdpDiagnostics(clampedPlan, clampedSettings) ==
    std::vector<std::string>{"Mesh DDP peel pass count was clamped to the renderer limit"});
}

TEST_CASE("mesh advanced lighting is disabled by default", "[rendering][mesh]")
{
  const mesh::MeshAdvancedLightingPlan plan = mesh::meshAdvancedLightingPlan({}, {});

  CHECK(plan.shadows.state == mesh::MeshAdvancedLightingFeatureState::Disabled);
  CHECK(plan.ambientOcclusion.state == mesh::MeshAdvancedLightingFeatureState::Disabled);
  CHECK(plan.ambientOcclusion.radiusMm == Catch::Approx(5.0f));
  CHECK(plan.ambientOcclusion.strength == Catch::Approx(0.5f));
  CHECK(plan.ambientOcclusion.power == Catch::Approx(1.0f));
  CHECK(plan.ambientOcclusion.contrast == Catch::Approx(1.0f));
  CHECK(plan.ambientOcclusion.sampleCount == 24);
  CHECK_FALSE(mesh::isRequestedButUnavailable(plan.shadows.state));
  CHECK_FALSE(mesh::isRequestedButUnavailable(plan.ambientOcclusion.state));
}

TEST_CASE("mesh advanced lighting reports requested unavailable passes", "[rendering][mesh]")
{
  mesh::MeshAdvancedLightingSettings settings;
  settings.shadows.enabled = true;
  settings.ambientOcclusion.enabled = true;

  const mesh::MeshAdvancedLightingPlan plan = mesh::meshAdvancedLightingPlan(settings, {});
  const std::vector<std::string> diagnostics = mesh::advancedLightingDiagnostics(plan);

  CHECK(plan.shadows.state == mesh::MeshAdvancedLightingFeatureState::Unavailable);
  CHECK(plan.ambientOcclusion.state == mesh::MeshAdvancedLightingFeatureState::Unavailable);
  CHECK(mesh::isRequestedButUnavailable(plan.shadows.state));
  CHECK(mesh::isRequestedButUnavailable(plan.ambientOcclusion.state));
  REQUIRE(diagnostics.size() == 2);
  CHECK(diagnostics[0] == "Mesh shadow maps were requested, but the shadow-map render pass is not available");
  CHECK(
    diagnostics[1] == "Mesh ambient occlusion was requested, but the ambient occlusion render pass is not available");
}

TEST_CASE("mesh advanced lighting enables supported requested passes", "[rendering][mesh]")
{
  mesh::MeshAdvancedLightingSettings settings;
  settings.shadows.enabled = true;
  settings.ambientOcclusion.enabled = true;

  const mesh::MeshAdvancedLightingCapabilities capabilities{
    .shadowMapPassAvailable = true,
    .ambientOcclusionPassAvailable = true};
  const mesh::MeshAdvancedLightingPlan plan = mesh::meshAdvancedLightingPlan(settings, capabilities);

  CHECK(plan.shadows.state == mesh::MeshAdvancedLightingFeatureState::Enabled);
  CHECK(plan.ambientOcclusion.state == mesh::MeshAdvancedLightingFeatureState::Enabled);
  CHECK(mesh::advancedLightingDiagnostics(plan).empty());
}

TEST_CASE("mesh advanced lighting clamps renderer resource settings", "[rendering][mesh]")
{
  mesh::MeshAdvancedLightingSettings lowSettings;
  lowSettings.shadows.mapSizePixels = 1;
  lowSettings.shadows.strength = -4.0f;
  lowSettings.shadows.depthBias = -1.0f;
  lowSettings.ambientOcclusion.radiusMm = -2.0f;
  lowSettings.ambientOcclusion.strength = -1.0f;
  lowSettings.ambientOcclusion.power = -1.0f;
  lowSettings.ambientOcclusion.contrast = -1.0f;
  lowSettings.ambientOcclusion.sampleCount = 0;

  const mesh::MeshAdvancedLightingPlan lowPlan = mesh::meshAdvancedLightingPlan(lowSettings, {});
  CHECK(lowPlan.shadows.mapSizePixels == 128);
  CHECK(lowPlan.shadows.strength == 0.0f);
  CHECK(lowPlan.shadows.depthBias == 0.0f);
  CHECK(lowPlan.ambientOcclusion.radiusMm == Catch::Approx(0.1f));
  CHECK(lowPlan.ambientOcclusion.strength == 0.0f);
  CHECK(lowPlan.ambientOcclusion.power == Catch::Approx(0.1f));
  CHECK(lowPlan.ambientOcclusion.contrast == Catch::Approx(0.1f));
  CHECK(lowPlan.ambientOcclusion.sampleCount == 8);

  mesh::MeshAdvancedLightingSettings highSettings;
  highSettings.shadows.mapSizePixels = 65536;
  highSettings.shadows.strength = 4.0f;
  highSettings.shadows.depthBias = 1.0f;
  highSettings.ambientOcclusion.radiusMm = 2000.0f;
  highSettings.ambientOcclusion.strength = 2.0f;
  highSettings.ambientOcclusion.power = 10.0f;
  highSettings.ambientOcclusion.contrast = 10.0f;
  highSettings.ambientOcclusion.sampleCount = 1024;

  const mesh::MeshAdvancedLightingPlan highPlan = mesh::meshAdvancedLightingPlan(highSettings, {});
  CHECK(highPlan.shadows.mapSizePixels == 8192);
  CHECK(highPlan.shadows.strength == 1.0f);
  CHECK(highPlan.shadows.depthBias == Catch::Approx(0.1f));
  CHECK(highPlan.ambientOcclusion.radiusMm == Catch::Approx(1000.0f));
  CHECK(highPlan.ambientOcclusion.strength == 1.0f);
  CHECK(highPlan.ambientOcclusion.power == Catch::Approx(8.0f));
  CHECK(highPlan.ambientOcclusion.contrast == Catch::Approx(8.0f));
  CHECK(highPlan.ambientOcclusion.sampleCount == 64);
}

TEST_CASE("isosurface renderable factory preserves style and enables triangle picking", "[rendering][mesh]")
{
  const mesh::MeshHandle handle{.uid = generateRandomUuid(), .geometryVersion = 4};
  const glm::mat4 world_T_mesh = glm::translate(glm::mat4{1.0f}, glm::vec3{1.0f, 2.0f, 3.0f});
  const mesh::IsosurfaceMeshStyle style{
    .material =
      {.baseColor = glm::vec4{0.25f, 0.5f, 0.75f, 0.6f},
       .metallic = 0.2f,
       .roughness = 0.4f,
       .ambientOcclusion = 0.8f,
       .shadingModel = mesh::MeshShadingModel::PhysicallyBased},
    .compositingMode = mesh::MeshCompositingMode::AlphaOverDdp,
    .fillMode = mesh::MeshFillMode::SurfaceWithWireframe,
    .backfaceCulling = true,
    .visible = true};

  const mesh::MeshRenderable renderable = mesh::makeIsosurfaceRenderable(handle, world_T_mesh, style);

  CHECK(renderable.mesh == handle);
  CHECK(renderable.world_T_mesh == world_T_mesh);
  CHECK(renderable.material == style.material);
  CHECK(renderable.compositingMode == mesh::MeshCompositingMode::AlphaOverDdp);
  CHECK(renderable.drawOptions.fillMode == mesh::MeshFillMode::SurfaceWithWireframe);
  CHECK(renderable.drawOptions.pickingMode == mesh::MeshPickingMode::Triangle);
  CHECK(renderable.drawOptions.backfaceCulling);
  CHECK(renderable.visible);
}

TEST_CASE("segmentation label renderable factory hides transparent labels", "[rendering][mesh]")
{
  const mesh::MeshHandle handle{.uid = generateRandomUuid(), .geometryVersion = 5};
  const mesh::SegmentationLabelMeshStyle hiddenStyle{
    .labelValue = 12,
    .material = {.baseColor = glm::vec4{1.0f, 0.0f, 0.0f, 0.0f}},
    .compositingMode = mesh::MeshCompositingMode::Additive,
    .fillMode = mesh::MeshFillMode::Wireframe,
    .backfaceCulling = false,
    .visible = true};

  const mesh::MeshRenderable renderable = mesh::makeSegmentationLabelRenderable(handle, glm::mat4{1.0f}, hiddenStyle);

  CHECK(renderable.mesh == handle);
  CHECK(renderable.material == hiddenStyle.material);
  CHECK(renderable.compositingMode == mesh::MeshCompositingMode::Additive);
  CHECK(renderable.drawOptions.fillMode == mesh::MeshFillMode::Wireframe);
  CHECK_FALSE(renderable.visible);
}

TEST_CASE("sphere glyph renderable uses world-space radius and object picking", "[rendering][mesh]")
{
  const mesh::MeshHandle handle{.uid = generateRandomUuid(), .geometryVersion = 1};
  const mesh::MeshSphereGlyphStyle style{
    .radiusWorld = 2.5f,
    .color = glm::vec4{0.2f, 0.3f, 0.4f, 0.5f},
    .compositingMode = mesh::MeshCompositingMode::AlphaOverDdp,
    .visible = true};

  const mesh::MeshRenderable renderable = mesh::makeSphereGlyphRenderable(handle, glm::vec3{1.0f, 2.0f, 3.0f}, style);

  CHECK(renderable.mesh == handle);
  CHECK(renderable.material.baseColor == style.color);
  CHECK(renderable.compositingMode == mesh::MeshCompositingMode::AlphaOverDdp);
  CHECK(renderable.drawOptions.pickingMode == mesh::MeshPickingMode::Object);
  CHECK(renderable.visible);
  requireVec3(glm::vec3{renderable.world_T_mesh[0]}, 2.5f, 0.0f, 0.0f);
  requireVec3(glm::vec3{renderable.world_T_mesh[1]}, 0.0f, 2.5f, 0.0f);
  requireVec3(glm::vec3{renderable.world_T_mesh[2]}, 0.0f, 0.0f, 2.5f);
  requireVec3(glm::vec3{renderable.world_T_mesh[3]}, 1.0f, 2.0f, 3.0f);
}

TEST_CASE("mesh crosshairs glyph policy disables invalid or redundant glyphs", "[rendering][mesh]")
{
  CHECK(mesh::shouldRenderMeshCrosshairsGlyph(
    {.showCrosshairsIn3D = true,
     .cameraFollowsCrosshairs = false,
     .diameterVoxelDiagonals = 2.0f,
     .lengthVoxelDiagonals = 8.0f,
     .voxelDiagonalWorld = 3.0f}));

  CHECK_FALSE(mesh::shouldRenderMeshCrosshairsGlyph(
    {.showCrosshairsIn3D = false,
     .cameraFollowsCrosshairs = false,
     .diameterVoxelDiagonals = 2.0f,
     .lengthVoxelDiagonals = 8.0f,
     .voxelDiagonalWorld = 3.0f}));
  CHECK_FALSE(mesh::shouldRenderMeshCrosshairsGlyph(
    {.showCrosshairsIn3D = true,
     .cameraFollowsCrosshairs = true,
     .diameterVoxelDiagonals = 2.0f,
     .lengthVoxelDiagonals = 8.0f,
     .voxelDiagonalWorld = 3.0f}));
  CHECK_FALSE(mesh::shouldRenderMeshCrosshairsGlyph(
    {.showCrosshairsIn3D = true,
     .cameraFollowsCrosshairs = false,
     .diameterVoxelDiagonals = 0.0f,
     .lengthVoxelDiagonals = 8.0f,
     .voxelDiagonalWorld = 3.0f}));
  CHECK_FALSE(mesh::shouldRenderMeshCrosshairsGlyph(
    {.showCrosshairsIn3D = true,
     .cameraFollowsCrosshairs = false,
     .diameterVoxelDiagonals = 2.0f,
     .lengthVoxelDiagonals = 8.0f,
     .voxelDiagonalWorld = 0.0f}));
  CHECK_FALSE(mesh::shouldRenderMeshCrosshairsGlyph(
    {.showCrosshairsIn3D = true,
     .cameraFollowsCrosshairs = false,
     .diameterVoxelDiagonals = 2.0f,
     .lengthVoxelDiagonals = 0.0f,
     .voxelDiagonalWorld = 3.0f}));
}

TEST_CASE("mesh crosshairs glyph style converts voxel units to physical dimensions", "[rendering][mesh]")
{
  const mesh::MeshCrosshairsGlyphInputs inputs{
    .showCrosshairsIn3D = true,
    .cameraFollowsCrosshairs = false,
    .diameterVoxelDiagonals = 2.0f,
    .lengthVoxelDiagonals = 8.0f,
    .voxelDiagonalWorld = 3.0f};

  const mesh::MeshCrosshairsGlyphStyle style = mesh::meshCrosshairsGlyphStyle(inputs);

  CHECK(style.radiusWorld == Catch::Approx(3.0f));
  CHECK(style.halfLengthWorld == Catch::Approx(12.0f));
  CHECK(style.visible);
}

TEST_CASE("mesh landmark glyph policy disables hidden or degenerate glyphs", "[rendering][mesh]")
{
  CHECK(mesh::shouldRenderMeshLandmarkGlyph(
    {.groupVisible = true,
     .pointVisible = true,
     .groupOpacity = 0.5f,
     .radiusFactor = 0.02f,
     .voxelDiagonalWorld = 3.0f}));

  CHECK_FALSE(mesh::shouldRenderMeshLandmarkGlyph(
    {.groupVisible = false,
     .pointVisible = true,
     .groupOpacity = 0.5f,
     .radiusFactor = 0.02f,
     .voxelDiagonalWorld = 3.0f}));
  CHECK_FALSE(mesh::shouldRenderMeshLandmarkGlyph(
    {.groupVisible = true,
     .pointVisible = false,
     .groupOpacity = 0.5f,
     .radiusFactor = 0.02f,
     .voxelDiagonalWorld = 3.0f}));
  CHECK_FALSE(mesh::shouldRenderMeshLandmarkGlyph(
    {.groupVisible = true,
     .pointVisible = true,
     .groupOpacity = 0.0f,
     .radiusFactor = 0.02f,
     .voxelDiagonalWorld = 3.0f}));
  CHECK_FALSE(mesh::shouldRenderMeshLandmarkGlyph(
    {.groupVisible = true,
     .pointVisible = true,
     .groupOpacity = 0.5f,
     .radiusFactor = 0.0f,
     .voxelDiagonalWorld = 3.0f}));
  CHECK_FALSE(mesh::shouldRenderMeshLandmarkGlyph(
    {.groupVisible = true,
     .pointVisible = true,
     .groupOpacity = 0.5f,
     .radiusFactor = 0.02f,
     .voxelDiagonalWorld = 0.0f}));
}

TEST_CASE("mesh landmark glyph style uses selected color source and alpha-over compositing", "[rendering][mesh]")
{
  const mesh::MeshLandmarkGlyphInputs groupColorInputs{
    .groupVisible = true,
    .pointVisible = true,
    .groupColorOverride = true,
    .groupOpacity = 1.5f,
    .radiusFactor = 0.25f,
    .voxelDiagonalWorld = 4.0f,
    .groupColor = glm::vec3{1.0f, 0.25f, 0.0f},
    .pointColor = glm::vec3{0.0f, 0.0f, 1.0f}};

  const mesh::MeshSphereGlyphStyle groupStyle = mesh::meshLandmarkSphereGlyphStyle(groupColorInputs);
  CHECK(groupStyle.radiusWorld == Catch::Approx(1.0f));
  CHECK(groupStyle.color == glm::vec4{1.0f, 0.25f, 0.0f, 1.0f});
  CHECK(groupStyle.compositingMode == mesh::MeshCompositingMode::AlphaOverDdp);
  CHECK(groupStyle.visible);

  mesh::MeshLandmarkGlyphInputs pointColorInputs = groupColorInputs;
  pointColorInputs.groupColorOverride = false;
  pointColorInputs.groupOpacity = 0.5f;

  const mesh::MeshSphereGlyphStyle pointStyle = mesh::meshLandmarkSphereGlyphStyle(pointColorInputs);
  CHECK(pointStyle.color == glm::vec4{0.0f, 0.0f, 1.0f, 0.5f});
}

TEST_CASE("cylinder glyph renderable uses world-space radius and length", "[rendering][mesh]")
{
  const mesh::MeshHandle handle{.uid = generateRandomUuid(), .geometryVersion = 2};
  const mesh::MeshCylinderGlyphStyle style{
    .radiusWorld = 1.5f,
    .lengthWorld = 6.0f,
    .color = glm::vec4{1.0f, 0.0f, 0.0f, 0.0f},
    .compositingMode = mesh::MeshCompositingMode::Opaque,
    .visible = true};

  const mesh::MeshRenderable renderable =
    mesh::makeZAxisCylinderGlyphRenderable(handle, glm::vec3{-1.0f, -2.0f, -3.0f}, style);

  CHECK(renderable.mesh == handle);
  CHECK(renderable.drawOptions.pickingMode == mesh::MeshPickingMode::Object);
  CHECK_FALSE(renderable.visible);
  requireVec3(glm::vec3{renderable.world_T_mesh[0]}, 1.5f, 0.0f, 0.0f);
  requireVec3(glm::vec3{renderable.world_T_mesh[1]}, 0.0f, 1.5f, 0.0f);
  requireVec3(glm::vec3{renderable.world_T_mesh[2]}, 0.0f, 0.0f, 6.0f);
  requireVec3(glm::vec3{renderable.world_T_mesh[3]}, -1.0f, -2.0f, -3.0f);
}

TEST_CASE("mesh geometry keys separate geometry changes from style changes", "[rendering][mesh]")
{
  mesh::MeshGeometryKey base;
  base.sourceDataVersion = 10;
  base.sourceGeometryVersion = 20;
  base.component = 1;
  base.timePoint = 2;
  base.isoValue = 42.0;
  base.extractionAlgorithm = "test";
  base.extractionAlgorithmVersion = 3;

  mesh::MeshGeometryKey changedIso = base;
  changedIso.isoValue = 43.0;

  mesh::MeshGeometryKey changedStyleOnly = base;

  CHECK(base == changedStyleOnly);
  CHECK_FALSE(base == changedIso);

  std::unordered_set<mesh::MeshGeometryKey, mesh::MeshGeometryKeyHash> keys;
  keys.insert(base);
  keys.insert(changedIso);
  keys.insert(changedStyleOnly);
  CHECK(keys.size() == 2);
}

TEST_CASE("mesh style keys change for material and draw-state edits", "[rendering][mesh]")
{
  mesh::MeshStyleKey base;

  mesh::MeshStyleKey changedColor = base;
  changedColor.material.baseColor = glm::vec4{1.0f, 0.0f, 0.0f, 1.0f};

  mesh::MeshStyleKey changedFill = base;
  changedFill.fillMode = mesh::MeshFillMode::Wireframe;

  CHECK_FALSE(base == changedColor);
  CHECK_FALSE(base == changedFill);

  std::unordered_set<mesh::MeshStyleKey, mesh::MeshStyleKeyHash> keys;
  keys.insert(base);
  keys.insert(changedColor);
  keys.insert(changedFill);
  CHECK(keys.size() == 3);
}

TEST_CASE("mesh materials sanitize shader-facing values", "[rendering][mesh]")
{
  mesh::MeshMaterial material;
  material.baseColor = glm::vec4{-1.0f, 2.0f, 0.5f, 1.5f};
  material.metallic = 2.0f;
  material.roughness = 0.0f;
  material.ambientOcclusion = -1.0f;
  material.shadingModel = mesh::MeshShadingModel::PhysicallyBased;
  material.rimLightingEnabled = true;
  material.rimOpacityStrength = 2.0f;
  material.rimEmissionStrength = -1.0f;
  material.rimPower = 0.0f;

  const mesh::MeshMaterial sanitized = mesh::sanitizedMaterial(material, glm::vec4{0.25f, 0.5f, 0.75f, 1.0f});

  CHECK(sanitized.baseColor == glm::vec4{0.0f, 1.0f, 0.5f, 1.0f});
  CHECK(sanitized.metallic == Catch::Approx(1.0f));
  CHECK(sanitized.roughness == Catch::Approx(0.001f));
  CHECK(sanitized.ambientOcclusion == Catch::Approx(0.0f));
  CHECK(sanitized.shadingModel == mesh::MeshShadingModel::PhysicallyBased);
  CHECK(sanitized.rimLightingEnabled);
  CHECK(sanitized.rimOpacityStrength == Catch::Approx(1.0f));
  CHECK(sanitized.rimEmissionStrength == Catch::Approx(0.0f));
  CHECK(sanitized.rimPower == Catch::Approx(0.001f));
}

TEST_CASE("mesh material sanitization repairs non-finite values", "[rendering][mesh]")
{
  mesh::MeshMaterial material;
  material.baseColor = glm::vec4{std::numeric_limits<float>::quiet_NaN()};
  material.metallic = std::numeric_limits<float>::quiet_NaN();
  material.roughness = std::numeric_limits<float>::quiet_NaN();
  material.ambientOcclusion = std::numeric_limits<float>::quiet_NaN();
  material.rimOpacityStrength = std::numeric_limits<float>::quiet_NaN();
  material.rimEmissionStrength = std::numeric_limits<float>::quiet_NaN();
  material.rimPower = std::numeric_limits<float>::quiet_NaN();

  const mesh::MeshMaterial sanitized = mesh::sanitizedMaterial(material, glm::vec4{0.25f, 0.5f, 0.75f, 1.0f});

  CHECK(sanitized.baseColor == glm::vec4{0.25f, 0.5f, 0.75f, 1.0f});
  CHECK(sanitized.metallic == Catch::Approx(0.25f));
  CHECK(sanitized.roughness == Catch::Approx(0.5f));
  CHECK(sanitized.ambientOcclusion == Catch::Approx(1.0f));
  CHECK(sanitized.rimOpacityStrength == Catch::Approx(1.0f));
  CHECK(sanitized.rimEmissionStrength == Catch::Approx(1.0f));
  CHECK(sanitized.rimPower == Catch::Approx(2.0f));
}

TEST_CASE("isosurface and segmentation extraction requests build distinct geometry keys", "[rendering][mesh]")
{
  const uuids::uuid imageUid = generateRandomUuid();
  const uuids::uuid segmentationUid = generateRandomUuid();

  const mesh::IsosurfaceMeshRequest isoRequest{
    .imageUid = imageUid,
    .imageDataVersion = 3,
    .imageGeometryVersion = 4,
    .component = 2,
    .timePoint = 5,
    .isoValue = 42.5,
    .algorithm = "flying-edges",
    .algorithmVersion = 6};

  const mesh::SegmentationMeshRequest segRequest{
    .segmentationUid = segmentationUid,
    .segmentationDataVersion = 7,
    .segmentationGeometryVersion = 8,
    .labelValue = 9,
    .timePoint = 10,
    .algorithm = "marching-cubes-label",
    .algorithmVersion = 11};

  const mesh::MeshGeometryKey isoKey = mesh::geometryKeyForRequest(isoRequest);
  const mesh::MeshGeometryKey segKey = mesh::geometryKeyForRequest(segRequest);

  CHECK(isoKey.sourceUid == imageUid);
  CHECK(isoKey.sourceDataVersion == 3);
  CHECK(isoKey.sourceGeometryVersion == 4);
  REQUIRE(isoKey.component);
  CHECK(*isoKey.component == 2);
  CHECK(!isoKey.labelValue);
  CHECK(isoKey.timePoint == 5);
  CHECK(isoKey.isoValue == Catch::Approx(42.5));
  CHECK(isoKey.extractionAlgorithm == "flying-edges");
  CHECK(isoKey.extractionAlgorithmVersion == 6);

  CHECK(segKey.sourceUid == segmentationUid);
  CHECK(segKey.sourceDataVersion == 7);
  CHECK(segKey.sourceGeometryVersion == 8);
  CHECK(!segKey.component);
  REQUIRE(segKey.labelValue);
  CHECK(*segKey.labelValue == 9);
  CHECK(segKey.timePoint == 10);
  CHECK(segKey.isoValue == Catch::Approx(0.0));
  CHECK(segKey.extractionAlgorithm == "marching-cubes-label");
  CHECK(segKey.extractionAlgorithmVersion == 11);
}

TEST_CASE("isosurface mesh policy keeps raycast-only states on the raycast path", "[rendering][mesh]")
{
  CHECK(mesh::canRenderIsosurfaceWithMesh({.opacity = 1.0f, .visible = true}));

  CHECK_FALSE(mesh::canRenderIsosurfaceWithMesh({.renderWarped = true, .opacity = 1.0f, .visible = true}));
  CHECK_FALSE(mesh::canRenderIsosurfaceWithMesh({.valueEditInProgress = true, .opacity = 1.0f, .visible = true}));
  CHECK(mesh::canRenderIsosurfaceWithMesh({.opacity = 0.5f, .visible = true}));
  CHECK_FALSE(mesh::canRenderIsosurfaceWithMesh({.opacity = 1.0f, .visible = false}));
}

TEST_CASE("isosurface mesh policy uses DDP for translucent surfaces", "[rendering][mesh]")
{
  CHECK(mesh::compositingModeForIsosurfaceAlpha(1.0f) == mesh::MeshCompositingMode::Opaque);
  CHECK(mesh::compositingModeForIsosurfaceAlpha(0.999f) == mesh::MeshCompositingMode::Opaque);
  CHECK(mesh::compositingModeForIsosurfaceAlpha(0.998f) == mesh::MeshCompositingMode::AlphaOverDdp);
  CHECK(mesh::compositingModeForIsosurfaceAlpha(1.0f, true, 1.0f) == mesh::MeshCompositingMode::AlphaOverDdp);
  CHECK(mesh::compositingModeForIsosurfaceAlpha(1.0f, true, 0.0f) == mesh::MeshCompositingMode::Opaque);
  CHECK(
    mesh::compositingModeForIsosurfaceAlpha(0.5f, false, 0.0f, mesh::MeshCompositingMode::Additive) ==
    mesh::MeshCompositingMode::Additive);
}

TEST_CASE("scalar-grid isosurface policy builds stable extraction requests", "[rendering][mesh]")
{
  const uuids::uuid imageUid = generateRandomUuid();

  const mesh::IsosurfaceMeshRequest request = mesh::makeScalarGridIsosurfaceRequest(imageUid, 11, 12, 2, 4, 17.5);

  CHECK(request.imageUid == imageUid);
  CHECK(request.imageDataVersion == 11);
  CHECK(request.imageGeometryVersion == 12);
  CHECK(request.component == 2);
  CHECK(request.timePoint == 4);
  CHECK(request.isoValue == Catch::Approx(17.5));
  CHECK(request.algorithm == mesh::kScalarGridIsosurfaceAlgorithm);
  CHECK(request.algorithmVersion == mesh::kScalarGridIsosurfaceAlgorithmVersion);

  const mesh::MeshGeometryKey key = mesh::geometryKeyForRequest(request);
  CHECK(key.sourceUid == imageUid);
  CHECK(key.sourceDataVersion == 11);
  CHECK(key.sourceGeometryVersion == 12);
  REQUIRE(key.component);
  CHECK(*key.component == 2);
  CHECK(key.timePoint == 4);
  CHECK(key.isoValue == Catch::Approx(17.5));
  CHECK(key.extractionAlgorithm == mesh::kScalarGridIsosurfaceAlgorithm);
  CHECK(key.extractionAlgorithmVersion == mesh::kScalarGridIsosurfaceAlgorithmVersion);
}

TEST_CASE("segmentation mesh policy respects independent 3D visibility and opacity", "[rendering][mesh]")
{
  CHECK(mesh::shouldRenderSegmentationLabelMesh({.showMesh = true, .opacity = 1.0f}));

  CHECK_FALSE(mesh::shouldRenderSegmentationLabelMesh({.showMesh = false, .opacity = 1.0f}));
  CHECK_FALSE(mesh::shouldRenderSegmentationLabelMesh({.showMesh = true, .opacity = 0.0f}));
}

TEST_CASE("segmentation mesh style preserves label value and modulates alpha", "[rendering][mesh]")
{
  CHECK(mesh::compositingModeForLabelAlpha(1.0f) == mesh::MeshCompositingMode::Opaque);
  CHECK(mesh::compositingModeForLabelAlpha(0.999f) == mesh::MeshCompositingMode::Opaque);
  CHECK(mesh::compositingModeForLabelAlpha(0.998f) == mesh::MeshCompositingMode::AlphaOverDdp);
  CHECK(
    mesh::compositingModeForLabelAlpha(0.998f, mesh::MeshCompositingMode::Additive) ==
    mesh::MeshCompositingMode::Additive);
  CHECK(
    mesh::compositingModeForLabelAlpha(1.0f, mesh::MeshCompositingMode::Multiplicative) ==
    mesh::MeshCompositingMode::Opaque);

  const mesh::SegmentationLabelMeshStyle style = mesh::segmentationLabelMeshStyle(
    4,
    glm::vec4{0.1f, 0.2f, 0.3f, 0.5f},
    {.showMesh = true, .opacity = 0.25f},
    mesh::MeshCompositingMode::Multiplicative);

  CHECK(style.labelValue == 4);
  CHECK(style.material.baseColor.r == Catch::Approx(0.1f));
  CHECK(style.material.baseColor.g == Catch::Approx(0.2f));
  CHECK(style.material.baseColor.b == Catch::Approx(0.3f));
  CHECK(style.material.baseColor.a == Catch::Approx(0.125f));
  CHECK(style.compositingMode == mesh::MeshCompositingMode::Multiplicative);
  CHECK(style.visible);
}

TEST_CASE("scalar-grid segmentation policy builds stable extraction requests", "[rendering][mesh]")
{
  const uuids::uuid segmentationUid = generateRandomUuid();

  const mesh::SegmentationMeshRequest request = mesh::makeScalarGridSegmentationRequest(segmentationUid, 13, 14, 5, 7);

  CHECK(request.segmentationUid == segmentationUid);
  CHECK(request.segmentationDataVersion == 13);
  CHECK(request.segmentationGeometryVersion == 14);
  CHECK(request.labelValue == 5);
  CHECK(request.timePoint == 7);
  CHECK(request.algorithm == mesh::kScalarGridSegmentationAlgorithm);
  CHECK(request.algorithmVersion == mesh::kScalarGridSegmentationAlgorithmVersion);

  const mesh::MeshGeometryKey key = mesh::geometryKeyForRequest(request);
  CHECK(key.sourceUid == segmentationUid);
  CHECK(key.sourceDataVersion == 13);
  CHECK(key.sourceGeometryVersion == 14);
  REQUIRE(key.labelValue);
  CHECK(*key.labelValue == 5);
  CHECK(key.timePoint == 7);
  CHECK(key.extractionAlgorithm == mesh::kScalarGridSegmentationAlgorithm);
  CHECK(key.extractionAlgorithmVersion == mesh::kScalarGridSegmentationAlgorithmVersion);
}

TEST_CASE("mesh cache stores pending, ready, failed, stale, and evicted states", "[rendering][mesh]")
{
  const uuids::uuid imageUid = generateRandomUuid();
  mesh::MeshGeometryKey key;
  key.sourceUid = imageUid;
  key.sourceDataVersion = 1;
  key.extractionAlgorithm = "test";

  mesh::MeshCache cache;
  cache.markPending(key);

  const mesh::MeshCacheEntry* pendingEntry = cache.find(key);
  REQUIRE(pendingEntry);
  CHECK(pendingEntry->state == mesh::MeshCacheState::Pending);
  CHECK(!cache.readyMesh(key));

  mesh::MeshExtractionResult result{.key = key, .mesh = makeTriangleMesh(), .diagnostics = {"ok"}};
  cache.storeReady(std::move(result));

  const mesh::MeshCacheEntry* readyEntry = cache.find(key);
  REQUIRE(readyEntry);
  CHECK(readyEntry->state == mesh::MeshCacheState::Ready);
  REQUIRE(cache.readyMesh(key));
  CHECK(cache.readyMesh(key)->indices.size() == 3);
  CHECK(readyEntry->diagnostics == std::vector<std::string>{"ok"});

  CHECK(cache.markSourceStale(imageUid) == 1);
  const mesh::MeshCacheEntry* staleEntry = cache.find(key);
  REQUIRE(staleEntry);
  CHECK(staleEntry->state == mesh::MeshCacheState::Stale);
  CHECK(!cache.readyMesh(key));
  REQUIRE(staleEntry->mesh);

  CHECK(cache.evictSource(imageUid) == 1);
  const mesh::MeshCacheEntry* evictedEntry = cache.find(key);
  REQUIRE(evictedEntry);
  CHECK(evictedEntry->state == mesh::MeshCacheState::Evicted);
  CHECK(!evictedEntry->mesh);
}

TEST_CASE("mesh resource reconciliation releases only obsolete extracted geometry", "[rendering][mesh]")
{
  mesh::MeshGeometryKey retainedKey;
  retainedKey.sourceUid = generateRandomUuid();
  retainedKey.isoValue = 1.0;
  mesh::MeshGeometryKey obsoleteKey = retainedKey;
  obsoleteKey.isoValue = 2.0;

  mesh::MeshCache cache;
  cache.markPending(retainedKey);
  cache.markPending(obsoleteKey);
  const mesh::MeshHandle retainedHandle{.uid = generateRandomUuid(), .geometryVersion = 1};
  const mesh::MeshHandle obsoleteHandle{.uid = generateRandomUuid(), .geometryVersion = 2};
  mesh::MeshHandleMap handles{{retainedKey, retainedHandle}, {obsoleteKey, obsoleteHandle}};
  std::vector<uuids::uuid> released;

  const std::size_t removed = mesh::reconcileExtractedMeshResources(
    mesh::MeshGeometryKeySet{retainedKey},
    cache,
    handles,
    [&released](const uuids::uuid& uid) { released.push_back(uid); });

  CHECK(removed == 1u);
  CHECK(handles.contains(retainedKey));
  CHECK_FALSE(handles.contains(obsoleteKey));
  CHECK(cache.contains(retainedKey));
  CHECK_FALSE(cache.contains(obsoleteKey));
  CHECK(released == std::vector<uuids::uuid>{obsoleteHandle.uid});

  const mesh::MeshExtractionRunResult stale = mesh::applyExtractionJobResult(
    {.key = obsoleteKey, .result = mesh::MeshExtractionResult{.key = obsoleteKey, .mesh = makeTriangleMesh()}},
    cache);
  CHECK(stale.status == mesh::MeshExtractionRunStatus::Stale);
  CHECK_FALSE(cache.contains(obsoleteKey));
}

TEST_CASE("mesh cache failed entries preserve diagnostics without ready mesh data", "[rendering][mesh]")
{
  mesh::MeshGeometryKey key;
  key.sourceUid = generateRandomUuid();
  key.extractionAlgorithm = "test";

  mesh::MeshCache cache;
  cache.storeFailed(key, {"empty contour", "threshold outside data range"});

  const mesh::MeshCacheEntry* entry = cache.find(key);
  REQUIRE(entry);
  CHECK(entry->state == mesh::MeshCacheState::Failed);
  CHECK(!entry->mesh);
  CHECK(!cache.readyMesh(key));
  CHECK(entry->diagnostics == std::vector<std::string>{"empty contour", "threshold outside data range"});
}

TEST_CASE("mesh cache rejects stale async completion results", "[rendering][mesh]")
{
  mesh::MeshGeometryKey key;
  key.sourceUid = generateRandomUuid();
  key.extractionAlgorithm = "test";

  mesh::MeshCache cache;
  cache.markPending(key);
  CHECK(cache.markSourceStale(key.sourceUid) == 1);

  CHECK_FALSE(cache.storeReadyIfPending(mesh::MeshExtractionResult{.key = key, .mesh = makeTriangleMesh()}));

  const mesh::MeshCacheEntry* staleEntry = cache.find(key);
  REQUIRE(staleEntry);
  CHECK(staleEntry->state == mesh::MeshCacheState::Stale);
  CHECK(!staleEntry->mesh);

  cache.markPending(key);
  CHECK(cache.storeReadyIfPending(mesh::MeshExtractionResult{.key = key, .mesh = makeTriangleMesh()}));
  REQUIRE(cache.readyMesh(key));
}

TEST_CASE("mesh cache rejects failure completion after eviction", "[rendering][mesh]")
{
  mesh::MeshGeometryKey key;
  key.sourceUid = generateRandomUuid();
  key.extractionAlgorithm = "test";

  mesh::MeshCache cache;
  cache.markPending(key);
  CHECK(cache.evictSource(key.sourceUid) == 1);

  CHECK_FALSE(cache.storeFailedIfPending(key, {"late failure"}));

  const mesh::MeshCacheEntry* evictedEntry = cache.find(key);
  REQUIRE(evictedEntry);
  CHECK(evictedEntry->state == mesh::MeshCacheState::Evicted);
  CHECK(evictedEntry->diagnostics.empty());
}

TEST_CASE("mesh extraction queue suppresses duplicate active keys", "[rendering][mesh]")
{
  mesh::MeshGeometryKey key;
  key.sourceUid = generateRandomUuid();
  key.extractionAlgorithm = "queued-test";

  mesh::MeshExtractionQueue queue;
  CHECK(queue.submit(key, "first mesh", [key] {
    return mesh::MeshExtractionJobResult{
      .key = key,
      .result = mesh::MeshExtractionResult{.key = key, .mesh = makeTriangleMesh()}};
  }));
  CHECK_FALSE(queue.submit(key, "duplicate mesh", [key] {
    return mesh::MeshExtractionJobResult{
      .key = key,
      .result = mesh::MeshExtractionResult{.key = key, .mesh = makeTriangleMesh()}};
  }));
  CHECK(queue.active(key));
  CHECK(queue.activeCount() == 1);
  CHECK(queue.activeDescriptions() == std::vector<std::string>{"first mesh"});

  const std::vector<mesh::MeshExtractionJobResult> completed = waitForCompleted(queue);

  REQUIRE(completed.size() == 1);
  CHECK(completed.front().key == key);
  CHECK_FALSE(queue.active(key));
  CHECK(queue.activeCount() == 0);
}

TEST_CASE("mesh extraction queue respects the active job limit", "[rendering][mesh]")
{
  mesh::MeshGeometryKey firstKey;
  firstKey.sourceUid = generateRandomUuid();
  firstKey.extractionAlgorithm = "first";

  mesh::MeshGeometryKey secondKey;
  secondKey.sourceUid = generateRandomUuid();
  secondKey.extractionAlgorithm = "second";

  mesh::MeshExtractionQueue queue{1};
  CHECK(queue.maxActiveJobs() == 1);

  CHECK(queue.submit(firstKey, "first mesh", [firstKey] {
    std::this_thread::sleep_for(std::chrono::milliseconds{5});
    return mesh::MeshExtractionJobResult{
      .key = firstKey,
      .result = mesh::MeshExtractionResult{.key = firstKey, .mesh = makeTriangleMesh()}};
  }));
  CHECK_FALSE(queue.submit(secondKey, "second mesh", [secondKey] {
    return mesh::MeshExtractionJobResult{
      .key = secondKey,
      .result = mesh::MeshExtractionResult{.key = secondKey, .mesh = makeTriangleMesh()}};
  }));

  const std::vector<mesh::MeshExtractionJobResult> completed = waitForCompleted(queue);
  REQUIRE(completed.size() == 1);
  CHECK(completed.front().key == firstKey);

  CHECK(queue.submit(secondKey, "second mesh", [secondKey] {
    return mesh::MeshExtractionJobResult{
      .key = secondKey,
      .result = mesh::MeshExtractionResult{.key = secondKey, .mesh = makeTriangleMesh()}};
  }));
}

TEST_CASE("mesh extraction queue converts worker exceptions into failed results", "[rendering][mesh]")
{
  mesh::MeshGeometryKey key;
  key.sourceUid = generateRandomUuid();
  key.extractionAlgorithm = "throwing-test";

  mesh::MeshExtractionQueue queue;
  REQUIRE(queue.submit(key, "throwing mesh", []() -> mesh::MeshExtractionJobResult {
    throw std::runtime_error{"synthetic extraction failure"};
  }));

  const std::vector<mesh::MeshExtractionJobResult> completed = waitForCompleted(queue);
  REQUIRE(completed.size() == 1u);
  CHECK(completed.front().key == key);
  CHECK_FALSE(completed.front().result);
  REQUIRE(completed.front().diagnostics.size() == 1u);
  CHECK(completed.front().diagnostics.front().find("synthetic extraction failure") != std::string::npos);
}

TEST_CASE("mesh extraction queue results are applied to pending cache entries", "[rendering][mesh]")
{
  mesh::MeshGeometryKey key;
  key.sourceUid = generateRandomUuid();
  key.extractionAlgorithm = "queued-test";

  mesh::MeshCache cache;
  cache.markPending(key);

  mesh::MeshExtractionQueue queue;
  REQUIRE(queue.submit(key, "ready mesh", [key] {
    return mesh::MeshExtractionJobResult{
      .key = key,
      .result = mesh::MeshExtractionResult{.key = key, .mesh = makeTriangleMesh(), .diagnostics = {"ready"}}};
  }));

  std::vector<mesh::MeshExtractionJobResult> completed = waitForCompleted(queue);
  REQUIRE(completed.size() == 1);

  const mesh::MeshExtractionRunResult runResult = mesh::applyExtractionJobResult(std::move(completed.front()), cache);

  CHECK(runResult.status == mesh::MeshExtractionRunStatus::Ready);
  CHECK(runResult.diagnostics == std::vector<std::string>{"ready"});
  REQUIRE(cache.readyMesh(key));
}

TEST_CASE("mesh extraction queue application rejects stale or wrong-key results", "[rendering][mesh]")
{
  mesh::MeshGeometryKey key;
  key.sourceUid = generateRandomUuid();
  key.extractionAlgorithm = "queued-test";

  mesh::MeshCache cache;
  cache.markPending(key);
  CHECK(cache.markSourceStale(key.sourceUid) == 1);

  mesh::MeshExtractionJobResult lateResult{
    .key = key,
    .result = mesh::MeshExtractionResult{.key = key, .mesh = makeTriangleMesh()}};
  const mesh::MeshExtractionRunResult lateRunResult = mesh::applyExtractionJobResult(std::move(lateResult), cache);
  CHECK(lateRunResult.status == mesh::MeshExtractionRunStatus::Stale);

  cache.markPending(key);
  mesh::MeshGeometryKey wrongKey = key;
  wrongKey.sourceDataVersion = key.sourceDataVersion + 1;
  mesh::MeshExtractionJobResult wrongKeyResult{
    .key = key,
    .result = mesh::MeshExtractionResult{.key = wrongKey, .mesh = makeTriangleMesh()}};
  const mesh::MeshExtractionRunResult runResult = mesh::applyExtractionJobResult(std::move(wrongKeyResult), cache);

  CHECK(runResult.status == mesh::MeshExtractionRunStatus::Failed);
  const mesh::MeshCacheEntry* entry = cache.find(key);
  REQUIRE(entry);
  CHECK(entry->state == mesh::MeshCacheState::Failed);
  CHECK(entry->diagnostics == std::vector<std::string>{"Extractor returned a mesh for a different geometry key"});
}

TEST_CASE("isosurface extraction runner stores ready mesh results", "[rendering][mesh]")
{
  const mesh::IsosurfaceMeshRequest request{
    .imageUid = generateRandomUuid(),
    .imageDataVersion = 1,
    .imageGeometryVersion = 2,
    .component = 3,
    .timePoint = 4,
    .isoValue = 5.0,
    .algorithm = "test",
    .algorithmVersion = 6};
  const mesh::MeshGeometryKey key = mesh::geometryKeyForRequest(request);

  FakeIsosurfaceExtractor extractor;
  extractor.result = mesh::MeshExtractionResult{.key = key, .mesh = makeTriangleMesh(), .diagnostics = {"ready"}};

  mesh::MeshCache cache;
  const mesh::MeshExtractionRunResult runResult = mesh::runIsosurfaceExtraction(request, extractor, cache);

  CHECK(runResult.status == mesh::MeshExtractionRunStatus::Ready);
  CHECK(runResult.key == key);
  CHECK(runResult.diagnostics == std::vector<std::string>{"ready"});
  REQUIRE(cache.readyMesh(key));
  CHECK(cache.readyMesh(key)->indices.size() == 3);
}

TEST_CASE("isosurface extraction runner stores failure when no mesh is produced", "[rendering][mesh]")
{
  const mesh::IsosurfaceMeshRequest request{.imageUid = generateRandomUuid(), .algorithm = "test"};
  const mesh::MeshGeometryKey key = mesh::geometryKeyForRequest(request);

  FakeIsosurfaceExtractor extractor;
  extractor.result = std::nullopt;

  mesh::MeshCache cache;
  const mesh::MeshExtractionRunResult runResult = mesh::runIsosurfaceExtraction(request, extractor, cache);

  CHECK(runResult.status == mesh::MeshExtractionRunStatus::Failed);
  const mesh::MeshCacheEntry* entry = cache.find(key);
  REQUIRE(entry);
  CHECK(entry->state == mesh::MeshCacheState::Failed);
  CHECK(entry->diagnostics == std::vector<std::string>{"Extractor returned no mesh"});
}

TEST_CASE("isosurface extraction runner rejects wrong-key backend results", "[rendering][mesh]")
{
  const mesh::IsosurfaceMeshRequest request{.imageUid = generateRandomUuid(), .algorithm = "test"};
  const mesh::MeshGeometryKey key = mesh::geometryKeyForRequest(request);

  FakeIsosurfaceExtractor extractor;
  mesh::MeshGeometryKey wrongKey = key;
  wrongKey.isoValue = 99.0;
  extractor.result = mesh::MeshExtractionResult{.key = wrongKey, .mesh = makeTriangleMesh()};

  mesh::MeshCache cache;
  const mesh::MeshExtractionRunResult runResult = mesh::runIsosurfaceExtraction(request, extractor, cache);

  CHECK(runResult.status == mesh::MeshExtractionRunStatus::Failed);
  const mesh::MeshCacheEntry* entry = cache.find(key);
  REQUIRE(entry);
  CHECK(entry->state == mesh::MeshCacheState::Failed);
  CHECK(entry->diagnostics == std::vector<std::string>{"Extractor returned a mesh for a different geometry key"});
  CHECK(!cache.contains(wrongKey));
}

TEST_CASE("isosurface extraction runner discards results when request becomes stale", "[rendering][mesh]")
{
  const mesh::IsosurfaceMeshRequest request{.imageUid = generateRandomUuid(), .algorithm = "test"};
  const mesh::MeshGeometryKey key = mesh::geometryKeyForRequest(request);

  mesh::MeshCache cache;
  FakeIsosurfaceExtractor extractor;
  extractor.cache = &cache;
  extractor.staleBeforeReturning = true;
  extractor.result = mesh::MeshExtractionResult{.key = key, .mesh = makeTriangleMesh()};

  const mesh::MeshExtractionRunResult runResult = mesh::runIsosurfaceExtraction(request, extractor, cache);

  CHECK(runResult.status == mesh::MeshExtractionRunStatus::Stale);
  const mesh::MeshCacheEntry* entry = cache.find(key);
  REQUIRE(entry);
  CHECK(entry->state == mesh::MeshCacheState::Stale);
  CHECK(!entry->mesh);
}

TEST_CASE("segmentation extraction runner stores label mesh results", "[rendering][mesh]")
{
  const mesh::SegmentationMeshRequest request{
    .segmentationUid = generateRandomUuid(),
    .segmentationDataVersion = 1,
    .segmentationGeometryVersion = 2,
    .labelValue = 11,
    .timePoint = 3,
    .algorithm = "test-label",
    .algorithmVersion = 4};
  const mesh::MeshGeometryKey key = mesh::geometryKeyForRequest(request);

  FakeSegmentationExtractor extractor;
  extractor.result = mesh::MeshExtractionResult{.key = key, .mesh = makeTriangleMesh()};

  mesh::MeshCache cache;
  const mesh::MeshExtractionRunResult runResult = mesh::runSegmentationExtraction(request, extractor, cache);

  CHECK(runResult.status == mesh::MeshExtractionRunStatus::Ready);
  REQUIRE(cache.readyMesh(key));
}

TEST_CASE("scalar-grid isosurface extraction rejects invalid grids", "[rendering][mesh]")
{
  mesh::ScalarGrid3D grid;
  grid.dimensions = glm::uvec3{2, 2, 1};
  grid.values.resize(4);

  CHECK_FALSE(mesh::isValidScalarGrid(grid));
  CHECK_FALSE(mesh::generateIsoSurfaceMesh(grid, 0.5));

  grid.dimensions = glm::uvec3{2, 2, 2};
  CHECK_FALSE(mesh::isValidScalarGrid(grid));
  CHECK_FALSE(mesh::generateIsoSurfaceMesh(grid, 0.5));
}

TEST_CASE("crosshairs axis generation creates a valid cylinder and cone mesh", "[rendering][mesh][crosshairs]")
{
  const std::optional<mesh::MeshData> axis = mesh::generateCrosshairsAxisMesh();

  REQUIRE(axis);
  CHECK(mesh::isValidMeshData(*axis));
  CHECK(axis->coordinateSpace == mesh::MeshCoordinateSpace::World);
  CHECK_FALSE(axis->positions.empty());
  CHECK_FALSE(mesh::generateCrosshairsAxisMesh(0.0));
  CHECK_FALSE(mesh::generateCrosshairsAxisMesh(1.01));
}

TEST_CASE("scalar-grid isosurface extraction creates a planar surface", "[rendering][mesh]")
{
  const std::optional<mesh::MeshData> meshData = mesh::generateIsoSurfaceMesh(makePlanarScalarGrid(), 0.5);

  REQUIRE(meshData);
  CHECK(mesh::isValidMeshData(*meshData));
  CHECK(meshData->coordinateSpace == mesh::MeshCoordinateSpace::ImageSubject);
  CHECK(meshData->positions.size() == meshData->normals.size());
  CHECK_FALSE(meshData->positions.empty());

  for (const glm::vec3& position : meshData->positions) {
    CHECK(position.x == Catch::Approx(0.5f));
    CHECK(position.y >= 0.0f);
    CHECK(position.y <= 1.0f);
    CHECK(position.z >= 0.0f);
    CHECK(position.z <= 1.0f);
  }
}

TEST_CASE("scalar-grid extraction honors coordinate transforms", "[rendering][mesh]")
{
  mesh::ScalarGrid3D grid = makePlanarScalarGrid();
  grid.grid_T_voxelIndex = glm::translate(glm::mat4{1.0f}, glm::vec3{10.0f, 20.0f, 30.0f}) *
                           glm::scale(glm::mat4{1.0f}, glm::vec3{2.0f, 3.0f, 4.0f});
  grid.coordinateSpace = mesh::MeshCoordinateSpace::World;

  const std::optional<mesh::MeshData> meshData = mesh::generateIsoSurfaceMesh(grid, 0.5);

  REQUIRE(meshData);
  CHECK(meshData->coordinateSpace == mesh::MeshCoordinateSpace::World);

  for (const glm::vec3& position : meshData->positions) {
    CHECK(position.x == Catch::Approx(11.0f));
    CHECK(position.y >= 20.0f);
    CHECK(position.y <= 23.0f);
    CHECK(position.z >= 30.0f);
    CHECK(position.z <= 34.0f);
  }
}

TEST_CASE("image component adapter creates scalar grids with image geometry", "[rendering][mesh]")
{
  const Image image = makeMeshScalarImage();
  const std::optional<mesh::ScalarGrid3D> grid = mesh::scalarGridFromImageComponent(image, 0);

  REQUIRE(grid);
  CHECK(grid->dimensions == glm::uvec3{2, 2, 2});
  CHECK(grid->coordinateSpace == mesh::MeshCoordinateSpace::ImageSubject);
  CHECK(grid->values[mesh::scalarGridValueIndex(grid->dimensions, 1, 1, 1)] == Catch::Approx(111.0f));

  const glm::vec4 subjectPoint = grid->grid_T_voxelIndex * glm::vec4{1.0f, 1.0f, 1.0f, 1.0f};
  requireVec3(glm::vec3{subjectPoint}, 7.0f, 22.0f, 34.0f);

  CHECK_FALSE(mesh::scalarGridFromImageComponent(image, 1));
}

TEST_CASE("image label adapter preserves exact 32-bit label identity", "[rendering][mesh]")
{
  constexpr int64_t targetLabel = 16'777'217;
  const Image image = makeMeshLabelImage();
  const std::optional<mesh::ScalarGrid3D> grid = mesh::labelMaskGridFromImageComponent(image, 0, targetLabel);

  REQUIRE(grid);
  CHECK(grid->values[mesh::scalarGridValueIndex(grid->dimensions, 0, 0, 0)] == 1.0f);
  CHECK(grid->values[mesh::scalarGridValueIndex(grid->dimensions, 1, 0, 0)] == 0.0f);
  REQUIRE(mesh::generateLabelMesh(*grid, 1));
}

TEST_CASE("scalar-grid segmentation extraction creates the requested label surface", "[rendering][mesh]")
{
  const std::optional<mesh::MeshData> meshData = mesh::generateLabelMesh(makeBinaryLabelGrid(), 7);

  REQUIRE(meshData);
  CHECK(mesh::isValidMeshData(*meshData));

  for (const glm::vec3& position : meshData->positions) {
    CHECK(position.x == Catch::Approx(0.5f));
  }

  CHECK_FALSE(mesh::generateLabelMesh(makeBinaryLabelGrid(), 99));
}

TEST_CASE("mesh cube primitive creates valid flat-shaded geometry", "[rendering][mesh]")
{
  const mesh::MeshData cube = mesh::makeCubeMesh(4.0f);

  CHECK(mesh::isValidMeshData(cube));
  CHECK(cube.positions.size() == 24);
  CHECK(cube.normals.size() == 24);
  CHECK(cube.indices.size() == 36);
  CHECK(cube.coordinateSpace == mesh::MeshCoordinateSpace::World);

  const std::optional<mesh::MeshBounds> bounds = mesh::computeBounds(cube);
  REQUIRE(bounds);
  requireVec3(bounds->min, -2.0f, -2.0f, -2.0f);
  requireVec3(bounds->max, 2.0f, 2.0f, 2.0f);
}

TEST_CASE("mesh sphere primitive creates valid smooth glyph geometry", "[rendering][mesh]")
{
  const mesh::MeshData sphere = mesh::makeSphereMesh(3.0f, 4, 8);

  CHECK(mesh::isValidMeshData(sphere));
  CHECK(sphere.positions.size() == 34);
  CHECK(sphere.normals.size() == sphere.positions.size());
  CHECK(sphere.indices.size() == 192);
  CHECK(sphere.coordinateSpace == mesh::MeshCoordinateSpace::World);

  const std::optional<mesh::MeshBounds> bounds = mesh::computeBounds(sphere);
  REQUIRE(bounds);
  CHECK(bounds->min.z == Catch::Approx(-3.0f));
  CHECK(bounds->max.z == Catch::Approx(3.0f));
}

TEST_CASE("mesh cylinder primitive creates valid z-axis glyph geometry", "[rendering][mesh]")
{
  const mesh::MeshData cylinder = mesh::makeCylinderMesh(2.0f, 5.0f, 12);

  CHECK(mesh::isValidMeshData(cylinder));
  CHECK(cylinder.positions.size() == 50);
  CHECK(cylinder.normals.size() == cylinder.positions.size());
  CHECK(cylinder.indices.size() == 144);
  CHECK(cylinder.coordinateSpace == mesh::MeshCoordinateSpace::World);

  const std::optional<mesh::MeshBounds> bounds = mesh::computeBounds(cylinder);
  REQUIRE(bounds);
  CHECK(bounds->min.z == Catch::Approx(-2.5f));
  CHECK(bounds->max.z == Catch::Approx(2.5f));
}

TEST_CASE("image plane mesh creates a valid world-space quad", "[rendering][mesh]")
{
  const mesh::MeshImagePlane plane{
    .centerWorld = glm::vec3{10.0f, 20.0f, 30.0f},
    .uDirectionWorld = glm::vec3{2.0f, 0.0f, 0.0f},
    .vDirectionWorld = glm::vec3{0.5f, 3.0f, 0.0f},
    .sizeWorld = glm::vec2{4.0f, 6.0f}};

  const std::optional<mesh::MeshData> meshData = mesh::makeImagePlaneMesh(plane);

  REQUIRE(meshData);
  CHECK(mesh::isValidMeshData(*meshData));
  CHECK(meshData->positions.size() == 4);
  CHECK(meshData->normals.size() == 4);
  CHECK(meshData->indices == std::vector<uint32_t>{0, 1, 2, 0, 2, 3});
  CHECK(meshData->coordinateSpace == mesh::MeshCoordinateSpace::World);

  requireVec3(meshData->positions[0], 8.0f, 17.0f, 30.0f);
  requireVec3(meshData->positions[2], 12.0f, 23.0f, 30.0f);
  requireVec3(meshData->normals[0], 0.0f, 0.0f, 1.0f);
}

TEST_CASE("image plane texture coordinates are derived from world positions", "[rendering][mesh]")
{
  const mesh::MeshImagePlane plane{
    .centerWorld = glm::vec3{10.0f, 20.0f, 30.0f},
    .uDirectionWorld = glm::vec3{1.0f, 0.0f, 0.0f},
    .vDirectionWorld = glm::vec3{0.0f, 1.0f, 0.0f},
    .sizeWorld = glm::vec2{4.0f, 6.0f}};
  const glm::mat4 texture_T_world = glm::scale(glm::mat4{1.0f}, glm::vec3{0.5f, 0.25f, 0.125f}) *
                                    glm::translate(glm::mat4{1.0f}, glm::vec3{-8.0f, -17.0f, -30.0f});

  const std::optional<mesh::MeshData> maybeMesh = mesh::makeTexturedImagePlaneMesh(plane, texture_T_world);

  REQUIRE(maybeMesh);
  const mesh::MeshData& meshData = *maybeMesh;
  REQUIRE(meshData.textureCoords);
  REQUIRE(meshData.textureCoords->size() == meshData.positions.size());
  requireVec3(meshData.textureCoords->at(0), 0.0f, 0.0f, 0.0f);
  requireVec3(meshData.textureCoords->at(2), 2.0f, 1.5f, 0.0f);
}

TEST_CASE("image plane renderables keep texture binding state separate from material meshes", "[rendering][mesh]")
{
  const uuids::uuid meshUid = generateRandomUuid();
  const uuids::uuid imageUid = generateRandomUuid();
  const mesh::MeshHandle handle{.uid = meshUid, .geometryVersion = 17};
  const mesh::MeshImagePlaneTexture texture{.imageUid = imageUid, .component = 2, .timePoint = 3};
  const glm::mat4 world_T_mesh = glm::translate(glm::mat4{1.0f}, glm::vec3{4.0f, 5.0f, 6.0f});
  const glm::vec3 centerWorld{7.0f, 8.0f, 9.0f};

  const mesh::MeshImagePlaneRenderable renderable =
    mesh::makeImagePlaneRenderable(handle, world_T_mesh, centerWorld, texture, 0.75f, false, true);

  CHECK(renderable.mesh.uid == meshUid);
  CHECK(renderable.mesh.geometryVersion == 17);
  CHECK(renderable.texture.imageUid == imageUid);
  CHECK(renderable.texture.component == 2);
  CHECK(renderable.texture.timePoint == 3);
  CHECK(renderable.world_T_mesh == world_T_mesh);
  CHECK(renderable.centerWorld == centerWorld);
  CHECK(renderable.opacityMultiplier == Catch::Approx(0.75f));
  CHECK_FALSE(renderable.shadingEnabled);
  CHECK(mesh::isDrawableImagePlaneRenderable(renderable));

  mesh::MeshImagePlaneRenderable transparent = renderable;
  transparent.opacityMultiplier = 0.0f;
  CHECK_FALSE(mesh::isDrawableImagePlaneRenderable(transparent));

  mesh::MeshImagePlaneRenderable hidden = renderable;
  hidden.visible = false;
  CHECK_FALSE(mesh::isDrawableImagePlaneRenderable(hidden));

  mesh::MeshImagePlaneRenderable missingMesh = renderable;
  missingMesh.mesh.uid = {};
  CHECK_FALSE(mesh::isDrawableImagePlaneRenderable(missingMesh));

  mesh::MeshImagePlaneRenderable missingImage = renderable;
  missingImage.texture.imageUid = {};
  CHECK_FALSE(mesh::isDrawableImagePlaneRenderable(missingImage));
}

TEST_CASE("image plane render list filters non-drawable image planes", "[rendering][mesh]")
{
  const uuids::uuid meshUid = generateRandomUuid();
  const uuids::uuid imageUid = generateRandomUuid();

  mesh::MeshImagePlaneRenderable drawable = mesh::makeImagePlaneRenderable(
    mesh::MeshHandle{.uid = meshUid, .geometryVersion = 1},
    glm::mat4{1.0f},
    glm::vec3{0.0f},
    mesh::MeshImagePlaneTexture{.imageUid = imageUid});
  mesh::MeshImagePlaneRenderable hidden = drawable;
  hidden.visible = false;
  mesh::MeshImagePlaneRenderable missingImage = drawable;
  missingImage.texture.imageUid = {};

  const std::vector imagePlanes{hidden, drawable, missingImage};
  const mesh::MeshImagePlaneRenderList list = mesh::buildImagePlaneRenderList(imagePlanes);

  REQUIRE(list.imagePlanes.size() == 1);
  CHECK(&list.imagePlanes.front().get() == &imagePlanes[1]);
  CHECK(mesh::visibleImagePlaneCount(list) == 1);
}

TEST_CASE("image plane DDP depth bias follows bottom-to-top image order", "[rendering][mesh][ddp]")
{
  using Orientation = mesh::MeshImagePlaneOrientation;
  CHECK(mesh::imagePlaneDdpDepthBias(0u, Orientation::Axial) == Catch::Approx(0.0f));
  CHECK(
    mesh::imagePlaneDdpDepthBias(0u, Orientation::Coronal) == Catch::Approx(mesh::k_imagePlaneDdpDepthBiasPerSurface));
  CHECK(
    mesh::imagePlaneDdpDepthBias(0u, Orientation::Sagittal) > mesh::imagePlaneDdpDepthBias(0u, Orientation::Coronal));
  CHECK(mesh::imagePlaneDdpDepthBias(1u, Orientation::Axial) > mesh::imagePlaneDdpDepthBias(0u, Orientation::Sagittal));
}

TEST_CASE("image plane borders are hidden with their source image", "[rendering][mesh]")
{
  CHECK(mesh::imagePlaneBorderOpacity(true, 1.0f) == Catch::Approx(1.0f));
  CHECK(mesh::imagePlaneBorderOpacity(true, 0.25f) == Catch::Approx(1.0f));
  CHECK(mesh::imagePlaneBorderOpacity(true, 0.0f) == Catch::Approx(0.0f));
  CHECK(mesh::imagePlaneBorderOpacity(false, 1.0f) == Catch::Approx(0.0f));
  CHECK(mesh::imagePlaneBorderOpacity(true, 1.0f, 0.25f) == Catch::Approx(0.25f));
  CHECK(mesh::imagePlaneBorderOpacity(true, 1.0f, 0.0f) == Catch::Approx(0.0f));
}

TEST_CASE("orthogonal image plane scene meshes are clipped to the image box", "[rendering][mesh]")
{
  const std::array<glm::vec3, 8> boxCorners{
    glm::vec3{0.0f, 0.0f, 0.0f},
    glm::vec3{1.0f, 0.0f, 0.0f},
    glm::vec3{0.0f, 1.0f, 0.0f},
    glm::vec3{1.0f, 1.0f, 0.0f},
    glm::vec3{0.0f, 0.0f, 1.0f},
    glm::vec3{1.0f, 0.0f, 1.0f},
    glm::vec3{0.0f, 1.0f, 1.0f},
    glm::vec3{1.0f, 1.0f, 1.0f}};

  const mesh::MeshImagePlaneSceneInputs inputs{
    .worldCrosshairs = glm::vec3{0.5f},
    .world_T_pixel = glm::mat4{1.0f},
    .pixel_T_world = glm::mat4{1.0f},
    .texture_T_world = glm::mat4{1.0f},
    .pixelBoxCorners = boxCorners,
    .orientations = {
      mesh::MeshImagePlaneOrientation::Axial,
      mesh::MeshImagePlaneOrientation::Coronal,
      mesh::MeshImagePlaneOrientation::Sagittal}};

  const std::vector<mesh::MeshImagePlaneSceneMesh> planes = mesh::buildOrthogonalImagePlaneSceneMeshes(inputs);

  REQUIRE(planes.size() == 3);
  for (const mesh::MeshImagePlaneSceneMesh& plane : planes) {
    CHECK_FALSE(plane.mesh.positions.empty());
    CHECK(plane.mesh.textureCoords.has_value());
    REQUIRE(plane.mesh.textureCoords->size() == plane.mesh.positions.size());

    for (const glm::vec3& position : plane.mesh.positions) {
      CHECK(position.x >= 0.0f);
      CHECK(position.x <= 1.0f);
      CHECK(position.y >= 0.0f);
      CHECK(position.y <= 1.0f);
      CHECK(position.z >= 0.0f);
      CHECK(position.z <= 1.0f);
      switch (plane.orientation) {
        case mesh::MeshImagePlaneOrientation::Axial:
          CHECK(position.z == inputs.worldCrosshairs.z);
          break;
        case mesh::MeshImagePlaneOrientation::Coronal:
          CHECK(position.y == inputs.worldCrosshairs.y);
          break;
        case mesh::MeshImagePlaneOrientation::Sagittal:
          CHECK(position.x == inputs.worldCrosshairs.x);
          break;
      }
    }

    for (const glm::vec3& texCoord : *plane.mesh.textureCoords) {
      CHECK(texCoord.x >= 0.0f);
      CHECK(texCoord.x <= 1.0f);
      CHECK(texCoord.y >= 0.0f);
      CHECK(texCoord.y <= 1.0f);
      CHECK(texCoord.z >= 0.0f);
      CHECK(texCoord.z <= 1.0f);
    }
  }
}

TEST_CASE("orthogonal image plane scene can build boundary meshes", "[rendering][mesh]")
{
  const std::array<glm::vec3, 8> boxCorners{
    glm::vec3{0.0f, 0.0f, 0.0f},
    glm::vec3{1.0f, 0.0f, 0.0f},
    glm::vec3{0.0f, 1.0f, 0.0f},
    glm::vec3{1.0f, 1.0f, 0.0f},
    glm::vec3{0.0f, 0.0f, 1.0f},
    glm::vec3{1.0f, 0.0f, 1.0f},
    glm::vec3{0.0f, 1.0f, 1.0f},
    glm::vec3{1.0f, 1.0f, 1.0f}};

  const mesh::MeshImagePlaneSceneInputs inputs{
    .worldCrosshairs = glm::vec3{0.5f},
    .world_T_pixel = glm::mat4{1.0f},
    .pixel_T_world = glm::mat4{1.0f},
    .texture_T_world = glm::mat4{1.0f},
    .pixelBoxCorners = boxCorners,
    .orientations = {mesh::MeshImagePlaneOrientation::Axial},
    .borderWidthWorld = 0.05f};

  const std::vector<mesh::MeshImagePlaneSceneMesh> planes = mesh::buildOrthogonalImagePlaneSceneMeshes(inputs);

  REQUIRE(planes.size() == 1);
  REQUIRE(planes.front().borderMesh.has_value());
  CHECK(planes.front().borderMesh->positions.size() == 8u);
  CHECK(planes.front().borderMesh->indices.size() == 24u);
}

TEST_CASE("image box border mesh builds all twelve box edges", "[rendering][mesh]")
{
  const std::array<glm::vec3, 8> boxCorners{
    glm::vec3{0.0f, 0.0f, 0.0f},
    glm::vec3{2.0f, 0.0f, 0.0f},
    glm::vec3{0.0f, 3.0f, 0.0f},
    glm::vec3{2.0f, 3.0f, 0.0f},
    glm::vec3{0.0f, 0.0f, 4.0f},
    glm::vec3{2.0f, 0.0f, 4.0f},
    glm::vec3{0.0f, 3.0f, 4.0f},
    glm::vec3{2.0f, 3.0f, 4.0f}};

  const std::optional<mesh::MeshData> mesh = mesh::makeImageBoxBorderMesh(boxCorners, 0.1f);

  REQUIRE(mesh.has_value());
  CHECK(mesh->positions.size() == static_cast<std::size_t>(12u) * 8u);
  CHECK(mesh->normals.size() == mesh->positions.size());
  CHECK(mesh->indices.size() == static_cast<std::size_t>(12u) * 36u);
}

TEST_CASE("image plane orientation opacity follows legacy auto-hiding behavior", "[rendering][mesh]")
{
  CHECK(
    mesh::imagePlaneViewOpacityMultiplier(glm::vec3{0.0f, 0.0f, 1.0f}, glm::vec3{0.0f, 0.0f, 2.0f}) ==
    Catch::Approx(1.0f));
  CHECK(
    mesh::imagePlaneViewOpacityMultiplier(glm::vec3{0.0f, 0.0f, 1.0f}, glm::vec3{0.0f, 2.0f, 0.0f}) ==
    Catch::Approx(0.0f));
  CHECK(mesh::imagePlaneViewOpacityMultiplier(glm::vec3{0.0f}, glm::vec3{0.0f, 0.0f, 1.0f}) == Catch::Approx(0.0f));
}

TEST_CASE("orthogonal image plane scene omits planes outside the image box", "[rendering][mesh]")
{
  const std::array<glm::vec3, 8> boxCorners{
    glm::vec3{0.0f, 0.0f, 0.0f},
    glm::vec3{1.0f, 0.0f, 0.0f},
    glm::vec3{0.0f, 1.0f, 0.0f},
    glm::vec3{1.0f, 1.0f, 0.0f},
    glm::vec3{0.0f, 0.0f, 1.0f},
    glm::vec3{1.0f, 0.0f, 1.0f},
    glm::vec3{0.0f, 1.0f, 1.0f},
    glm::vec3{1.0f, 1.0f, 1.0f}};

  const mesh::MeshImagePlaneSceneInputs inputs{
    .worldCrosshairs = glm::vec3{2.0f},
    .world_T_pixel = glm::mat4{1.0f},
    .pixel_T_world = glm::mat4{1.0f},
    .texture_T_world = glm::mat4{1.0f},
    .pixelBoxCorners = boxCorners,
    .orientations = {
      mesh::MeshImagePlaneOrientation::Axial,
      mesh::MeshImagePlaneOrientation::Coronal,
      mesh::MeshImagePlaneOrientation::Sagittal}};

  CHECK(mesh::buildOrthogonalImagePlaneSceneMeshes(inputs).empty());
}

TEST_CASE("image plane presets follow LPS view conventions", "[rendering][mesh]")
{
  const glm::vec3 center{1.0f, 2.0f, 3.0f};
  const glm::vec2 size{4.0f, 6.0f};

  const mesh::MeshImagePlane axial = mesh::makeAxialImagePlane(center, size);
  const mesh::MeshImagePlane coronal = mesh::makeCoronalImagePlane(center, size);
  const mesh::MeshImagePlane sagittal = mesh::makeSagittalImagePlane(center, size);

  requireVec3(axial.centerWorld, 1.0f, 2.0f, 3.0f);
  requireVec3(axial.uDirectionWorld, -1.0f, 0.0f, 0.0f);
  requireVec3(axial.vDirectionWorld, 0.0f, 1.0f, 0.0f);

  requireVec3(coronal.uDirectionWorld, -1.0f, 0.0f, 0.0f);
  requireVec3(coronal.vDirectionWorld, 0.0f, 0.0f, 1.0f);

  requireVec3(sagittal.uDirectionWorld, 0.0f, -1.0f, 0.0f);
  requireVec3(sagittal.vDirectionWorld, 0.0f, 0.0f, 1.0f);
}

TEST_CASE("image plane mesh rejects degenerate geometry", "[rendering][mesh]")
{
  mesh::MeshImagePlane plane;
  plane.sizeWorld = glm::vec2{0.0f, 1.0f};
  CHECK_FALSE(mesh::makeImagePlaneMesh(plane));

  plane.sizeWorld = glm::vec2{1.0f, 1.0f};
  plane.uDirectionWorld = glm::vec3{0.0f};
  CHECK_FALSE(mesh::makeImagePlaneMesh(plane));

  plane.uDirectionWorld = glm::vec3{1.0f, 0.0f, 0.0f};
  plane.vDirectionWorld = glm::vec3{2.0f, 0.0f, 0.0f};
  CHECK_FALSE(mesh::makeImagePlaneMesh(plane));
}

TEST_CASE("image slice intersection mesh removes duplicate polygon vertices", "[rendering][mesh]")
{
  intersection::IntersectionVertices intersections{
    glm::vec3{1.0f, 0.0f, 0.0f},
    glm::vec3{1.0f, 0.0f, 0.0f},
    glm::vec3{0.0f, 1.0f, 0.0f},
    glm::vec3{0.0f, 1.0f, 0.0f},
    glm::vec3{0.0f, 0.0f, 0.0f},
    glm::vec3{0.0f, 0.0f, 0.0f},
    glm::vec3{1.0f / 3.0f, 1.0f / 3.0f, 0.0f}};

  const std::optional<mesh::MeshData> maybeMesh = mesh::makeImageSliceIntersectionMesh(intersections);

  REQUIRE(maybeMesh);
  const mesh::MeshData& meshData = *maybeMesh;
  CHECK(meshData.coordinateSpace == mesh::MeshCoordinateSpace::World);
  REQUIRE(meshData.positions.size() == 4u);
  REQUIRE(meshData.normals.size() == 4u);
  REQUIRE(meshData.indices.size() == 9u);
  CHECK(meshData.positions.front() == intersections.back());
  CHECK(meshData.indices == std::vector<uint32_t>{0u, 1u, 2u, 0u, 2u, 3u, 0u, 3u, 1u});
}

TEST_CASE("textured image slice intersection mesh maps every fan vertex to texture space", "[rendering][mesh]")
{
  intersection::IntersectionVertices intersections{
    glm::vec3{1.0f, 0.0f, 0.0f},
    glm::vec3{1.0f, 0.0f, 0.0f},
    glm::vec3{0.0f, 1.0f, 0.0f},
    glm::vec3{0.0f, 1.0f, 0.0f},
    glm::vec3{0.0f, 0.0f, 0.0f},
    glm::vec3{0.0f, 0.0f, 0.0f},
    glm::vec3{1.0f / 3.0f, 1.0f / 3.0f, 0.0f}};
  const glm::mat4 texture_T_world = glm::translate(glm::mat4{1.0f}, glm::vec3{0.25f, 0.5f, 0.75f});

  const std::optional<mesh::MeshData> maybeMesh =
    mesh::makeTexturedImageSliceIntersectionMesh(intersections, texture_T_world);

  REQUIRE(maybeMesh);
  const mesh::MeshData& meshData = *maybeMesh;
  REQUIRE(meshData.textureCoords);
  REQUIRE(meshData.textureCoords->size() == meshData.positions.size());
  requireVec3(meshData.textureCoords->front(), 1.0f / 3.0f + 0.25f, 1.0f / 3.0f + 0.5f, 0.75f);
  requireVec3(meshData.textureCoords->at(1), 1.25f, 0.5f, 0.75f);
}

TEST_CASE("image slice intersection mesh supports homogeneous coordinates", "[rendering][mesh]")
{
  intersection::IntersectionVerticesVec4 intersections{
    glm::vec4{-1.0f, -1.0f, 0.0f, 2.0f},
    glm::vec4{1.0f, -1.0f, 0.0f, 2.0f},
    glm::vec4{1.0f, 1.0f, 0.0f, 2.0f},
    glm::vec4{-1.0f, 1.0f, 0.0f, 2.0f},
    glm::vec4{-1.0f, 1.0f, 0.0f, 2.0f},
    glm::vec4{-1.0f, -1.0f, 0.0f, 2.0f},
    glm::vec4{0.0f, 0.0f, 0.0f, 1.0f}};

  const std::optional<mesh::MeshData> maybeMesh = mesh::makeImageSliceIntersectionMesh(intersections);

  REQUIRE(maybeMesh);
  const mesh::MeshData& meshData = *maybeMesh;
  REQUIRE(meshData.positions.size() == 5u);
  CHECK(meshData.positions[1] == glm::vec3{-0.5f, -0.5f, 0.0f});
  CHECK(meshData.positions[2] == glm::vec3{0.5f, -0.5f, 0.0f});
  CHECK(meshData.indices.size() == 12u);
}

TEST_CASE(
  "image slice intersection mesh rejects degenerate polygons and invalid homogeneous points",
  "[rendering][mesh]")
{
  intersection::IntersectionVertices degenerate{};
  degenerate.fill(glm::vec3{1.0f, 2.0f, 3.0f});
  CHECK_FALSE(mesh::makeImageSliceIntersectionMesh(degenerate));

  intersection::IntersectionVerticesVec4 invalidHomogeneous{};
  invalidHomogeneous.fill(glm::vec4{1.0f, 2.0f, 3.0f, 1.0f});
  invalidHomogeneous[2].w = 0.0f;
  CHECK_FALSE(mesh::makeImageSliceIntersectionMesh(invalidHomogeneous));

  const std::optional<mesh::MeshData> planeMesh = mesh::makeImagePlaneMesh(mesh::MeshImagePlane{});
  REQUIRE(planeMesh);
  glm::mat4 invalidTextureTransform{1.0f};
  invalidTextureTransform[3][3] = 0.0f;
  CHECK_FALSE(mesh::withImageTextureCoordinates(*planeMesh, invalidTextureTransform));
}

TEST_CASE("mesh view viewport maps window clip rectangles into device pixels", "[rendering][mesh]")
{
  const glm::vec4 windowDeviceViewport{0.0f, 0.0f, 1600.0f, 1200.0f};

  CHECK(
    mesh::meshViewDeviceViewport(glm::vec4{-1.0f, -1.0f, 2.0f, 2.0f}, windowDeviceViewport) ==
    glm::ivec4{0, 0, 1600, 1200});
  CHECK(
    mesh::meshViewDeviceViewport(glm::vec4{-1.0f, -1.0f, 1.0f, 2.0f}, windowDeviceViewport) ==
    glm::ivec4{0, 0, 800, 1200});
  CHECK(
    mesh::meshViewDeviceViewport(glm::vec4{0.0f, 0.0f, 1.0f, 1.0f}, windowDeviceViewport) ==
    glm::ivec4{800, 600, 800, 600});
}

TEST_CASE("mesh view viewport includes window device offset", "[rendering][mesh]")
{
  const glm::vec4 windowDeviceViewport{20.0f, 40.0f, 1600.0f, 1200.0f};

  CHECK(
    mesh::meshViewDeviceViewport(glm::vec4{-0.5f, -0.5f, 1.0f, 1.0f}, windowDeviceViewport) ==
    glm::ivec4{420, 340, 800, 600});
}
