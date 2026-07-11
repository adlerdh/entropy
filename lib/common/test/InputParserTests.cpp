#include "common/InputParser.h"
#include "common/LoggingDefaults.h"

#include <catch2/catch_test_macros.hpp>

#include <spdlog/spdlog.h>

#include <array>
#include <string>
#include <vector>

TEST_CASE("macOS LaunchServices process serial number argument is ignored", "[common][input]")
{
  char app[] = "Entropy";
  char psn[] = "-psn_0_1327411";

  std::array<char*, 2> argv{app, psn};

  InputParams params;
  REQUIRE(parseCommandLine(static_cast<int>(argv.size()), argv.data(), params));

  CHECK_FALSE(params.set);
  CHECK(params.imageFiles.empty());
  CHECK(params.dicomPaths.empty());
  CHECK_FALSE(params.projectFile);
}

TEST_CASE("console log level defaults to the build configuration level", "[common][input]")
{
  char app[] = "Entropy";

  std::array<char*, 1> argv{app};

  InputParams params;
  REQUIRE(parseCommandLine(static_cast<int>(argv.size()), argv.data(), params));

  CHECK(params.consoleLogLevel == entropy::logging::defaultLogLevel());
}

TEST_CASE("console log level option accepts all documented spellings", "[common][input]")
{
  const std::vector<std::pair<std::string, spdlog::level::level_enum> > cases{
    {"trace", spdlog::level::trace},
    {"debug", spdlog::level::debug},
    {"info", spdlog::level::info},
    {"warn", spdlog::level::warn},
    {"warning", spdlog::level::warn},
    {"err", spdlog::level::err},
    {"error", spdlog::level::err},
    {"critical", spdlog::level::critical},
    {"off", spdlog::level::off},
    {"invalid", spdlog::level::info}};

  for (const auto& [levelName, expectedLevel] : cases) {
    std::string app = "Entropy";
    std::string logOpt = "--log-level";
    std::string level = levelName;
    std::array<char*, 3> argv{app.data(), logOpt.data(), level.data()};

    InputParams params;
    REQUIRE(parseCommandLine(static_cast<int>(argv.size()), argv.data(), params));
    CHECK(params.consoleLogLevel == expectedLevel);
  }
}

TEST_CASE("repeated segmentation options are associated with the preceding image in argument order", "[common][input]")
{
  char app[] = "Entropy";
  char imageOpt[] = "--image";
  char image[] = "image.nii.gz";
  char segOpt0[] = "--seg";
  char seg0[] = "seg-a.nii.gz";
  char segOpt1[] = "--seg";
  char seg1[] = "seg-b.nii.gz";

  std::array<char*, 7> argv{app, imageOpt, image, segOpt0, seg0, segOpt1, seg1};

  InputParams params;
  REQUIRE(parseCommandLine(static_cast<int>(argv.size()), argv.data(), params));

  REQUIRE(params.imageFiles.size() == 1);
  CHECK(params.imageFiles[0].image == "image.nii.gz");
  REQUIRE(params.imageFiles[0].segmentations.size() == 2);
  CHECK(params.imageFiles[0].segmentations[0] == "seg-a.nii.gz");
  CHECK(params.imageFiles[0].segmentations[1] == "seg-b.nii.gz");
}

TEST_CASE("one segmentation option can provide multiple segmentations for the preceding image", "[common][input]")
{
  char app[] = "Entropy";
  char imageOpt[] = "--image";
  char image[] = "image.nii.gz";
  char segOpt[] = "--seg";
  char seg0[] = "seg-a.nii.gz";
  char seg1[] = "seg-b.nii.gz";

  std::array<char*, 6> argv{app, imageOpt, image, segOpt, seg0, seg1};

  InputParams params;
  REQUIRE(parseCommandLine(static_cast<int>(argv.size()), argv.data(), params));

  REQUIRE(params.imageFiles.size() == 1);
  CHECK(params.imageFiles[0].image == "image.nii.gz");
  REQUIRE(params.imageFiles[0].segmentations.size() == 2);
  CHECK(params.imageFiles[0].segmentations[0] == "seg-a.nii.gz");
  CHECK(params.imageFiles[0].segmentations[1] == "seg-b.nii.gz");
}

