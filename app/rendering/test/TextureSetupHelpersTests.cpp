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
  const auto layout = texture_setup::textureUploadLayoutForImage(glm::uvec3{1, 3072, 1536}, limits);

  REQUIRE(layout);
  REQUIRE(layout->layout.dimension == TextureDimension::Texture2D);
  REQUIRE(layout->layout.axes == glm::ivec2{1, 2});
  REQUIRE(layout->uploadSize == glm::uvec3{3072, 1536, 1});
}

TEST_CASE("texture setup helpers reject oversized non-planar images", "[rendering][texture-setup]")
{
  const texture_setup::TextureLimits limits{.maxTextureSize = 8192, .max3DTextureSize = 2048};
  REQUIRE_FALSE(texture_setup::textureUploadLayoutForImage(glm::uvec3{4096, 4096, 2}, limits));
}

TEST_CASE("texture setup helpers reject planar images that exceed 2D limits", "[rendering][texture-setup]")
{
  const texture_setup::TextureLimits limits{.maxTextureSize = 4096, .max3DTextureSize = 2048};
  REQUIRE_FALSE(texture_setup::textureUploadLayoutForImage(glm::uvec3{1, 8192, 1536}, limits));
}
