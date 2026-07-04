#include "ui/windows/LoadingStatusModel.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

namespace model = entropy::ui::loading_status_model;

TEST_CASE("Loading status progress is weighted by bytes", "[ui][loading_status]")
{
  const std::vector<GuiData::LoadingStatusItem> items{
    {GuiData::LoadingStatusItem::Kind::Image, "image-a.nrrd", 100u, true},
    {GuiData::LoadingStatusItem::Kind::Image, "image-b.nrrd", 300u, false}};

  const model::LoadingProgress progress = model::loadingProgress(items);

  CHECK(progress.loaded == 100u);
  CHECK(progress.total == 400u);
  CHECK(model::progressFraction(progress) == Catch::Approx(0.25f));
  CHECK(model::progressPercentLabel(model::progressFraction(progress)) == "25%");
}

TEST_CASE("Loading status progress includes segmentations", "[ui][loading_status]")
{
  const std::vector<GuiData::LoadingStatusItem> items{
    {GuiData::LoadingStatusItem::Kind::Image, "image.nrrd", 100u, true},
    {GuiData::LoadingStatusItem::Kind::Segmentation, "seg.nrrd", 50u, true},
    {GuiData::LoadingStatusItem::Kind::Image, "moving.nrrd", 150u, false}};

  const model::LoadingProgress progress = model::loadingProgress(items);

  CHECK(progress.loaded == 150u);
  CHECK(progress.total == 300u);
  CHECK(model::progressFraction(progress) == Catch::Approx(0.5f));
}

TEST_CASE("Loading status progress gives unknown-size items fallback weight", "[ui][loading_status]")
{
  const std::vector<GuiData::LoadingStatusItem> items{
    {GuiData::LoadingStatusItem::Kind::Image, "image.nrrd", std::nullopt, true},
    {GuiData::LoadingStatusItem::Kind::Segmentation, "seg.nrrd", std::nullopt, false}};

  const model::LoadingProgress progress = model::loadingProgress(items);

  CHECK(progress.loaded == 1u);
  CHECK(progress.total == 2u);
  CHECK(model::progressFraction(progress) == Catch::Approx(0.5f));
}
