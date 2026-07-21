#include "logic/app/LoadingStatusItems.h"

#include "logic/serialization/ProjectSerialization.h"

#include <catch2/catch_test_macros.hpp>

#include <fstream>

namespace
{
void writeBytes(const std::filesystem::path& fileName, std::size_t count)
{
  std::ofstream stream(fileName, std::ios::binary);
  for (std::size_t i = 0; i < count; ++i) {
    stream.put(static_cast<char>(i & 0xffu));
  }
}
} // namespace

TEST_CASE("Loading status image items preserve file order", "[LoadingStatusItems]")
{
  const std::vector<std::filesystem::path> files{"first.nii.gz", "second.nrrd"};

  const auto items = loading_status::imageItems(files);

  REQUIRE(items.size() == 2);
  CHECK(items[0].kind == GuiData::LoadingStatusItem::Kind::Image);
  CHECK(items[0].fileName == files[0]);
  CHECK(items[1].kind == GuiData::LoadingStatusItem::Kind::Image);
  CHECK(items[1].fileName == files[1]);
}

TEST_CASE("Loading status project items include images and segmentations", "[LoadingStatusItems]")
{
  serialize::EntropyProject project;
  project.m_referenceImage.m_imageFileName = "reference.nii.gz";
  project.m_referenceImage.m_inverseWarpFieldPath = "inverse-warp.nrrd";
  project.m_referenceImage.m_forwardWarpFieldPath = "forward-warp.nrrd";

  serialize::Segmentation segmentation;
  segmentation.m_segFileName = "reference-seg.nrrd";
  project.m_referenceImage.m_segmentations.push_back(segmentation);

  serialize::Image movingImage;
  movingImage.m_imageFileName = "moving.nii.gz";
  project.m_additionalImages.push_back(movingImage);

  const auto items = loading_status::projectItems(project);

  REQUIRE(items.size() == 5);
  CHECK(items[0].kind == GuiData::LoadingStatusItem::Kind::Image);
  CHECK(items[0].fileName == "reference.nii.gz");
  CHECK(items[1].kind == GuiData::LoadingStatusItem::Kind::Image);
  CHECK(items[1].fileName == "inverse-warp.nrrd");
  CHECK(items[2].kind == GuiData::LoadingStatusItem::Kind::Image);
  CHECK(items[2].fileName == "forward-warp.nrrd");
  CHECK(items[3].kind == GuiData::LoadingStatusItem::Kind::Segmentation);
  CHECK(items[3].fileName == "reference-seg.nrrd");
  CHECK(items[4].kind == GuiData::LoadingStatusItem::Kind::Image);
  CHECK(items[4].fileName == "moving.nii.gz");
}

TEST_CASE("Loading status project items include image and segmentation byte sizes", "[LoadingStatusItems]")
{
  const std::filesystem::path directory =
    std::filesystem::temp_directory_path() / "entropy-loading-status-byte-size-test";
  std::filesystem::create_directories(directory);
  const std::filesystem::path imageFile = directory / "reference.nrrd";
  const std::filesystem::path segmentationFile = directory / "segmentation.nrrd";
  writeBytes(imageFile, 11);
  writeBytes(segmentationFile, 7);

  serialize::EntropyProject project;
  project.m_referenceImage.m_imageFileName = imageFile;

  serialize::Segmentation segmentation;
  segmentation.m_segFileName = segmentationFile;
  project.m_referenceImage.m_segmentations.push_back(segmentation);

  const auto items = loading_status::projectItems(project);

  REQUIRE(items.size() == 2);
  CHECK(items[0].kind == GuiData::LoadingStatusItem::Kind::Image);
  REQUIRE(items[0].bytes);
  CHECK(*items[0].bytes == 11);
  CHECK(items[1].kind == GuiData::LoadingStatusItem::Kind::Segmentation);
  REQUIRE(items[1].bytes);
  CHECK(*items[1].bytes == 7);

  std::filesystem::remove_all(directory);
}

TEST_CASE("Loading status paths can match by exact path or filename", "[LoadingStatusItems]")
{
  CHECK(loading_status::equivalentPath("/tmp/image.nii.gz", "/tmp/image.nii.gz"));
  CHECK(loading_status::equivalentPath("/first/image.nii.gz", "/second/image.nii.gz"));
  CHECK_FALSE(loading_status::equivalentPath("/first/image.nii.gz", "/first/other.nii.gz"));
}
