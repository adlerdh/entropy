#include "rendering/helpers/PipelineHelpers.h"

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <limits>
#include <unordered_map>

TEST_CASE("rendering helpers replace shader placeholders", "[rendering][helpers]")
{
  const std::string source = "vec4 SAMPLE = TEXTURE_LOOKUP(tc);";
  const std::unordered_map<std::string, std::string> replacements{{"SAMPLE", "color"}, {"TEXTURE_LOOKUP", "texture3D"}};

  REQUIRE(rendering::replacePlaceholders(source, replacements) == "vec4 color = texture3D(tc);");
}

TEST_CASE("rendering helpers preserve 3D shader texture lookup replacements", "[rendering][helpers]")
{
  const rendering::TextureLookupReplacementSources lookupSources{
    .linear3D = "linear3D",
    .linear2D = "linear2D",
    .floatingPointLinear3D = "floatingLinear3D",
    .floatingPointLinear2D = "floatingLinear2D",
    .cubic3D = "cubic3D",
    .cubic2D = "cubic2D",
    .uintLinear2D = "uintLinear2D"};
  const std::unordered_map<std::string, std::string> replacements{
    {"$$TEXTURE_LOOKUP_FUNCTION$$", "cubic3D"},
    {"$$UINT_TEXTURE_LOOKUP_FUNCTION$$", "uintLinear3D"},
    {"$$OTHER$$", "unchanged"}};

  const auto result = rendering::shaderReplacementsForTextureDimension(
    replacements,
    rendering::TextureDimension::Texture3D,
    lookupSources);

  REQUIRE(result.at("$$IMAGE_SAMPLER_TYPE$$") == "sampler3D");
  REQUIRE(result.at("$$SEG_SAMPLER_TYPE$$") == "usampler3D");
  REQUIRE(result.at("$$TEXTURE_LOOKUP_FUNCTION$$") == "cubic3D");
  REQUIRE(result.at("$$UINT_TEXTURE_LOOKUP_FUNCTION$$") == "uintLinear3D");
  REQUIRE(result.at("$$OTHER$$") == "unchanged");
}

TEST_CASE("rendering helpers adapt shader texture lookup replacements for 2D textures", "[rendering][helpers]")
{
  const rendering::TextureLookupReplacementSources lookupSources{
    .linear3D = "linear3D",
    .linear2D = "linear2D",
    .floatingPointLinear3D = "floatingLinear3D",
    .floatingPointLinear2D = "floatingLinear2D",
    .cubic3D = "cubic3D",
    .cubic2D = "cubic2D",
    .uintLinear2D = "uintLinear2D"};

  auto replacements = std::unordered_map<std::string, std::string>{
    {"$$TEXTURE_LOOKUP_FUNCTION$$", "linear3D"},
    {"$$UINT_TEXTURE_LOOKUP_FUNCTION$$", "uintLinear3D"}};
  auto result = rendering::shaderReplacementsForTextureDimension(
    replacements,
    rendering::TextureDimension::Texture2D,
    lookupSources);
  REQUIRE(result.at("$$IMAGE_SAMPLER_TYPE$$") == "sampler2D");
  REQUIRE(result.at("$$SEG_SAMPLER_TYPE$$") == "usampler2D");
  REQUIRE(result.at("$$TEXTURE_LOOKUP_FUNCTION$$") == "linear2D");
  REQUIRE(result.at("$$UINT_TEXTURE_LOOKUP_FUNCTION$$") == "uintLinear2D");

  replacements["$$TEXTURE_LOOKUP_FUNCTION$$"] = "floatingLinear3D";
  result = rendering::shaderReplacementsForTextureDimension(
    replacements,
    rendering::TextureDimension::Texture2D,
    lookupSources);
  REQUIRE(result.at("$$TEXTURE_LOOKUP_FUNCTION$$") == "floatingLinear2D");

  replacements["$$TEXTURE_LOOKUP_FUNCTION$$"] = "cubic3D";
  result = rendering::shaderReplacementsForTextureDimension(
    replacements,
    rendering::TextureDimension::Texture2D,
    lookupSources);
  REQUIRE(result.at("$$TEXTURE_LOOKUP_FUNCTION$$") == "cubic2D");
}

TEST_CASE(
  "rendering helpers add only sampler placeholders when 2D lookup placeholders are absent",
  "[rendering][helpers]")
{
  const rendering::TextureLookupReplacementSources lookupSources{
    .linear3D = "linear3D",
    .linear2D = "linear2D",
    .floatingPointLinear3D = "floatingLinear3D",
    .floatingPointLinear2D = "floatingLinear2D",
    .cubic3D = "cubic3D",
    .cubic2D = "cubic2D",
    .uintLinear2D = "uintLinear2D"};
  const std::unordered_map<std::string, std::string> replacements{{"$$OTHER$$", "unchanged"}};

  const auto result = rendering::shaderReplacementsForTextureDimension(
    replacements,
    rendering::TextureDimension::Texture2D,
    lookupSources);

  REQUIRE(result.at("$$IMAGE_SAMPLER_TYPE$$") == "sampler2D");
  REQUIRE(result.at("$$SEG_SAMPLER_TYPE$$") == "usampler2D");
  REQUIRE(result.at("$$OTHER$$") == "unchanged");
  REQUIRE_FALSE(result.contains("$$TEXTURE_LOOKUP_FUNCTION$$"));
  REQUIRE_FALSE(result.contains("$$UINT_TEXTURE_LOOKUP_FUNCTION$$"));
}

