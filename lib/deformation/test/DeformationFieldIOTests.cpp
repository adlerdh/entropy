#include "deformation/DeformationFieldIO.h"

#include <catch2/catch_test_macros.hpp>

#include <bit>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <limits>
#include <string>

namespace df = deformation;
namespace fs = std::filesystem;

namespace
{
struct TemporaryFile
{
  fs::path path;
  explicit TemporaryFile(const std::string& name)
    : path(
        fs::temp_directory_path() /
        ("entropy-native-field-" + name + "-" +
         std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + ".nrrd"))
  {
  }
  ~TemporaryFile()
  {
    std::error_code ignored;
    fs::remove(path, ignored);
  }
};

df::FieldAsset asset(bool volume)
{
  df::DomainGeometry geometry;
  geometry.dimension = volume ? df::SpatialDimension::Volume : df::SpatialDimension::Plane;
  geometry.size = volume ? df::GridSize{4, 3, 2} : df::GridSize{4, 3, 1};
  geometry.origin = {1e6 + 0.25, -2e6 - 0.5, 3e6 + 0.125};
  geometry.spacing = volume ? glm::dvec3{0.75, 1.25, 2.5} : glm::dvec3{0.75, 1.25, 1};
  geometry.directions[0] = {0, -1, 0};
  geometry.directions[1] = {-1, 0, 0};
  geometry.directions[2] = {0, 0, -1};
  geometry.validExtent = df::IndexExtent{{1, 0, 0}, {4, 3, volume ? 2u : 1u}};
  df::FieldDomain domain(geometry);
  df::FieldAsset result{domain, volume ? df::MapDirection::Inverse : df::MapDirection::Forward, {}};
  for (std::size_t i = 0; i < domain.sampleCount(); ++i) {
    result.samples.emplace_back(float(i) / 16.0f, -float(i) / 32.0f, float(i) / 64.0f, i % 3 ? 1.0f : 0.0f);
  }
  result.samples[1].x = -0.0f;
  result.samples[2].y = std::bit_cast<float>(std::uint32_t{0x7fc12345});
  return result;
}

void requireSame(const df::FieldAsset& before, const df::FieldAsset& after)
{
  REQUIRE(after.domain.dimension() == before.domain.dimension());
  REQUIRE(after.domain.size() == before.domain.size());
  REQUIRE(after.domain.validExtent().begin == before.domain.validExtent().begin);
  REQUIRE(after.domain.validExtent().end == before.domain.validExtent().end);
  REQUIRE(after.domain.origin() == before.domain.origin());
  REQUIRE(after.domain.spacing() == before.domain.spacing());
  for (int axis = 0; axis < 3; ++axis)
    REQUIRE(after.domain.directions()[axis] == before.domain.directions()[axis]);
  REQUIRE(after.role == before.role);
  REQUIRE(after.samples.size() == before.samples.size());
  for (std::size_t i = 0; i < before.samples.size(); ++i) {
    for (int component = 0; component < 4; ++component) {
      REQUIRE(
        std::bit_cast<std::uint32_t>(after.samples[i][component]) ==
        std::bit_cast<std::uint32_t>(before.samples[i][component]));
    }
  }
}
} // namespace

TEST_CASE("Native 2D and 3D field assets round trip geometry role validity and float bits", "[deformation][io]")
{
  for (bool volume : {false, true}) {
    TemporaryFile file(volume ? "volume" : "plane");
    const auto original = asset(volume);
    df::writeNativeField(file.path, original);
    requireSame(original, df::readNativeField(file.path, 4096));
  }
}

TEST_CASE("Accepted field export validates usable vectors and validity", "[deformation][io]")
{
  auto field = asset(false);
  for (auto& sample : field.samples)
    sample = {0.25f, 0.5f, 0, 1};
  REQUIRE_NOTHROW(df::validateFieldForExport(field));
  field.samples[0].w = 0.5f;
  REQUIRE_THROWS(df::validateFieldForExport(field));
  field.samples[0].w = 1;
  field.samples[0].x = std::numeric_limits<float>::quiet_NaN();
  REQUIRE_THROWS(df::validateFieldForExport(field));
  field.samples[0].w = 0;
  REQUIRE_NOTHROW(df::validateFieldForExport(field));
  field.samples[0] = {0, 0, 0.01f, 1};
  REQUIRE_THROWS(df::validateFieldForExport(field));
  field.samples.pop_back();
  REQUIRE_THROWS(df::validateFieldForExport(field));
}

TEST_CASE("Native field import rejects truncation corruption and excessive allocation", "[deformation][io]")
{
  TemporaryFile file("corrupt");
  const auto original = asset(true);
  df::writeNativeField(file.path, original);
  REQUIRE_THROWS(df::readNativeField(file.path, original.samples.size() * 16 - 1));
  std::ifstream input(file.path, std::ios::binary);
  std::string contents(std::istreambuf_iterator<char>{input}, {});
  REQUIRE(contents.size() > original.samples.size() * 16);
  contents.back() ^= 1;
  {
    std::ofstream output(file.path, std::ios::binary | std::ios::trunc);
    output.write(contents.data(), static_cast<std::streamsize>(contents.size()));
  }
  REQUIRE_THROWS(df::readNativeField(file.path, 4096));
  contents.pop_back();
  {
    std::ofstream output(file.path, std::ios::binary | std::ios::trunc);
    output.write(contents.data(), static_cast<std::streamsize>(contents.size()));
  }
  REQUIRE_THROWS(df::readNativeField(file.path, 4096));
}

TEST_CASE("Native field import rejects disagreement between standard and editor geometry", "[deformation][io]")
{
  TemporaryFile file("geometry");
  df::writeNativeField(file.path, asset(false));
  std::ifstream input(file.path, std::ios::binary);
  std::string contents(std::istreambuf_iterator<char>{input}, {});
  const auto origin = contents.find("space origin: (");
  REQUIRE(origin != std::string::npos);
  contents[origin + std::string("space origin: (").size()] = '9';
  {
    std::ofstream output(file.path, std::ios::binary | std::ios::trunc);
    output.write(contents.data(), static_cast<std::streamsize>(contents.size()));
  }
  REQUIRE_THROWS(df::readNativeField(file.path, 4096));
}