TEST_CASE("image option accepts singular, plural, and short spellings", "[common][input]")
{
  char app[] = "Entropy";
  char imageOpt0[] = "--image";
  char image0[] = "image-a.nii.gz";
  char imageOpt1[] = "--images";
  char image1[] = "image-b.nii.gz";
  char imageOpt2[] = "-i";
  char image2[] = "image-c.nii.gz";

  std::array<char*, 7> argv{app, imageOpt0, image0, imageOpt1, image1, imageOpt2, image2};

  InputParams params;
  REQUIRE(parseCommandLine(static_cast<int>(argv.size()), argv.data(), params));

  REQUIRE(params.imageFiles.size() == 3);
  CHECK(params.imageFiles[0].image == "image-a.nii.gz");
  CHECK(params.imageFiles[1].image == "image-b.nii.gz");
  CHECK(params.imageFiles[2].image == "image-c.nii.gz");
}

TEST_CASE("segmentation option accepts long and short spellings", "[common][input]")
{
  char app[] = "Entropy";
  char imageOpt[] = "-i";
  char image[] = "image.nii.gz";
  char segOpt0[] = "--seg";
  char seg0[] = "seg-a.nii.gz";
  char segOpt1[] = "-s";
  char seg1[] = "seg-b.nii.gz";

  std::array<char*, 7> argv{app, imageOpt, image, segOpt0, seg0, segOpt1, seg1};

  InputParams params;
  REQUIRE(parseCommandLine(static_cast<int>(argv.size()), argv.data(), params));

  REQUIRE(params.imageFiles.size() == 1);
  REQUIRE(params.imageFiles[0].segmentations.size() == 2);
  CHECK(params.imageFiles[0].segmentations[0] == "seg-a.nii.gz");
  CHECK(params.imageFiles[0].segmentations[1] == "seg-b.nii.gz");
}

TEST_CASE("a new image option terminates a multi-value segmentation option", "[common][input]")
{
  char app[] = "Entropy";
  char imageOpt0[] = "--image";
  char image0[] = "image-a.nii.gz";
  char segOpt[] = "--seg";
  char seg0[] = "seg-a.nii.gz";
  char seg1[] = "seg-b.nii.gz";
  char imageOpt1[] = "--image";
  char image1[] = "image-b.nii.gz";

  std::array<char*, 8> argv{app, imageOpt0, image0, segOpt, seg0, seg1, imageOpt1, image1};

  InputParams params;
  REQUIRE(parseCommandLine(static_cast<int>(argv.size()), argv.data(), params));

  REQUIRE(params.imageFiles.size() == 2);
  CHECK(params.imageFiles[0].image == "image-a.nii.gz");
  REQUIRE(params.imageFiles[0].segmentations.size() == 2);
  CHECK(params.imageFiles[0].segmentations[0] == "seg-a.nii.gz");
  CHECK(params.imageFiles[0].segmentations[1] == "seg-b.nii.gz");
  CHECK(params.imageFiles[1].image == "image-b.nii.gz");
  CHECK(params.imageFiles[1].segmentations.empty());
}

TEST_CASE("DICOM option accepts long and short spellings", "[common][input]")
{
  char app[] = "Entropy";
  char dicomOpt0[] = "--dicom";
  char dicom0[] = "dicom-a";
  char dicom1[] = "dicom-b";
  char dicomOpt1[] = "-d";
  char dicom2[] = "dicom-c";

  std::array<char*, 6> argv{app, dicomOpt0, dicom0, dicom1, dicomOpt1, dicom2};

  InputParams params;
  REQUIRE(parseCommandLine(static_cast<int>(argv.size()), argv.data(), params));

  CHECK(params.imageFiles.empty());
  REQUIRE(params.dicomPaths.size() == 3);
  CHECK(params.dicomPaths[0] == "dicom-a");
  CHECK(params.dicomPaths[1] == "dicom-b");
  CHECK(params.dicomPaths[2] == "dicom-c");
  CHECK_FALSE(params.projectFile);
}

TEST_CASE("positional image paths are accepted as an unambiguous shorthand", "[common][input]")
{
  char app[] = "Entropy";
  char image0[] = "image-a.nii.gz";
  char image1[] = "image-b.nii.gz";

  std::array<char*, 3> argv{app, image0, image1};

  InputParams params;
  REQUIRE(parseCommandLine(static_cast<int>(argv.size()), argv.data(), params));

  REQUIRE(params.imageFiles.size() == 2);
  CHECK(params.imageFiles[0].image == "image-a.nii.gz");
  CHECK(params.imageFiles[0].segmentations.empty());
  CHECK(params.imageFiles[1].image == "image-b.nii.gz");
  CHECK(params.imageFiles[1].segmentations.empty());
  CHECK_FALSE(params.projectFile);
}

