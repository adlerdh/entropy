#include "ui/ExportJobService.h"
#include "../../../test/support/TempDirectory.h"
#include "../../../test/support/CompletionGate.h"
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <chrono>

using entropy::test::readBytes;
using entropy::test::writeBytes;

TEST_CASE(
  "Failed sidecar publication and rollback retain both original files at the reported recovery path",
  "[workflow][export][filesystem]")
{
  entropy::test::TempDirectory directory;
  const auto header = directory.path() / "labels.mhd";
  const auto pixels = directory.path() / "labels.raw";
  writeBytes(header, "original header");
  writeBytes(pixels, "original pixels");
  std::filesystem::path recovery;
  {
    unsigned calls = 0;
    ui::export_jobs::StagedOutput output(header, [&](const auto& from, const auto& to) -> std::optional<std::string> {
      if (++calls >= 4) return "injected publish and rollback failures";
      std::error_code error;
      std::filesystem::rename(from, to, error);
      return error ? std::optional{error.message()} : std::nullopt;
    });
    recovery = output.temporaryPath().parent_path();
    writeBytes(output.temporaryPath(), "new header");
    writeBytes(recovery / "labels.raw", "new pixels");
    const auto failure = output.commit();
    REQUIRE(failure);
    CHECK(failure->find(recovery.string()) != std::string::npos);
  }
  REQUIRE(std::filesystem::is_directory(recovery));
  bool foundHeader = false, foundPixels = false;
  for (const auto& entry : std::filesystem::directory_iterator(recovery)) {
    const auto bytes = readBytes(entry.path());
    foundHeader |= bytes == "original header";
    foundPixels |= bytes == "original pixels";
  }
  CHECK(foundHeader);
  CHECK(foundPixels);
}

TEST_CASE("Sidecar publication rolls back every failing rename boundary", "[workflow][export][filesystem]")
{
  const unsigned failAt = GENERATE(1u, 2u, 3u, 4u);
  entropy::test::TempDirectory directory;
  const auto header = directory.path() / "labels.mhd";
  const auto pixels = directory.path() / "labels.raw";
  writeBytes(header, "original header");
  writeBytes(pixels, "original pixels");
  unsigned calls = 0;
  std::filesystem::path staging;
  {
    ui::export_jobs::StagedOutput output(header, [&](const auto& from, const auto& to) -> std::optional<std::string> {
      if (++calls == failAt) return "injected rename failure";
      std::error_code error;
      std::filesystem::rename(from, to, error);
      return error ? std::optional{error.message()} : std::nullopt;
    });
    staging = output.temporaryPath().parent_path();
    writeBytes(output.temporaryPath(), "new header");
    writeBytes(staging / "labels.raw", "new pixels");
    REQUIRE(output.commit().has_value());
    CHECK(readBytes(header) == "original header");
    CHECK(readBytes(pixels) == "original pixels");
  }
  CHECK_FALSE(std::filesystem::exists(staging));
}

TEST_CASE("Aborted writers cannot publish partial bytes or destroy existing sidecars", "[workflow][export][filesystem]")
{
  entropy::test::TempDirectory directory;
  const auto destination = directory.path() / "labels.mhd";
  writeBytes(destination, "original header");
  writeBytes(directory.path() / "labels.raw", "original pixels");
  std::filesystem::path staging;
  bool writeFailed = false;
  try {
    ui::export_jobs::StagedOutput output(destination);
    staging = output.temporaryPath().parent_path();
    writeBytes(output.temporaryPath(), "partial header");
    writeBytes(staging / "labels.raw", "partial pixels");
    throw std::ios_base::failure("injected write/flush failure");
  }
  catch (const std::ios_base::failure&) {
    writeFailed = true;
  }
  CHECK(writeFailed);
  CHECK(readBytes(destination) == "original header");
  CHECK(readBytes(directory.path() / "labels.raw") == "original pixels");
  CHECK_FALSE(std::filesystem::exists(staging));
}

TEST_CASE("Cancel before publication preserves disk bytes and dispatches once", "[workflow][export][threading]")
{
  entropy::test::TempDirectory directory;
  const auto destination = directory.path() / "labels.nii";
  writeBytes(destination, "original");
  ui::export_jobs::Service service;
  entropy::test::CompletionGate gate;
  unsigned completions = 0;
  REQUIRE(service.submit(
    {.description = "paused export",
     .destination = destination,
     .task =
       [&, pause = gate.waiter()](auto& context) {
         ui::export_jobs::StagedOutput output(destination);
         writeBytes(output.temporaryPath(), "new revision");
         pause();
         if (context.cancellationRequested()) return ui::export_jobs::Result::cancelled();
         if (const auto error = output.commit()) return ui::export_jobs::Result::failure(*error);
         return ui::export_jobs::Result::success({destination});
       },
     .completion =
       [&](const auto& result) {
         ++completions;
         CHECK(result.outcome == ui::export_jobs::Outcome::Cancelled);
       }}));
  REQUIRE(gate.awaitEntry());
  service.requestCancel();
  gate.release();
  REQUIRE(service.waitForFinished(std::chrono::seconds{5}));
  service.dispatchCompletion();
  service.dispatchCompletion();
  CHECK(completions == 1);
  CHECK(readBytes(destination) == "original");
}
