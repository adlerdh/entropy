#include "logic/app/DeformationArchive.h"

#include <catch2/catch_test_macros.hpp>

#include <bit>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <vector>

namespace fs = std::filesystem;
namespace df = deformation;
namespace archive = deformation_archive;

namespace
{
struct TemporaryDirectory
{
  fs::path path =
    fs::temp_directory_path() /
    ("entropy-deformation-archive-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  TemporaryDirectory()
  {
    fs::create_directories(path);
  }
  ~TemporaryDirectory()
  {
    std::error_code ignored;
    fs::remove_all(path, ignored);
  }
};

df::FieldDomain domain()
{
  df::DomainGeometry geometry;
  geometry.dimension = df::SpatialDimension::Plane;
  geometry.size = {5, 4, 1};
  geometry.origin = {12, -34, 56};
  geometry.spacing = {0.5, 0.75, 1};
  geometry.directions[0] = {0, 1, 0};
  geometry.directions[1] = {-1, 0, 0};
  geometry.directions[2] = {0, 0, 1};
  return df::FieldDomain(geometry);
}
std::shared_ptr<const df::FieldCheckpoint> checkpoint(float value)
{
  auto result = std::make_shared<df::FieldCheckpoint>();
  result->forward.assign(domain().sampleCount(), {value, 0, 0, 1});
  result->inverse.assign(domain().sampleCount(), {-value, 0, 0, 1});
  return result;
}
df::QualityReport quality()
{
  df::QualityReport result;
  for (auto* direction : {&result.forward, &result.inverse}) {
    direction->requested = 1;
    direction->evaluated = 1;
    direction->minDeterminant = 1;
    direction->maxDeterminant = 1;
    direction->minSingularValue = 1;
    direction->maxSingularValue = 1;
  }
  result.protectionChecked = true;
  result.convergenceChecked = true;
  result.cellsVerified = true;
  return result;
}
std::vector<df::BrushDefinition> stroke()
{
  df::BrushDefinition brush;
  brush.dimension = df::SpatialDimension::Plane;
  brush.centerMm = {12, -34, 56};
  brush.directions = domain().directions();
  brush.radiusMm = 3;
  brush.motion = df::PushMotion{{0.1, 0, 0}};
  return {brush};
}
df::EditHistory history()
{
  const auto field = domain();
  df::EditHistory result(field, field, {"source", "reference", "baseline", 3}, {1}, checkpoint(0));
  const auto first = result.append(checkpoint(0.125f), quality(), {}, stroke());
  auto radial = stroke();
  radial.front().motion = df::RadialMotion{0.05};
  static_cast<void>(result.append(checkpoint(0.25f), quality(), {}, radial));
  REQUIRE(result.undo());
  REQUIRE(result.current().id() == first);
  auto twirl = stroke();
  twirl.front().motion = df::TwirlMotion{{0, 0, 1}, 0.1};
  twirl.front().protection.push_back({{12, -34, 56}, 0.5, 0.25});
  static_cast<void>(result.append(checkpoint(-0.0625f), quality(), {}, twirl));
  return result;
}
} // namespace

TEST_CASE("Revision bundle restores exact branches cursor and physical field values", "[deformation-archive]")
{
  TemporaryDirectory directory;
  const fs::path projectFile = directory.path / "project.json";
  const auto original = history();
  serialize::ProjectDeformationReference reference;
  {
    archive::StagedBundle bundle(projectFile, "edit_1", original);
    reference = bundle.publish();
    bundle.commit();
  }
  const auto restored = archive::loadBundle(reference, 8192);
  REQUIRE(restored.size() == original.size());
  REQUIRE(restored.current().id() == original.current().id());
  REQUIRE(restored.children({2}) == original.children({2}));
  REQUIRE(restored.current().provenance() == original.current().provenance());
  for (std::uint64_t id = 1; id <= original.size(); ++id) {
    const auto before = original.find({id});
    const auto after = restored.find({id});
    REQUIRE(after);
    REQUIRE(after->parent() == before->parent());
    REQUIRE(after->stroke().size() == before->stroke().size());
    if (!before->stroke().empty()) {
      CHECK(after->stroke().front().motion.index() == before->stroke().front().motion.index());
      CHECK(after->stroke().front().protection.size() == before->stroke().front().protection.size());
    }
    REQUIRE(after->checkpoint().forward.size() == before->checkpoint().forward.size());
    for (std::size_t i = 0; i < before->checkpoint().forward.size(); ++i) {
      REQUIRE(
        std::bit_cast<std::uint32_t>(after->checkpoint().forward[i].x) ==
        std::bit_cast<std::uint32_t>(before->checkpoint().forward[i].x));
      REQUIRE(
        std::bit_cast<std::uint32_t>(after->checkpoint().inverse[i].x) ==
        std::bit_cast<std::uint32_t>(before->checkpoint().inverse[i].x));
    }
  }
  const auto relativeManifest = reference.m_manifestPath.lexically_relative(directory.path);
  const fs::path relocated = directory.path.string() + "-moved";
  fs::rename(directory.path, relocated);
  directory.path = relocated;
  reference.m_manifestPath = relocated / relativeManifest;
  const auto moved = archive::loadBundle(reference, 8192);
  CHECK(moved.current().id() == original.current().id());
}

TEST_CASE("Revision bundle rejects malformed accepted fields before publication", "[deformation-archive]")
{
  TemporaryDirectory directory;
  auto damaged = std::make_shared<df::FieldCheckpoint>(*checkpoint(0));
  damaged->forward.front().w = 0.5f;
  const auto field = domain();
  df::EditHistory invalid(field, field, {"source", "reference", "baseline", 3}, {1}, damaged);
  REQUIRE_THROWS(archive::StagedBundle(directory.path / "project.json", "edit_1", invalid));
  const auto bundles = directory.path / "project.json.assets" / "deformations" / "edit_1";
  REQUIRE(fs::is_empty(bundles));
}

TEST_CASE("Uncommitted bundles roll back and corrupted or over-budget bundles fail closed", "[deformation-archive]")
{
  TemporaryDirectory directory;
  const fs::path projectFile = directory.path / "project.json";
  const auto original = history();
  fs::path abandoned;
  {
    archive::StagedBundle bundle(projectFile, "edit_1", original);
    abandoned = bundle.publish().m_manifestPath;
    REQUIRE(fs::exists(abandoned));
  }
  REQUIRE_FALSE(fs::exists(abandoned));

  archive::StagedBundle bundle(projectFile, "edit_1", original);
  const auto reference = bundle.publish();
  REQUIRE_THROWS(archive::loadBundle(reference, 1));
  const fs::path field = reference.m_manifestPath.parent_path() / "revisions" / "2" / "forward.nrrd";
  std::fstream stream(field, std::ios::binary | std::ios::in | std::ios::out);
  stream.seekp(-1, std::ios::end);
  stream.put('\x7f');
  stream.close();
  REQUIRE_THROWS(archive::loadBundle(reference, 8192));
  fs::remove(field);
  REQUIRE_THROWS(archive::loadBundle(reference, 8192));
}
