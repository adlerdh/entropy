#include "registration/ImportPlan.h"

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <utility>

namespace
{

registration::DataRef imageRef(std::string uid, std::string name)
{
  registration::DataRef ref;
  ref.uid = std::move(uid);
  ref.displayName = std::move(name);
  ref.fileName = ref.displayName + ".nii.gz";
  ref.source = registration::DataSource::LoadedImage;
  return ref;
}

registration::JobSpec job()
{
  registration::JobSpec spec;
  spec.fixedImage = imageRef("fixed-uid", "Fixed");
  spec.movingImage = imageRef("moving-uid", "Moving");
  spec.outputs.loadWarpedSegmentation = true;
  spec.outputs.transformSurfaces = true;
  spec.outputs.makeWarpedImageActive = true;
  return spec;
}

registration::ResultManifest manifest()
{
  registration::ResultManifest result;
  result.success = true;
  result.inverseWarp = "inverse.nii.gz";
  result.forwardWarp = "forward.nii.gz";
  result.warpedImage = "warped.nii.gz";
  result.warpedSegmentations = {"seg.nii.gz"};
  result.transformedLandmarks = {"landmarks.json"};
  result.transformedSurfaces = {"surface.vtk"};
  return result;
}

} // namespace

TEST_CASE("registration import plan names warped outputs", "[registration][import]")
{
  const registration::JobSpec spec = job();

  CHECK(registration::warpedImageName(spec.fixedImage, spec.movingImage) == "Moving registered to Fixed");
  CHECK(registration::inverseWarpName(spec.fixedImage, spec.movingImage) == "Moving inverse warp to Fixed");
  CHECK(registration::forwardWarpName(spec.fixedImage, spec.movingImage) == "Moving forward warp from Fixed");
}

TEST_CASE("registration import plan orders warp assignment before loaded warped image", "[registration][import]")
{
  const registration::ImportPlan plan = registration::buildImportPlan(job(), manifest());

  REQUIRE(plan.steps.size() == 8);
  CHECK(plan.steps.at(0).action == registration::ImportAction::LoadInverseWarp);
  CHECK(plan.steps.at(0).displayName == "Moving inverse warp to Fixed");
  CHECK(plan.steps.at(1).action == registration::ImportAction::LoadForwardWarp);
  CHECK(plan.steps.at(2).action == registration::ImportAction::AssignWarpsToMovingImage);
  CHECK(plan.steps.at(3).action == registration::ImportAction::LoadWarpedImage);
  CHECK(plan.steps.at(3).displayName == "Moving registered to Fixed");
  CHECK(plan.steps.back().action == registration::ImportAction::MakeWarpedImageActive);
}

TEST_CASE("registration import plan respects disabled outputs", "[registration][import]")
{
  registration::JobSpec spec = job();
  spec.outputs.loadWarpedImage = false;
  spec.outputs.loadInverseWarp = false;
  spec.outputs.loadForwardWarp = false;
  spec.outputs.applyWarpToMovingImage = false;
  spec.outputs.loadWarpedSegmentation = false;
  spec.outputs.transformLandmarksAndAnnotations = false;
  spec.outputs.transformSurfaces = false;
  spec.outputs.makeWarpedImageActive = false;

  const registration::ImportPlan plan = registration::buildImportPlan(spec, manifest());

  CHECK(plan.steps.empty());
  CHECK(plan.warnings.empty());
}

TEST_CASE("registration import plan warns for requested missing artifacts", "[registration][import]")
{
  registration::ResultManifest result;
  result.warnings = {"backend warning"};

  const registration::ImportPlan plan = registration::buildImportPlan(job(), result);

  CHECK(plan.steps.empty());
  REQUIRE(plan.warnings.size() == 6);
  CHECK(plan.warnings.front() == "backend warning");
}
