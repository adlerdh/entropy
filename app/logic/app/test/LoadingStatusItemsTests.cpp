#include "logic/app/LoadingStatusItems.h"

#include "logic/serialization/ProjectSerialization.h"

#include <catch2/catch_test_macros.hpp>

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

  serialize::Segmentation segmentation;
  segmentation.m_segFileName = "reference-seg.nrrd";
  project.m_referenceImage.m_segmentations.push_back(segmentation);

  serialize::Image movingImage;
  movingImage.m_imageFileName = "moving.nii.gz";
  project.m_additionalImages.push_back(movingImage);

  const auto items = loading_status::projectItems(project);

  REQUIRE(items.size() == 3);
  CHECK(items[0].kind == GuiData::LoadingStatusItem::Kind::Image);
  CHECK(items[0].fileName == "reference.nii.gz");
  CHECK(items[1].kind == GuiData::LoadingStatusItem::Kind::Segmentation);
  CHECK(items[1].fileName == "reference-seg.nrrd");
  CHECK(items[2].kind == GuiData::LoadingStatusItem::Kind::Image);
  CHECK(items[2].fileName == "moving.nii.gz");
}

TEST_CASE("Loading status paths can match by exact path or filename", "[LoadingStatusItems]")
{
  CHECK(loading_status::equivalentPath("/tmp/image.nii.gz", "/tmp/image.nii.gz"));
  CHECK(loading_status::equivalentPath("/first/image.nii.gz", "/second/image.nii.gz"));
  CHECK_FALSE(loading_status::equivalentPath("/first/image.nii.gz", "/first/other.nii.gz"));
}
