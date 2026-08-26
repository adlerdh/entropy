#include "rendering/helpers/TextureSetupHelpers.h"

#include <catch2/catch_test_macros.hpp>

namespace texture_setup = rendering::texture_setup;
using TextureDimension = rendering::TextureDimension;

TEST_CASE("texture setup helpers classify non-singleton image axes", "[rendering][texture-setup]")
{
  REQUIRE(texture_setup::nonSingletonAxes(glm::uvec3{1, 4, 1}) == std::vector<int>{1});
  REQUIRE(texture_setup::nonSingletonAxes(glm::uvec3{1, 4, 8}) == std::vector<int>{1, 2});
  REQUIRE(texture_setup::nonSingletonAxes(glm::uvec3{2, 4, 8}) == std::vector<int>{0, 1, 2});
}

TEST_CASE("texture setup helpers choose 3D upload when dimensions fit", "[rendering][texture-setup]")
{
  const texture_setup::TextureLimits limits{.maxTextureSize = 8192, .max3DTextureSize = 2048};
  const auto layout = texture_setup::textureUploadLayoutForImage(glm::uvec3{512, 1024, 128}, limits);

  REQUIRE(layout);
  REQUIRE(layout->layout.dimension == TextureDimension::Texture3D);
  REQUIRE(layout->uploadSize == glm::uvec3{512, 1024, 128});
}

TEST_CASE("texture setup helpers choose 2D upload for oversized planar images", "[rendering][texture-setup]")
{
  const texture_setup::TextureLimits limits{.maxTextureSize = 8192, .max3DTextureSize = 2048};
  const auto xy = texture_setup::textureUploadLayoutForImage(glm::uvec3{3072, 1536, 1}, limits);
  const auto xz = texture_setup::textureUploadLayoutForImage(glm::uvec3{3072, 1, 1536}, limits);
  const auto yz = texture_setup::textureUploadLayoutForImage(glm::uvec3{1, 3072, 1536}, limits);

  REQUIRE(xy);
  CHECK(xy->layout.dimension == TextureDimension::Texture2D);
  CHECK(xy->layout.axes == glm::ivec2{0, 1});
  CHECK(xy->uploadSize == glm::uvec3{3072, 1536, 1});
  REQUIRE(xz);
  CHECK(xz->layout.axes == glm::ivec2{0, 2});
  CHECK(xz->uploadSize == glm::uvec3{3072, 1536, 1});
  REQUIRE(yz);
  CHECK(yz->layout.axes == glm::ivec2{1, 2});
  CHECK(yz->uploadSize == glm::uvec3{3072, 1536, 1});
}

TEST_CASE("texture setup helpers reject oversized non-planar images", "[rendering][texture-setup]")
{
  const texture_setup::TextureLimits limits{.maxTextureSize = 8192, .max3DTextureSize = 2048};
  REQUIRE_FALSE(texture_setup::textureUploadLayoutForImage(glm::uvec3{4096, 4096, 2}, limits));
  REQUIRE_FALSE(texture_setup::textureUploadLayoutForImage(glm::uvec3{4096, 1, 1}, limits));
}

TEST_CASE("texture setup helpers reject planar images that exceed 2D limits", "[rendering][texture-setup]")
{
  const texture_setup::TextureLimits limits{.maxTextureSize = 4096, .max3DTextureSize = 2048};
  REQUIRE_FALSE(texture_setup::textureUploadLayoutForImage(glm::uvec3{1, 8192, 1536}, limits));
}

TEST_CASE("texture setup helpers reject empty dimensions and invalid GPU limits", "[rendering][texture-setup]")
{
  const texture_setup::TextureLimits valid{.maxTextureSize = 8192, .max3DTextureSize = 2048};
  CHECK_FALSE(texture_setup::textureUploadLayoutForImage(glm::uvec3{0, 64, 64}, valid));
  CHECK_FALSE(texture_setup::textureUploadLayoutForImage(
    glm::uvec3{64},
    texture_setup::TextureLimits{.maxTextureSize = 8192, .max3DTextureSize = 0}));
  CHECK_FALSE(texture_setup::textureUploadLayoutForImage(
    glm::uvec3{4096, 4096, 1},
    texture_setup::TextureLimits{.maxTextureSize = 0, .max3DTextureSize = 2048}));
}

TEST_CASE("texture setup helpers map texture update regions", "[rendering][texture-setup]")
{
  const auto volume = texture_setup::textureUploadRegion(
    {.dimension = TextureDimension::Texture3D},
    {100, 80, 60},
    {10, 20, 30},
    {40, 30, 20});
  const auto xy = texture_setup::textureUploadRegion(
    {.dimension = TextureDimension::Texture2D, .axes = {0, 1}},
    {100, 80, 1},
    {10, 20, 0},
    {30, 40, 1});
  const auto xz = texture_setup::textureUploadRegion(
    {.dimension = TextureDimension::Texture2D, .axes = {0, 2}},
    {100, 1, 80},
    {10, 0, 20},
    {30, 1, 40});
  const auto yz = texture_setup::textureUploadRegion(
    {.dimension = TextureDimension::Texture2D, .axes = {1, 2}},
    {1, 100, 80},
    {0, 10, 20},
    {1, 30, 40});

  REQUIRE(volume);
  CHECK(volume->offset == glm::uvec3{10, 20, 30});
  CHECK(volume->size == glm::uvec3{40, 30, 20});
  REQUIRE(xy);
  CHECK(xy->offset == glm::uvec3{10, 20, 0});
  CHECK(xy->size == glm::uvec3{30, 40, 1});
  REQUIRE(xz);
  CHECK(xz->offset == glm::uvec3{10, 20, 0});
  CHECK(xz->size == glm::uvec3{30, 40, 1});
  REQUIRE(yz);
  CHECK(yz->offset == glm::uvec3{10, 20, 0});
  CHECK(yz->size == glm::uvec3{30, 40, 1});
}

TEST_CASE("texture setup helpers reject invalid update regions", "[rendering][texture-setup]")
{
  const rendering::PlanarTextureLayout xy{.dimension = TextureDimension::Texture2D, .axes = {0, 1}};
  CHECK_FALSE(texture_setup::textureUploadRegion(
    {.dimension = TextureDimension::Texture2D, .axes = {1, 1}},
    {100, 80, 1},
    {0, 0, 0},
    {1, 1, 1}));
  CHECK_FALSE(texture_setup::textureUploadRegion(xy, {100, 80, 2}, {0, 0, 0}, {1, 1, 1}));
  CHECK_FALSE(texture_setup::textureUploadRegion(xy, {100, 80, 1}, {0, 0, 0}, {1, 1, 0}));
  CHECK_FALSE(texture_setup::textureUploadRegion(xy, {100, 80, 1}, {90, 0, 0}, {11, 1, 1}));
  CHECK_FALSE(texture_setup::textureUploadRegion(
    {.dimension = TextureDimension::Texture3D},
    {100, 80, 60},
    {90, 0, 0},
    {11, 1, 1}));
  CHECK_FALSE(texture_setup::textureUploadRegion(
    {.dimension = TextureDimension::Texture3D},
    {100, 80, 60},
    {0, 0, 0},
    {1, 0, 1}));
}
