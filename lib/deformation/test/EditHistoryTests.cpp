#include "deformation/EditHistory.h"

#include <catch2/catch_test_macros.hpp>

#include <bit>
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <type_traits>
#include <vector>

namespace df = deformation;

namespace
{
df::FieldDomain plane()
{
  df::DomainGeometry geometry;
  geometry.dimension = df::SpatialDimension::Plane;
  geometry.size = {3, 3, 1};
  geometry.origin = {1e6, -2e6, 3e6};
  return df::FieldDomain(geometry);
}
std::shared_ptr<const df::FieldCheckpoint> checkpoint(const df::FieldDomain& domain, float value)
{
  auto result = std::make_shared<df::FieldCheckpoint>();
  result->forward.assign(domain.sampleCount(), {value, 0, 0, 1});
  result->inverse.assign(domain.sampleCount(), {-value, 0, 0, 1});
  return result;
}
df::QualityReport acceptedReport()
{
  df::QualityReport report;
  for (auto* direction : {&report.forward, &report.inverse}) {
    direction->requested = 1;
    direction->evaluated = 1;
    direction->minDeterminant = 1;
    direction->maxDeterminant = 1;
    direction->minSingularValue = 1;
    direction->maxSingularValue = 1;
  }
  report.protectionChecked = true;
  report.convergenceChecked = true;
  report.cellsVerified = true;
  return report;
}
std::vector<df::BrushDefinition> stroke()
{
  df::BrushDefinition step;
  step.dimension = df::SpatialDimension::Plane;
  step.motion = df::PushMotion{{0.1, 0, 0}};
  return {step};
}
} // namespace

TEST_CASE("History restores exact accepted checkpoints and preserves redo branches", "[deformation][history]")
{
  const auto domain = plane();
  df::EditHistory history(domain, domain, {"source", "reference", "baseline", 2}, {1}, checkpoint(domain, 0));
  STATIC_REQUIRE(std::is_same_v<decltype(history.current()), const df::EditRevision&>);
  REQUIRE(history.current().id() == df::RevisionId{1});
  REQUIRE_FALSE(history.undo());
  const auto firstSnapshot = checkpoint(domain, 0.125f);
  const auto first = history.append(firstSnapshot, acceptedReport(), {}, stroke());
  const auto secondSnapshot = checkpoint(domain, 0.3125f);
  const auto second = history.append(secondSnapshot, acceptedReport(), {}, stroke());
  REQUIRE(history.current().checkpointPtr() == secondSnapshot);
  REQUIRE(history.undo());
  REQUIRE(history.current().checkpointPtr() == firstSnapshot);
  REQUIRE(history.undo());
  REQUIRE(history.current().id() == df::RevisionId{1});
  REQUIRE(history.redo(first));
  const auto branchSnapshot = checkpoint(domain, -0.0625f);
  const auto branch = history.append(branchSnapshot, acceptedReport(), {}, stroke());
  REQUIRE(branch.value > second.value);
  REQUIRE(history.children(first) == std::vector<df::RevisionId>{second, branch});
  REQUIRE(history.undo());
  REQUIRE(history.redo(second));
  REQUIRE(
    std::bit_cast<std::uint32_t>(history.current().checkpoint().forward.front().x) ==
    std::bit_cast<std::uint32_t>(secondSnapshot->forward.front().x));
  REQUIRE_FALSE(history.redo(branch));
  REQUIRE(history.undo());
  REQUIRE(history.redo(branch));
  REQUIRE(history.current().checkpointPtr() == branchSnapshot);
  REQUIRE(history.current().parent() == first);
  REQUIRE((history.current().provenance() == df::EditProvenance{"source", "reference", "baseline", 2}));
}

TEST_CASE("History refuses incomplete or rejected revisions without moving its cursor", "[deformation][history]")
{
  const auto domain = plane();
  df::EditHistory history(domain, domain, {"source", "reference", "baseline", 0}, {1}, checkpoint(domain, 0));
  const auto root = history.currentPtr();
  auto incomplete = std::make_shared<df::FieldCheckpoint>();
  incomplete->forward.assign(domain.sampleCount(), {0, 0, 0, 1});
  REQUIRE_THROWS_AS(history.append(incomplete, acceptedReport(), {}, stroke()), std::invalid_argument);
  REQUIRE_THROWS_AS(history.append(checkpoint(domain, 1), {}, {}, stroke()), std::invalid_argument);
  REQUIRE_THROWS_AS(history.append(checkpoint(domain, 1), acceptedReport(), {}, {}), std::invalid_argument);
  REQUIRE(history.size() == 1);
  REQUIRE(history.currentPtr() == root);
  REQUIRE(history.children(root->id()).empty());
  REQUIRE(history.append(checkpoint(domain, 0.1f), acceptedReport(), {}, stroke()) == df::RevisionId{2});
}
