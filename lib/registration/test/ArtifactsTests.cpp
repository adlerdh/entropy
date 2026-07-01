#include "registration/Artifacts.h"

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <utility>
#include <vector>

namespace
{

registration::JobSpec baseJob()
{
  registration::JobSpec job;
  job.outputDirectory = "/tmp/entropy-registration";
  job.outputPrefix = "moving_to_fixed";
  return job;
}

registration::DataRef dataRef(std::string uid, std::string fileName, registration::DataSource source)
{
  registration::DataRef ref;
  ref.uid = std::move(uid);
  ref.fileName = std::move(fileName);
  ref.displayName = ref.uid;
  ref.source = source;
  return ref;
}

} // namespace

TEST_CASE("registration artifact paths use the job output prefix", "[registration][artifacts]")
{
  const registration::JobSpec job = baseJob();

  CHECK(registration::outputPrefix(job) == "moving_to_fixed");
  CHECK(
    registration::artifactPath(job, registration::ArtifactRole::JobSpec) ==
    "/tmp/entropy-registration/moving_to_fixed_job.json");
  CHECK(
    registration::artifactPath(job, registration::ArtifactRole::ResultManifest) ==
    "/tmp/entropy-registration/moving_to_fixed_result.json");
  CHECK(
    registration::artifactPath(job, registration::ArtifactRole::InverseWarp) ==
    "/tmp/entropy-registration/moving_to_fixed_inverse_warp.nii.gz");
  CHECK(
    registration::artifactPath(job, registration::ArtifactRole::WarpedImage) ==
    "/tmp/entropy-registration/moving_to_fixed_warped.nii.gz");
}

TEST_CASE("registration repeated artifact paths are stable and readable", "[registration][artifacts]")
{
  const registration::JobSpec job = baseJob();

  CHECK(
    registration::artifactPath(job, registration::ArtifactRole::WarpedSegmentation, 0) ==
    "/tmp/entropy-registration/moving_to_fixed_warped_segmentation_01.nii.gz");
  CHECK(
    registration::artifactPath(job, registration::ArtifactRole::WarpedSegmentation, 12) ==
    "/tmp/entropy-registration/moving_to_fixed_warped_segmentation_13.nii.gz");
  CHECK(
    registration::artifactPath(job, registration::ArtifactRole::TransformedLandmarks, 1) ==
    "/tmp/entropy-registration/moving_to_fixed_transformed_landmarks_02.json");
}

TEST_CASE("registration artifact paths fall back to a generic prefix", "[registration][artifacts]")
{
  registration::JobSpec job;
  job.outputDirectory = "/tmp/entropy-registration";

  CHECK(registration::outputPrefix(job) == "registration");
  CHECK(
    registration::artifactPath(job, registration::ArtifactRole::AffineTransform) ==
    "/tmp/entropy-registration/registration_affine.mat");
}

TEST_CASE("registration affine initialization paths use backend-readable extensions", "[registration][artifacts]")
{
  registration::JobSpec job = baseJob();

  job.backend = registration::Backend::Greedy;
  CHECK(registration::initialAffineInputPath(job) == "/tmp/entropy-registration/moving_to_fixed_initial_affine.mat");

  job.backend = registration::Backend::ANTs;
  CHECK(registration::initialAffineInputPath(job) == "/tmp/entropy-registration/moving_to_fixed_initial_affine.tfm");

  job.backend = registration::Backend::FireANTs;
  CHECK(registration::initialAffineInputPath(job) == "/tmp/entropy-registration/moving_to_fixed_initial_affine.tfm");
}

TEST_CASE("registration input artifact plan distinguishes existing files from app exports", "[registration][artifacts]")
{
  registration::JobSpec job = baseJob();
  job.fixedMask = dataRef("fixed-mask", "fixed-mask.nii.gz", registration::DataSource::Segmentation);
  job.movingMask = dataRef("moving-mask", "", registration::DataSource::Segmentation);

  const std::vector<registration::InputArtifact> artifacts = registration::buildInputArtifactPlan(job);

  REQUIRE(artifacts.size() == 2);
  CHECK(artifacts.at(0).role == registration::ArtifactRole::FixedMask);
  CHECK(artifacts.at(0).path == "fixed-mask.nii.gz");
  CHECK_FALSE(artifacts.at(0).exportRequired);
  CHECK(artifacts.at(1).role == registration::ArtifactRole::MovingMask);
  CHECK(artifacts.at(1).path == "/tmp/entropy-registration/moving_to_fixed_moving_mask.nii.gz");
  CHECK(artifacts.at(1).exportRequired);
}

