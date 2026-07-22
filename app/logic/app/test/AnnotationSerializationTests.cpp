#include "logic/annotation/SerializeAnnot.h"

#include <catch2/catch_test_macros.hpp>
#include <nlohmann/json.hpp>

namespace
{
using json = nlohmann::json;
}

TEST_CASE("annotation JSON color helpers write normalized RGBA arrays", "[annotation][serialization]")
{
  const json serialized = annotation_json::colorToJson({0.1f, 0.2f, 0.3f, 0.4f});

  CHECK(serialized == json::array({0.1f, 0.2f, 0.3f, 0.4f}));
}

TEST_CASE("annotation JSON color helpers read normalized RGBA arrays", "[annotation][serialization]")
{
  const glm::vec4 color = annotation_json::colorFromJson(json::array({0.5f, 0.6f, 0.7f, 0.8f}));

  CHECK(color == glm::vec4{0.5f, 0.6f, 0.7f, 0.8f});
}

TEST_CASE("annotation JSON color helpers reject legacy RGBA objects", "[annotation][serialization]")
{
  CHECK_THROWS(annotation_json::colorFromJson(json{{"r", 0.5f}, {"g", 0.6f}, {"b", 0.7f}, {"a", 0.8f}}));
}
