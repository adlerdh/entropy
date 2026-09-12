#include "ui/ExportJobService.h"

#include <catch2/catch_test_macros.hpp>

#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>
#include <thread>

namespace
{
using namespace std::chrono_literals;

std::filesystem::path testPath(const std::string& name)
{
  return std::filesystem::temp_directory_path() / ("entropy-export-job-test-" + name);
}
} // namespace

TEST_CASE("Export jobs publish progress and completion", "[ui][export][threading]")
{
  ui::export_jobs::Service service;
  REQUIRE(service.submit(
    {.description = "Test export", .destination = "output.vtp", .task = [](ui::export_jobs::JobContext& context) {
       context.update("Writing", 0.5f);
       return ui::export_jobs::Result::success({"output.vtp"}, "Done");
     }}));
  REQUIRE(service.waitForFinished(2s));

  const auto snapshot = service.snapshot();
  CHECK(snapshot.hasJob);
  CHECK(snapshot.outcome == ui::export_jobs::Outcome::Succeeded);
  CHECK(snapshot.progress == 1.0f);
  CHECK(snapshot.message == "Done");
  REQUIRE(snapshot.outputFileNames.size() == 1u);
  CHECK(snapshot.outputFileNames.front() == "output.vtp");

  service.dismiss();
  CHECK_FALSE(service.snapshot().hasJob);
}

TEST_CASE("Indeterminate export stages retain the last known progress", "[ui][export][threading]")
{
  ui::export_jobs::Service service;
  std::atomic<bool> enteredIndeterminateStage = false;
  std::atomic<bool> finish = false;
  REQUIRE(service.submit(
    {.description = "Test export",
     .destination = "output.vtp",
     .task = [&enteredIndeterminateStage, &finish](ui::export_jobs::JobContext& context) {
       context.update("Preparing", 0.4f);
       context.update("Writing");
       enteredIndeterminateStage.store(true, std::memory_order_release);
       while (!finish.load(std::memory_order_acquire)) {
         std::this_thread::yield();
       }
       return ui::export_jobs::Result::success({"output.vtp"});
     }}));
  while (!enteredIndeterminateStage.load(std::memory_order_acquire)) {
    std::this_thread::yield();
  }

  const auto snapshot = service.snapshot();
  CHECK(snapshot.indeterminate);
  REQUIRE(snapshot.progress.has_value());
  CHECK(*snapshot.progress == 0.4f);

  finish.store(true, std::memory_order_release);
  REQUIRE(service.waitForFinished(2s));
}

TEST_CASE("Export jobs reject overlap and support cooperative cancellation", "[ui][export][threading]")
{
  ui::export_jobs::Service service;
  std::atomic<bool> started = false;
  REQUIRE(service.submit(
    {.description = "Long export",
     .destination = "first.vtp",
     .task = [&started](ui::export_jobs::JobContext& context) {
       started.store(true, std::memory_order_release);
       while (!context.cancellationRequested()) {
         std::this_thread::yield();
       }
       return ui::export_jobs::Result::cancelled();
     }}));
  while (!started.load(std::memory_order_acquire)) {
    std::this_thread::yield();
  }

  CHECK_FALSE(service.submit(
    {.description = "Overlapping export", .destination = "second.vtp", .task = [](ui::export_jobs::JobContext&) {
       return ui::export_jobs::Result::success({"second.vtp"});
     }}));

  service.requestCancel();
  REQUIRE(service.waitForFinished(2s));
  CHECK(service.snapshot().outcome == ui::export_jobs::Outcome::Cancelled);
}

TEST_CASE("Staged exports commit completed files and discard incomplete files", "[ui][export][filesystem]")
{
  const auto committedPath = testPath("committed.vtp");
  const auto discardedPath = testPath("discarded.vtp");
  std::filesystem::remove(committedPath);
  std::filesystem::remove(discardedPath);

  {
    std::ofstream existing{committedPath};
    existing << "old";
  }
  {
    ui::export_jobs::StagedOutput output{committedPath};
    CHECK(output.temporaryPath().extension() == ".vtp");
    std::ofstream temporary{output.temporaryPath()};
    temporary << "new";
    temporary.close();
    CHECK_FALSE(output.commit().has_value());
  }
  {
    std::ifstream committed{committedPath};
    std::string contents;
    committed >> contents;
    CHECK(contents == "new");
  }

  std::filesystem::path abandonedTemporary;
  {
    ui::export_jobs::StagedOutput output{discardedPath};
    abandonedTemporary = output.temporaryPath();
    std::ofstream temporary{abandonedTemporary};
    temporary << "partial";
  }
  CHECK_FALSE(std::filesystem::exists(abandonedTemporary));
  CHECK_FALSE(std::filesystem::exists(discardedPath));

  std::filesystem::remove(committedPath);
}

TEST_CASE("Staged exports preserve codec sidecar filenames", "[ui][export][filesystem]")
{
  const auto headerPath = testPath("paired.hdr");
  const auto imagePath = headerPath.parent_path() / "entropy-export-job-test-paired.img";
  std::filesystem::remove(headerPath);
  std::filesystem::remove(imagePath);

  {
    std::ofstream oldHeader{headerPath};
    oldHeader << "old-header";
    std::ofstream oldImage{imagePath};
    oldImage << "old-pixels";
  }

  {
    ui::export_jobs::StagedOutput output{headerPath};
    std::ofstream header{output.temporaryPath()};
    header << "header";
    header.close();
    std::ofstream sidecar{output.temporaryPath().parent_path() / "entropy-export-job-test-paired.img"};
    sidecar << "pixels";
    sidecar.close();
    REQUIRE_FALSE(output.commit().has_value());
  }

  CHECK(std::filesystem::exists(headerPath));
  CHECK(std::filesystem::exists(imagePath));
  {
    std::ifstream header{headerPath};
    std::ifstream image{imagePath};
    std::string headerContents;
    std::string imageContents;
    header >> headerContents;
    image >> imageContents;
    CHECK(headerContents == "header");
    CHECK(imageContents == "pixels");
  }
  std::filesystem::remove(headerPath);
  std::filesystem::remove(imagePath);
}