TEST_CASE(
  "registration input artifact plan includes auxiliary data landmarks and surfaces",
  "[registration][artifacts]")
{
  registration::JobSpec job = baseJob();
  registration::AuxiliaryImagePair pair;
  pair.fixed = dataRef("aux-fixed", "", registration::DataSource::LoadedImage);
  pair.moving = dataRef("aux-moving", "aux-moving.nii.gz", registration::DataSource::LoadedImage);
  job.auxiliaryImagePairs.push_back(pair);
  job.landmarks.enabled = true;
  job.landmarks.fixedLandmarks = dataRef("fixed-points", "", registration::DataSource::LandmarkGroup);
  job.landmarks.movingLandmarks =
    dataRef("moving-points", "moving-points.json", registration::DataSource::LandmarkGroup);
  job.surfaces.push_back(dataRef("surface", "", registration::DataSource::Surface));

  const std::vector<registration::InputArtifact> artifacts = registration::buildInputArtifactPlan(job);

  REQUIRE(artifacts.size() == 5);
  CHECK(artifacts.at(0).role == registration::ArtifactRole::AuxiliaryFixedImage);
  CHECK(artifacts.at(0).path == "/tmp/entropy-registration/moving_to_fixed_aux_fixed_01.nii.gz");
  CHECK(artifacts.at(0).exportRequired);
  CHECK(artifacts.at(1).role == registration::ArtifactRole::AuxiliaryMovingImage);
  CHECK(artifacts.at(1).path == "aux-moving.nii.gz");
  CHECK_FALSE(artifacts.at(1).exportRequired);
  CHECK(artifacts.at(2).role == registration::ArtifactRole::FixedLandmarks);
  CHECK(artifacts.at(2).path == "/tmp/entropy-registration/moving_to_fixed_fixed_landmarks.json");
  CHECK(artifacts.at(3).role == registration::ArtifactRole::MovingLandmarks);
  CHECK(artifacts.at(3).path == "moving-points.json");
  CHECK(artifacts.at(4).role == registration::ArtifactRole::Surface);
  CHECK(artifacts.at(4).path == "/tmp/entropy-registration/moving_to_fixed_surface_01.vtk");
  CHECK(artifacts.at(4).exportRequired);
}

TEST_CASE("registration expected result manifest follows requested outputs", "[registration][artifacts]")
{
  registration::JobSpec job = baseJob();
  job.backend = registration::Backend::Greedy;
  job.fixedImage = dataRef("fixed", "fixed.nii.gz", registration::DataSource::LoadedImage);
  job.movingImage = dataRef("moving", "moving.nii.gz", registration::DataSource::LoadedImage);
  job.outputs.loadWarpedSegmentation = true;
  job.outputs.transformLandmarksAndAnnotations = true;
  job.outputs.transformSurfaces = true;

  const registration::ResultManifest manifest = registration::buildExpectedResultManifest(job);

  CHECK(manifest.backend == registration::Backend::Greedy);
  CHECK(manifest.fixedImageUid == "fixed");
  CHECK(manifest.movingImageUid == "moving");
  CHECK(manifest.warpedImage == "/tmp/entropy-registration/moving_to_fixed_warped.nii.gz");
  CHECK(manifest.inverseWarp == "/tmp/entropy-registration/moving_to_fixed_inverse_warp.nii.gz");
  CHECK(manifest.forwardWarp == "/tmp/entropy-registration/moving_to_fixed_forward_warp.nii.gz");
  REQUIRE(manifest.warpedSegmentations.size() == 1);
  CHECK(
    manifest.warpedSegmentations.front() == "/tmp/entropy-registration/moving_to_fixed_warped_segmentation_01.nii.gz");
  REQUIRE(manifest.transformedLandmarks.size() == 1);
  CHECK(
    manifest.transformedLandmarks.front() == "/tmp/entropy-registration/moving_to_fixed_transformed_landmarks_01.json");
  REQUIRE(manifest.transformedSurfaces.size() == 1);
  CHECK(manifest.transformedSurfaces.front() == "/tmp/entropy-registration/moving_to_fixed_transformed_surface_01.vtk");
}

