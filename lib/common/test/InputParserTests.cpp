#include "common/InputParser.h"

#include <catch2/catch_test_macros.hpp>

#include <array>

TEST_CASE("macOS LaunchServices process serial number argument is ignored", "[common][input]")
{
  char app[] = "Entropy";
  char psn[] = "-psn_0_1327411";

  std::array<char*, 2> argv{app, psn};

  InputParams params;
  REQUIRE(parseCommandLine(static_cast<int>(argv.size()), argv.data(), params));

  CHECK_FALSE(params.set);
  CHECK(params.imageFiles.empty());
  CHECK_FALSE(params.projectFile);
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
