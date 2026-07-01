#include "registration/ImportPlan.h"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
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
  result.affineTransform = "affine.mat";
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

TEST_CASE("registration import plan loads warped image before warp fields", "[registration][import]")
{
  const registration::ImportPlan plan = registration::buildImportPlan(job(), manifest());

  REQUIRE(plan.steps.size() == 9);
  CHECK(plan.steps.at(0).action == registration::ImportAction::ApplyAffineTransform);
  CHECK(plan.steps.at(1).action == registration::ImportAction::LoadWarpedImage);
  CHECK(plan.steps.at(1).displayName == "Moving registered to Fixed");
  CHECK(plan.steps.at(2).action == registration::ImportAction::LoadInverseWarp);
  CHECK(plan.steps.at(2).displayName == "Moving inverse warp to Fixed");
  CHECK(plan.steps.at(3).action == registration::ImportAction::LoadForwardWarp);
  CHECK(plan.steps.at(4).action == registration::ImportAction::AssignWarpsToMovingImage);
  CHECK(plan.steps.back().action == registration::ImportAction::MakeWarpedImageActive);
}

TEST_CASE("registration import plan respects disabled outputs", "[registration][import]")
{
  registration::JobSpec spec = job();
  spec.outputs.loadWarpedImage = false;
  spec.outputs.loadAffineTransform = false;
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

TEST_CASE("registration import plan respects disabled affine transform import", "[registration][import]")
{
  registration::JobSpec spec = job();
  spec.outputs.loadAffineTransform = false;

  const registration::ImportPlan plan = registration::buildImportPlan(spec, manifest());

  CHECK(std::none_of(plan.steps.begin(), plan.steps.end(), [](const registration::ImportStep& step) {
    return step.action == registration::ImportAction::ApplyAffineTransform;
  }));
  CHECK(std::any_of(plan.steps.begin(), plan.steps.end(), [](const registration::ImportStep& step) {
    return step.action == registration::ImportAction::LoadWarpedImage;
  }));
}

TEST_CASE("registration import plan ignores warp artifacts for affine-only transforms", "[registration][import]")
{
  for (const registration::TransformModel model :
       {registration::TransformModel::Rigid,
        registration::TransformModel::Affine,
        registration::TransformModel::RigidAffine})
  {
    registration::JobSpec spec = job();
    spec.transformModel = model;

    const registration::ImportPlan plan = registration::buildImportPlan(spec, manifest());

    REQUIRE_FALSE(plan.steps.empty());
    CHECK(plan.steps.front().action == registration::ImportAction::ApplyAffineTransform);
    CHECK(std::none_of(plan.steps.begin(), plan.steps.end(), [](const registration::ImportStep& step) {
      return step.action == registration::ImportAction::LoadInverseWarp ||
             step.action == registration::ImportAction::LoadForwardWarp ||
             step.action == registration::ImportAction::AssignWarpsToMovingImage;
    }));
  }
}

TEST_CASE("registration import plan fills omitted ANTs warp paths", "[registration][import]")
{
  registration::JobSpec spec = job();
  spec.backend = registration::Backend::ANTs;
  spec.transformModel = registration::TransformModel::AffineDeformable;
  spec.outputDirectory = "/tmp/entropy-registration";
  spec.outputPrefix = "moving_to_fixed";

  registration::ResultManifest result = manifest();
  result.inverseWarp.clear();
  result.forwardWarp.clear();

  const registration::ImportPlan plan = registration::buildImportPlan(spec, result);

  auto stepWithAction = [&plan](registration::ImportAction action) {
    return std::find_if(plan.steps.begin(), plan.steps.end(), [&](const registration::ImportStep& step) {
      return step.action == action;
    });
  };

  const auto inverseStep = stepWithAction(registration::ImportAction::LoadInverseWarp);
  REQUIRE(inverseStep != plan.steps.end());
  CHECK(inverseStep->path == "/tmp/entropy-registration/moving_to_fixed1Warp.nii.gz");

  const auto forwardStep = stepWithAction(registration::ImportAction::LoadForwardWarp);
  REQUIRE(forwardStep != plan.steps.end());
  CHECK(forwardStep->path == "/tmp/entropy-registration/moving_to_fixed1InverseWarp.nii.gz");

  spec.transformModel = registration::TransformModel::Deformable;
  spec.initialAffineTransform = "/tmp/entropy-registration/moving_to_fixed_initial_affine.tfm";

  const registration::ImportPlan deformableWithInitialPlan = registration::buildImportPlan(spec, result);
  const auto initializedInverseStep = std::find_if(
    deformableWithInitialPlan.steps.begin(),
    deformableWithInitialPlan.steps.end(),
    [](const registration::ImportStep& step) { return step.action == registration::ImportAction::LoadInverseWarp; });
  REQUIRE(initializedInverseStep != deformableWithInitialPlan.steps.end());
  CHECK(initializedInverseStep->path == "/tmp/entropy-registration/moving_to_fixed1Warp.nii.gz");
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