TEST_CASE("registration expected result manifest omits warps for affine-only transforms", "[registration][artifacts]")
{
  for (const registration::TransformModel model :
       {registration::TransformModel::Rigid,
        registration::TransformModel::Affine,
        registration::TransformModel::RigidAffine})
  {
    registration::JobSpec job = baseJob();
    job.transformModel = model;
    job.outputs.loadInverseWarp = true;
    job.outputs.loadForwardWarp = true;
    job.outputs.applyWarpToMovingImage = true;
    job.outputs.transformLandmarksAndAnnotations = true;
    job.outputs.transformSurfaces = true;

    const registration::ResultManifest manifest = registration::buildExpectedResultManifest(job);

    CHECK(manifest.inverseWarp.empty());
    CHECK(manifest.forwardWarp.empty());
    CHECK_FALSE(manifest.affineTransform.empty());
  }
}

TEST_CASE(
  "registration expected result manifest omits affine for deformable-only transforms",
  "[registration][artifacts]")
{
  registration::JobSpec job = baseJob();
  job.transformModel = registration::TransformModel::Deformable;
  job.outputs.loadAffineTransform = true;

  const registration::ResultManifest manifest = registration::buildExpectedResultManifest(job);

  CHECK(manifest.affineTransform.empty());
}

TEST_CASE("registration expected result manifest uses ANTs warp output names", "[registration][artifacts]")
{
  registration::JobSpec job = baseJob();
  job.backend = registration::Backend::ANTs;
  job.transformModel = registration::TransformModel::AffineDeformable;
  job.outputs.loadInverseWarp = true;
  job.outputs.loadForwardWarp = true;

  registration::ResultManifest manifest = registration::buildExpectedResultManifest(job);

  CHECK(manifest.inverseWarp == "/tmp/entropy-registration/moving_to_fixed1Warp.nii.gz");
  CHECK(manifest.forwardWarp == "/tmp/entropy-registration/moving_to_fixed1InverseWarp.nii.gz");

  job.transformModel = registration::TransformModel::Deformable;
  manifest = registration::buildExpectedResultManifest(job);

  CHECK(manifest.inverseWarp == "/tmp/entropy-registration/moving_to_fixed0Warp.nii.gz");
  CHECK(manifest.forwardWarp == "/tmp/entropy-registration/moving_to_fixed0InverseWarp.nii.gz");

  job.initialAffineTransform = "/tmp/entropy-registration/moving_to_fixed_initial_affine.tfm";
  manifest = registration::buildExpectedResultManifest(job);

  CHECK(manifest.inverseWarp == "/tmp/entropy-registration/moving_to_fixed1Warp.nii.gz");
  CHECK(manifest.forwardWarp == "/tmp/entropy-registration/moving_to_fixed1InverseWarp.nii.gz");
}

TEST_CASE(
  "registration expected result manifest predicts ANTs warp names even when import toggles are off",
  "[registration][artifacts]")
{
  registration::JobSpec job = baseJob();
  job.backend = registration::Backend::ANTs;
  job.transformModel = registration::TransformModel::AffineDeformable;
  job.outputs.loadInverseWarp = false;
  job.outputs.loadForwardWarp = false;
  job.outputs.applyWarpToMovingImage = false;

  const registration::ResultManifest manifest = registration::buildExpectedResultManifest(job);

  CHECK(manifest.inverseWarp == "/tmp/entropy-registration/moving_to_fixed1Warp.nii.gz");
  CHECK(manifest.forwardWarp == "/tmp/entropy-registration/moving_to_fixed1InverseWarp.nii.gz");
}