TEST_CASE("standalone layout file can be used with positional image paths", "[common][input]")
{
  char app[] = "Entropy";
  char layoutsOpt[] = "--layouts";
  char layouts[] = "custom-layouts.json";
  char image0[] = "image-a.nii.gz";
  char image1[] = "image-b.nii.gz";

  std::array<char*, 5> argv{app, layoutsOpt, layouts, image0, image1};

  InputParams params;
  REQUIRE(parseCommandLine(static_cast<int>(argv.size()), argv.data(), params));

  REQUIRE(params.layoutsFile);
  CHECK(*params.layoutsFile == "custom-layouts.json");
  REQUIRE(params.imageFiles.size() == 2);
  CHECK(params.imageFiles[0].image == "image-a.nii.gz");
  CHECK(params.imageFiles[1].image == "image-b.nii.gz");
}

TEST_CASE("positional image paths cannot be mixed with explicit image options", "[common][input]")
{
  char app[] = "Entropy";
  char positionalImage[] = "image-a.nii.gz";
  char imageOpt[] = "--image";
  char explicitImage[] = "image-b.nii.gz";

  std::array<char*, 4> argv{app, positionalImage, imageOpt, explicitImage};

  InputParams params;
  CHECK_FALSE(parseCommandLine(static_cast<int>(argv.size()), argv.data(), params));
}

TEST_CASE("positional image paths cannot be mixed with segmentation options", "[common][input]")
{
  char app[] = "Entropy";
  char positionalImage[] = "image.nii.gz";
  char segOpt[] = "--seg";
  char seg[] = "seg.nii.gz";

  std::array<char*, 4> argv{app, positionalImage, segOpt, seg};

  InputParams params;
  CHECK_FALSE(parseCommandLine(static_cast<int>(argv.size()), argv.data(), params));
}

TEST_CASE("positional image paths cannot be mixed with DICOM options", "[common][input]")
{
  char app[] = "Entropy";
  char positionalImage[] = "image.nii.gz";
  char dicomOpt[] = "--dicom";
  char dicomPath[] = "dicom-folder";

  std::array<char*, 4> argv{app, positionalImage, dicomOpt, dicomPath};

  InputParams params;
  CHECK_FALSE(parseCommandLine(static_cast<int>(argv.size()), argv.data(), params));
}

TEST_CASE("explicit image paths cannot be mixed with DICOM options", "[common][input]")
{
  char app[] = "Entropy";
  char imageOpt[] = "--image";
  char image[] = "image.nii.gz";
  char dicomOpt[] = "--dicom";
  char dicomPath[] = "dicom-folder";

  std::array<char*, 5> argv{app, imageOpt, image, dicomOpt, dicomPath};

  InputParams params;
  CHECK_FALSE(parseCommandLine(static_cast<int>(argv.size()), argv.data(), params));
}

TEST_CASE("standalone layout file can override project layouts", "[common][input]")
{
  char app[] = "Entropy";
  char projectOpt[] = "--project";
  char project[] = "project.json";
  char layoutsOpt[] = "--layouts";
  char layouts[] = "custom-layouts.json";

  std::array<char*, 5> argv{app, projectOpt, project, layoutsOpt, layouts};

  InputParams params;
  REQUIRE(parseCommandLine(static_cast<int>(argv.size()), argv.data(), params));

  REQUIRE(params.projectFile);
  CHECK(*params.projectFile == "project.json");
  REQUIRE(params.layoutsFile);
  CHECK(*params.layoutsFile == "custom-layouts.json");
}

TEST_CASE("project option cannot be mixed with image, segmentation, or DICOM inputs", "[common][input]")
{
  SECTION("project with image")
  {
    char app[] = "Entropy";
    char projectOpt[] = "--project";
    char project[] = "project.json";
    char imageOpt[] = "--image";
    char image[] = "image.nii.gz";

    std::array<char*, 5> argv{app, projectOpt, project, imageOpt, image};

    InputParams params;
    CHECK_FALSE(parseCommandLine(static_cast<int>(argv.size()), argv.data(), params));
  }

  SECTION("project with segmentation")
  {
    char app[] = "Entropy";
    char projectOpt[] = "--project";
    char project[] = "project.json";
    char segOpt[] = "--seg";
    char seg[] = "seg.nii.gz";

    std::array<char*, 5> argv{app, projectOpt, project, segOpt, seg};

    InputParams params;
    CHECK_FALSE(parseCommandLine(static_cast<int>(argv.size()), argv.data(), params));
  }

  SECTION("project with DICOM")
  {
    char app[] = "Entropy";
    char projectOpt[] = "--project";
    char project[] = "project.json";
    char dicomOpt[] = "--dicom";
    char dicomPath[] = "dicom-folder";

    std::array<char*, 5> argv{app, projectOpt, project, dicomOpt, dicomPath};

    InputParams params;
    CHECK_FALSE(parseCommandLine(static_cast<int>(argv.size()), argv.data(), params));
  }
}
