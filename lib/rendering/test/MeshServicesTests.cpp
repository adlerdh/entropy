#include "rendering/mesh/MeshData.h"
#include "rendering/mesh/MeshExtraction.h"
#include "rendering/mesh/MeshExtractionQueue.h"
#include "rendering/mesh/MeshExtractionRunner.h"
#include "rendering/mesh/MeshExtractionService.h"
#include "rendering/mesh/MeshHandle.h"
#include "rendering/mesh/MeshKeys.h"
#include "rendering/mesh/MeshResourceStore.h"

#include <uuid.h>

#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace
{

using namespace rendering::mesh;

uuids::uuid testUuid(const std::string& text)
{
  return uuids::uuid::from_string(text).value();
}

MeshGeometryKey testKey(const uuids::uuid& sourceUid, const double isoValue)
{
  return MeshGeometryKey{
    .sourceUid = sourceUid,
    .sourceDataVersion = 1,
    .sourceGeometryVersion = 2,
    .component = {},
    .labelValue = {},
    .isoValue = isoValue,
    .extractionAlgorithm = "test",
    .extractionAlgorithmVersion = 1};
}

MeshData triangleMesh()
{
  return MeshData{
    .positions = {{0.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}},
    .normals = {{0.0f, 0.0f, 1.0f}, {0.0f, 0.0f, 1.0f}, {0.0f, 0.0f, 1.0f}},
    .indices = {0, 1, 2},
    .colors = {},
    .textureCoords = {}};
}

std::vector<MeshExtractionRunResult> waitForCompletion(MeshExtractionService& service)
{
  for (int attempt = 0; attempt < 200; ++attempt) {
    std::vector<MeshExtractionRunResult> results = service.consumeCompleted();
    if (!results.empty()) {
      return results;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds{1});
  }
  return {};
}

} // namespace

TEST_CASE("mesh extraction service owns scheduling and CPU cache transitions", "[rendering][mesh][service]")
{
  const MeshGeometryKey key = testKey(testUuid("11111111-2222-3333-4444-555555555555"), 1.5);
  MeshExtractionService service;

  REQUIRE(service.canSubmit(key));
  REQUIRE(service.submit(key, "test mesh", [key] {
    return MeshExtractionJobResult{
      .key = key,
      .result = MeshExtractionResult{.key = key, .mesh = triangleMesh(), .diagnostics = {"complete"}},
      .diagnostics = {}};
  }));
  CHECK_FALSE(service.canSubmit(key));

  const std::vector<MeshExtractionRunResult> results = waitForCompletion(service);
  REQUIRE(results.size() == 1u);
  CHECK(results.front().status == MeshExtractionRunStatus::Ready);
  REQUIRE(service.readyMesh(key));
  CHECK(service.readyMesh(key)->indices == std::vector<uint32_t>{0, 1, 2});
}

TEST_CASE("mesh extraction service discards cache and queued work outside the live set", "[rendering][mesh][service]")
{
  const uuids::uuid sourceUid = testUuid("aaaaaaaa-bbbb-cccc-dddd-eeeeeeeeeeee");
  const MeshGeometryKey retainedKey = testKey(sourceUid, 1.0);
  const MeshGeometryKey obsoleteKey = testKey(sourceUid, 2.0);
  MeshExtractionService service;

  REQUIRE(service.submit(retainedKey, "retained", [retainedKey] {
    return MeshExtractionJobResult{
      .key = retainedKey,
      .result = MeshExtractionResult{.key = retainedKey, .mesh = triangleMesh(), .diagnostics = {}},
      .diagnostics = {}};
  }));
  REQUIRE(waitForCompletion(service).size() == 1u);
  REQUIRE(service.submit(obsoleteKey, "obsolete", [obsoleteKey] {
    return MeshExtractionJobResult{
      .key = obsoleteKey,
      .result = MeshExtractionResult{.key = obsoleteKey, .mesh = triangleMesh(), .diagnostics = {}},
      .diagnostics = {}};
  }));
  REQUIRE(waitForCompletion(service).size() == 1u);

  CHECK(service.retainOnly(MeshGeometryKeySet{retainedKey}) == 1u);
  CHECK(service.readyMesh(retainedKey));
  CHECK_FALSE(service.readyMesh(obsoleteKey));
}

TEST_CASE("mesh resource store provides stable handles without application state", "[rendering][mesh][service]")
{
  const uuids::uuid firstHandleUid = testUuid("10000000-0000-0000-0000-000000000001");
  const uuids::uuid secondHandleUid = testUuid("20000000-0000-0000-0000-000000000002");
  std::vector<uuids::uuid> ids{firstHandleUid, secondHandleUid};
  std::size_t nextId = 0;
  MeshResourceStore store{[&ids, &nextId] {
    return ids.at(nextId++);
  }};
  const uuids::uuid sourceUid = testUuid("30000000-0000-0000-0000-000000000003");
  const MeshGeometryKey retainedKey = testKey(sourceUid, 1.0);
  const MeshGeometryKey obsoleteKey = testKey(sourceUid, 2.0);

  const MeshHandle retained = store.handleFor(retainedKey);
  CHECK(store.handleFor(retainedKey) == retained);
  const MeshHandle obsolete = store.handleFor(obsoleteKey);
  CHECK(retained.uid == firstHandleUid);
  CHECK(obsolete.uid == secondHandleUid);
  REQUIRE(store.findKey(retained));
  CHECK(*store.findKey(retained) == retainedKey);

  CHECK(store.retainOnly(MeshGeometryKeySet{retainedKey}) == 1u);
  CHECK(store.findHandle(retainedKey));
  CHECK_FALSE(store.findHandle(obsoleteKey));
  CHECK_FALSE(store.findKey(obsolete));
}
