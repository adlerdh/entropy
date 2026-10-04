#include "rendering/deformation/FieldWorkspace.h"

#include <catch2/catch_test_macros.hpp>

#include <new>

namespace gpu = rendering::deformation;

TEST_CASE("Shared field workspace counts categories and releases failed reservations", "[deformation-workspace]")
{
  gpu::FieldWorkspace workspace(1000);
  auto images = workspace.reserve(gpu::FieldBudgetUse::ImageTextures, 350);
  auto metrics = workspace.reserve(gpu::FieldBudgetUse::Metrics, 250);
  REQUIRE(workspace.usedBytes() == 600);
  REQUIRE(workspace.usedBytes(gpu::FieldBudgetUse::ImageTextures) == 350);
  REQUIRE_THROWS_AS(workspace.reserve(gpu::FieldBudgetUse::Scratch, 401), std::bad_alloc);
  REQUIRE(workspace.usedBytes() == 600);
  auto scratch = workspace.reserve(gpu::FieldBudgetUse::Scratch, 400);
  REQUIRE(workspace.usedBytes() == workspace.capacityBytes());
  scratch = {};
  REQUIRE(workspace.usedBytes() == 600);
  metrics = {};
  images = {};
  REQUIRE(workspace.usedBytes() == 0);
}