TEST_CASE("rendering helpers grow brush preview capacity conservatively", "[rendering][helpers]")
{
  REQUIRE(rendering::growBrushPreviewCapacity(0u, 7u) == 7u);
  REQUIRE(rendering::growBrushPreviewCapacity(10u, 7u) == 10u);
  REQUIRE(rendering::growBrushPreviewCapacity(10u, 12u) == 15u);
  REQUIRE(rendering::growBrushPreviewCapacity(10u, 40u) == 40u);

  constexpr uint32_t maxValue = std::numeric_limits<uint32_t>::max();
  REQUIRE(rendering::growBrushPreviewCapacity(maxValue - 1u, maxValue) == maxValue);
}

TEST_CASE("rendering helpers grow 3D brush preview capacity per axis", "[rendering][helpers]")
{
  REQUIRE(rendering::growBrushPreviewCapacity(glm::uvec3{4, 10, 0}, glm::uvec3{5, 8, 2}) == glm::uvec3{6, 10, 2});
}

TEST_CASE("rendering helpers pack brush preview uploads into texture capacity", "[rendering][helpers]")
{
  const std::array<int64_t, 8> source{1, 2, 3, 4, 5, 6, 7, 8};
  std::vector<uint8_t> uploadData;

  const void* uploadPtr =
    rendering::prepareBrushPreviewUploadData(uploadData, source.data(), glm::uvec3{2, 2, 2}, glm::uvec3{3, 2, 2});

  REQUIRE(uploadPtr == uploadData.data());
  REQUIRE(uploadData == std::vector<uint8_t>{1, 2, 0, 3, 4, 0, 5, 6, 0, 7, 8, 0});
}

TEST_CASE("rendering helpers return texture layout defaults and cached layouts", "[rendering][helpers]")
{
  const uuids::uuid imageUid = uuids::uuid::from_string("11111111-2222-3333-4444-555555555555").value();
  const rendering::PlanarTextureLayout planarLayout{
    .dimension = rendering::TextureDimension::Texture2D,
    .axes = glm::ivec2{1, 2}};
  const std::unordered_map<uuids::uuid, rendering::PlanarTextureLayout> layouts{{imageUid, planarLayout}};

  REQUIRE(rendering::textureLayoutOrDefault(layouts, std::nullopt).dimension == rendering::TextureDimension::Texture3D);
  REQUIRE(rendering::textureLayoutOrDefault(layouts, imageUid).dimension == rendering::TextureDimension::Texture2D);
  REQUIRE(rendering::textureLayoutOrDefault(layouts, imageUid).axes == glm::ivec2{1, 2});
}

TEST_CASE("rendering helpers derive texture axes for shader slots", "[rendering][helpers]")
{
  REQUIRE(
    rendering::textureAxesForProgramSlot({.dimension = rendering::TextureDimension::Texture3D}) == glm::ivec2{0, 1});
  REQUIRE(
    rendering::textureAxesForProgramSlot(
      {.dimension = rendering::TextureDimension::Texture2D, .axes = glm::ivec2{1, 2}}) == glm::ivec2{1, 2});
}

TEST_CASE("rendering helpers classify raycastable texture layouts", "[rendering][helpers]")
{
  const uuids::uuid missingUid = uuids::uuid::from_string("aaaaaaaa-bbbb-cccc-dddd-eeeeeeeeeeee").value();
  const uuids::uuid volumeUid = uuids::uuid::from_string("11111111-2222-3333-4444-555555555555").value();
  const uuids::uuid planarUid = uuids::uuid::from_string("66666666-7777-8888-9999-aaaaaaaaaaaa").value();

  const std::unordered_map<uuids::uuid, rendering::PlanarTextureLayout> layouts{
    {volumeUid, {.dimension = rendering::TextureDimension::Texture3D}},
    {planarUid, {.dimension = rendering::TextureDimension::Texture2D, .axes = glm::ivec2{0, 1}}}};

  REQUIRE(rendering::imageHasRaycastableTextureLayout(layouts, missingUid));
  REQUIRE(rendering::imageHasRaycastableTextureLayout(layouts, volumeUid));
  REQUIRE_FALSE(rendering::imageHasRaycastableTextureLayout(layouts, planarUid));

  const std::list<uuids::uuid> inputUids{missingUid, planarUid, volumeUid};
  const std::list<uuids::uuid> expectedUids{missingUid, volumeUid};
  REQUIRE(rendering::raycastableImageUids(inputUids, layouts) == expectedUids);
}
