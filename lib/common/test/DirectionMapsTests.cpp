#include "common/DirectionMaps.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <glm/geometric.hpp>
#include <glm/vec3.hpp>

namespace
{

void checkVec3(const glm::vec3& actual, const glm::vec3& expected)
{
  CHECK(actual.x == Catch::Approx(expected.x));
  CHECK(actual.y == Catch::Approx(expected.y));
  CHECK(actual.z == Catch::Approx(expected.z));
}

} // namespace

TEST_CASE("cartesian directions expose expected vectors and abbreviations", "[common][directions]")
{
  checkVec3(Directions::get(Directions::Cartesian::PosX), {1.0f, 0.0f, 0.0f});
  checkVec3(Directions::get(Directions::Cartesian::PosY), {0.0f, 1.0f, 0.0f});
  checkVec3(Directions::get(Directions::Cartesian::PosZ), {0.0f, 0.0f, 1.0f});
  checkVec3(Directions::get(Directions::Cartesian::NegX), {-1.0f, 0.0f, 0.0f});
  checkVec3(Directions::get(Directions::Cartesian::NegY), {0.0f, -1.0f, 0.0f});
  checkVec3(Directions::get(Directions::Cartesian::NegZ), {0.0f, 0.0f, -1.0f});
  checkVec3(Directions::get(Directions::Cartesian::XY), glm::normalize(glm::vec3{1.0f, 1.0f, 0.0f}));
  checkVec3(Directions::get(Directions::Cartesian::YZ), glm::normalize(glm::vec3{0.0f, 1.0f, 1.0f}));
  checkVec3(Directions::get(Directions::Cartesian::ZX), glm::normalize(glm::vec3{1.0f, 0.0f, 1.0f}));
  checkVec3(Directions::get(Directions::Cartesian::XYZ), glm::normalize(glm::vec3{1.0f, 1.0f, 1.0f}));

  CHECK(Directions::abbrev(Directions::Cartesian::PosX) == "+x");
  CHECK(Directions::abbrev(Directions::Cartesian::PosY) == "+y");
  CHECK(Directions::abbrev(Directions::Cartesian::PosZ) == "+z");
  CHECK(Directions::abbrev(Directions::Cartesian::NegX) == "-x");
  CHECK(Directions::abbrev(Directions::Cartesian::NegY) == "-y");
  CHECK(Directions::abbrev(Directions::Cartesian::NegZ) == "-z");
  CHECK(Directions::abbrev(Directions::Cartesian::XY) == "xy");
  CHECK(Directions::abbrev(Directions::Cartesian::YZ) == "yz");
  CHECK(Directions::abbrev(Directions::Cartesian::ZX) == "zx");
  CHECK(Directions::abbrev(Directions::Cartesian::XYZ) == "xyz");
}

TEST_CASE("view directions expose expected vectors", "[common][directions]")
{
  checkVec3(Directions::get(Directions::View::Left), {-1.0f, 0.0f, 0.0f});
  checkVec3(Directions::get(Directions::View::Right), {1.0f, 0.0f, 0.0f});
  checkVec3(Directions::get(Directions::View::Down), {0.0f, -1.0f, 0.0f});
  checkVec3(Directions::get(Directions::View::Up), {0.0f, 1.0f, 0.0f});
  checkVec3(Directions::get(Directions::View::Front), {0.0f, 0.0f, -1.0f});
  checkVec3(Directions::get(Directions::View::Back), {0.0f, 0.0f, 1.0f});
}

TEST_CASE("human anatomical directions use LPS-positive coordinates", "[common][directions]")
{
  checkVec3(Directions::get(Directions::Anatomy::Right), {-1.0f, 0.0f, 0.0f});
  checkVec3(Directions::get(Directions::Anatomy::Left), {1.0f, 0.0f, 0.0f});
  checkVec3(Directions::get(Directions::Anatomy::Anterior), {0.0f, -1.0f, 0.0f});
  checkVec3(Directions::get(Directions::Anatomy::Posterior), {0.0f, 1.0f, 0.0f});
  checkVec3(Directions::get(Directions::Anatomy::Inferior), {0.0f, 0.0f, -1.0f});
  checkVec3(Directions::get(Directions::Anatomy::Superior), {0.0f, 0.0f, 1.0f});

  CHECK(Directions::abbrev(Directions::Anatomy::Right) == "R");
  CHECK(Directions::abbrev(Directions::Anatomy::Left) == "L");
  CHECK(Directions::abbrev(Directions::Anatomy::Anterior) == "A");
  CHECK(Directions::abbrev(Directions::Anatomy::Posterior) == "P");
  CHECK(Directions::abbrev(Directions::Anatomy::Inferior) == "I");
  CHECK(Directions::abbrev(Directions::Anatomy::Superior) == "S");
}

TEST_CASE("rodent anatomical directions expose expected vectors and abbreviations", "[common][directions]")
{
  checkVec3(Directions::get(Directions::Animal::Right), {-1.0f, 0.0f, 0.0f});
  checkVec3(Directions::get(Directions::Animal::Left), {1.0f, 0.0f, 0.0f});
  checkVec3(Directions::get(Directions::Animal::Ventral), {0.0f, -1.0f, 0.0f});
  checkVec3(Directions::get(Directions::Animal::Dorsal), {0.0f, 1.0f, 0.0f});
  checkVec3(Directions::get(Directions::Animal::Caudal), {0.0f, 0.0f, -1.0f});
  checkVec3(Directions::get(Directions::Animal::Rostral), {0.0f, 0.0f, 1.0f});

  CHECK(Directions::abbrev(Directions::Animal::Right) == "R");
  CHECK(Directions::abbrev(Directions::Animal::Left) == "L");
  CHECK(Directions::abbrev(Directions::Animal::Rostral) == "Ros");
  CHECK(Directions::abbrev(Directions::Animal::Dorsal) == "Dor");
  CHECK(Directions::abbrev(Directions::Animal::Caudal) == "Cau");
  CHECK(Directions::abbrev(Directions::Animal::Ventral) == "Ven");
}
