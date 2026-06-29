#include "registration/Commands.h"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <iterator>

namespace
{

registration::DataRef imageRef(std::string uid, std::string path)
{
  registration::DataRef ref;
  ref.uid = std::move(uid);
  ref.fileName = std::move(path);
  ref.displayName = ref.uid;
  ref.source = registration::DataSource::LoadedImage;
  return ref;
}

registration::JobSpec baseJob(registration::Backend backend)
{
  registration::JobSpec job;
  job.backend = backend;
  job.dimension = 3;
  job.fixedImage = imageRef("fixed", "fixed.nii.gz");
  job.movingImage = imageRef("moving", "moving.nii.gz");
  job.outputDirectory = "/tmp/reg";
  job.outputPrefix = "moving_to_fixed";
  return job;
}

std::string argumentAfter(const std::vector<std::string>& args, const std::string& key)
{
  const auto it = std::find(args.begin(), args.end(), key);
  if (it == args.end() || std::next(it) == args.end()) {
    return {};
  }
  return *std::next(it);
}

} // namespace

TEST_CASE("Greedy command generation emits affine deformable and reslice steps", "[registration][commands]")
{
  registration::JobSpec job = baseJob(registration::Backend::Greedy);
  job.transformModel = registration::TransformModel::AffineDeformable;
  job.metric = registration::Metric::WNCC;
  job.fixedMask = imageRef("mask", "mask.nii.gz");

  const std::vector<registration::CommandSpec> commands = registration::generateCommands(job);

  REQUIRE(commands.size() == 3);
  CHECK(commands.at(0).description == "Greedy affine registration");
  CHECK(commands.at(0).executable == "greedy");
  CHECK(commands.at(0).args.at(0) == "-d");
  CHECK(commands.at(0).args.at(1) == "3");
  CHECK(commands.at(1).description == "Greedy deformable registration");
  CHECK(commands.at(2).description == "Greedy reslice warped image");

  const std::string preview = registration::displayCommand(commands.at(1));
  CHECK(preview.find("-oinv") != std::string::npos);
  CHECK(preview.find("moving_to_fixed_forward_warp.nii.gz") != std::string::npos);
}

TEST_CASE("Greedy command generation uses selected parameter values", "[registration][commands]")
{
  registration::JobSpec job = baseJob(registration::Backend::Greedy);
  job.transformModel = registration::TransformModel::AffineDeformable;
  job.parameterValues = {{"iterations", "12x6"}, {"wnccRadius", "4"}, {"threads", "3"}, {"singlePrecision", "true"}};

  const std::vector<registration::CommandSpec> commands = registration::generateCommands(job);

  REQUIRE(commands.size() == 3);
  const std::string preview = registration::displayCommand(commands.at(0));
  CHECK(preview.find("12x6") != std::string::npos);
  CHECK(preview.find("4x4x4") != std::string::npos);
  CHECK(preview.find("-threads 3") != std::string::npos);
  CHECK(preview.find("-float") != std::string::npos);
}

TEST_CASE("Greedy command generation emits advanced affine and deformable options", "[registration][commands]")
{
  registration::JobSpec job = baseJob(registration::Backend::Greedy);
  job.transformModel = registration::TransformModel::AffineDeformable;
  job.parameterValues = {
    {"smoothingUnits", "mm"},
    {"smoothingSigma", "2,0.5"},
    {"stepSize", "0.75"},
    {"velocityModel", "svlb"},
    {"inverseExponentiation", "5"},
    {"fixedMaskTrim", "4x4x4"},
    {"affineJitter", "0.25"},
    {"affineSearchIterations", "20"},
    {"affineSearchRotation", "flip"},
    {"affineSearchTranslation", "25"},
    {"affineMoments", "2"},
    {"affineDeterminant", "1"},
    {"warpPrecision", "0.01"}};

  const std::vector<registration::CommandSpec> commands = registration::generateCommands(job);

  REQUIRE(commands.size() == 3);
  const std::string affinePreview = registration::displayCommand(commands.at(0));
  CHECK(affinePreview.find("-gm-trim 4x4x4") != std::string::npos);
  CHECK(affinePreview.find("-jitter 0.25") != std::string::npos);
  CHECK(affinePreview.find("-search 20 flip 25") != std::string::npos);
  CHECK(affinePreview.find("-ia-moments 2") != std::string::npos);
  CHECK(affinePreview.find("-det 1") != std::string::npos);

  const std::string deformablePreview = registration::displayCommand(commands.at(1));
  CHECK(deformablePreview.find("-s 2mm 0.5mm") != std::string::npos);
  CHECK(deformablePreview.find("-e 0.75") != std::string::npos);
  CHECK(deformablePreview.find("-svlb") != std::string::npos);
  CHECK(deformablePreview.find("-wp 0.01") != std::string::npos);
  CHECK(deformablePreview.find("-exp 5 -oinv") != std::string::npos);
}

TEST_CASE("Greedy command generation uses current affine initialization path", "[registration][commands]")
{
  registration::JobSpec job = baseJob(registration::Backend::Greedy);
  job.transformModel = registration::TransformModel::Affine;
  job.useCurrentAffineTransformsForInitialization = true;
  job.useImageCentersForInitialization = false;
  job.initialAffineTransform = "/tmp/reg/initial_affine.mat";

  const std::vector<registration::CommandSpec> commands = registration::generateCommands(job);

  REQUIRE_FALSE(commands.empty());
  const std::string preview = registration::displayCommand(commands.front());
  CHECK(preview.find("-ia /tmp/reg/initial_affine.mat") != std::string::npos);
  CHECK(preview.find("-ia-image-centers") == std::string::npos);
}

TEST_CASE("Greedy command generation constrains affine degrees of freedom", "[registration][commands]")
{
  auto affineDofFor = [](registration::TransformModel model) {
    registration::JobSpec job = baseJob(registration::Backend::Greedy);
    job.transformModel = model;
    job.outputs.loadWarpedImage = false;
    job.outputs.loadInverseWarp = false;
    job.outputs.loadForwardWarp = false;

    const std::vector<registration::CommandSpec> commands = registration::generateCommands(job);

    REQUIRE_FALSE(commands.empty());
    return argumentAfter(commands.front().args, "-dof");
  };

  CHECK(affineDofFor(registration::TransformModel::Rigid) == "6");
  CHECK(affineDofFor(registration::TransformModel::Similarity) == "7");
  CHECK(affineDofFor(registration::TransformModel::Affine) == "12");
  CHECK(affineDofFor(registration::TransformModel::RigidAffine) == "12");
  CHECK(affineDofFor(registration::TransformModel::AffineDeformable) == "12");
}

TEST_CASE("Greedy command generation emits masks and auxiliary image pairs", "[registration][commands]")
{
  registration::JobSpec job = baseJob(registration::Backend::Greedy);
  job.transformModel = registration::TransformModel::AffineDeformable;
  job.fixedMask = imageRef("fixed-mask", "fixed_mask.nii.gz");
  job.movingMask = imageRef("moving-mask", "moving_mask.nii.gz");
  registration::AuxiliaryImagePair pair;
  pair.fixed = imageRef("aux-fixed", "aux_fixed.nii.gz");
  pair.moving = imageRef("aux-moving", "aux_moving.nii.gz");
  pair.weight = 0.5;
  job.auxiliaryImagePairs.push_back(pair);

  const std::vector<registration::CommandSpec> commands = registration::generateCommands(job);

  REQUIRE(commands.size() == 3);
  const std::string affinePreview = registration::displayCommand(commands.at(0));
  CHECK(affinePreview.find("-gm fixed_mask.nii.gz") != std::string::npos);
  CHECK(affinePreview.find("-w 0.500000 -i aux_fixed.nii.gz aux_moving.nii.gz") != std::string::npos);

  const std::string deformablePreview = registration::displayCommand(commands.at(1));
  CHECK(deformablePreview.find("-gm fixed_mask.nii.gz") != std::string::npos);
  CHECK(deformablePreview.find("-mm moving_mask.nii.gz") != std::string::npos);
  CHECK(deformablePreview.find("-w 0.500000 -i aux_fixed.nii.gz aux_moving.nii.gz") != std::string::npos);
}

TEST_CASE(
  "registration command generation appends expert arguments to registration commands",
  "[registration][commands]")
{
  registration::JobSpec job = baseJob(registration::Backend::Greedy);
  job.transformModel = registration::TransformModel::AffineDeformable;
  job.parameterValues = {{"extraArgs", "-debug -dump-pyramid"}};
  job.extraArguments = {"-extra-file", "debug.txt"};

  const std::vector<registration::CommandSpec> commands = registration::generateCommands(job);

  REQUIRE(commands.size() == 3);
  CHECK(
    registration::displayCommand(commands.at(0)).find("-debug -dump-pyramid -extra-file debug.txt") !=
    std::string::npos);
  CHECK(
    registration::displayCommand(commands.at(1)).find("-debug -dump-pyramid -extra-file debug.txt") !=
    std::string::npos);
  CHECK(registration::displayCommand(commands.at(2)).find("-debug") == std::string::npos);
}

TEST_CASE("Greedy command generation reslices moving segmentation when requested", "[registration][commands]")
{
  registration::JobSpec job = baseJob(registration::Backend::Greedy);
  job.transformModel = registration::TransformModel::AffineDeformable;
  job.outputs.loadWarpedSegmentation = true;
  job.movingMask = imageRef("moving-seg", "moving_seg.nii.gz");

  const std::vector<registration::CommandSpec> commands = registration::generateCommands(job);

  REQUIRE(commands.size() == 4);
  CHECK(commands.back().description == "Greedy reslice warped segmentation");
  const std::string preview = registration::displayCommand(commands.back());
  CHECK(preview.find("-ri LABEL 0.2vox") != std::string::npos);
  CHECK(preview.find("moving_seg.nii.gz") != std::string::npos);
  CHECK(preview.find("moving_to_fixed_warped_segmentation_01.nii.gz") != std::string::npos);
}

TEST_CASE("ANTs command generation includes staged output and metric", "[registration][commands]")
{
  registration::JobSpec job = baseJob(registration::Backend::ANTs);
  job.transformModel = registration::TransformModel::AffineDeformable;
  job.metric = registration::Metric::CC;

  const std::vector<registration::CommandSpec> commands = registration::generateCommands(job);

  REQUIRE(commands.size() == 1);
  CHECK(commands.front().executable == "antsRegistration");
  const std::string preview = registration::displayCommand(commands.front());
  CHECK(preview.find("SyN") != std::string::npos);
  CHECK(preview.find("CC[fixed.nii.gz,moving.nii.gz,1,4]") != std::string::npos);
}

TEST_CASE("ANTs command generation uses selected pyramid and convergence parameters", "[registration][commands]")
{
  registration::JobSpec job = baseJob(registration::Backend::ANTs);
  job.transformModel = registration::TransformModel::AffineDeformable;
  job.metric = registration::Metric::CC;
  job.parameterValues = {
    {"iterations", "80x20"},
    {"shrinkFactors", "8x2"},
    {"smoothingSigmas", "3x0"},
    {"convergenceThreshold", "1e-5"},
    {"convergenceWindow", "5"}};

  const std::vector<registration::CommandSpec> commands = registration::generateCommands(job);

  REQUIRE(commands.size() == 1);
  const std::string preview = registration::displayCommand(commands.front());
  CHECK(preview.find("[80x20,1e-5,5]") != std::string::npos);
  CHECK(preview.find("--shrink-factors 8x2") != std::string::npos);
  CHECK(preview.find("--smoothing-sigmas 3x0vox") != std::string::npos);
}

TEST_CASE("ANTs command generation emits masks and auxiliary metrics", "[registration][commands]")
{
  registration::JobSpec job = baseJob(registration::Backend::ANTs);
  job.transformModel = registration::TransformModel::AffineDeformable;
  job.metric = registration::Metric::CC;
  job.fixedMask = imageRef("fixed-mask", "fixed_mask.nii.gz");
  job.movingMask = imageRef("moving-mask", "moving_mask.nii.gz");
  registration::AuxiliaryImagePair pair;
  pair.fixed = imageRef("aux-fixed", "aux_fixed.nii.gz");
  pair.moving = imageRef("aux-moving", "aux_moving.nii.gz");
  pair.metric = registration::Metric::MSE;
  pair.weight = 0.25;
  job.auxiliaryImagePairs.push_back(pair);

  const std::vector<registration::CommandSpec> commands = registration::generateCommands(job);

  REQUIRE(commands.size() == 1);
  const std::string preview = registration::displayCommand(commands.front());
  CHECK(preview.find("--masks '[fixed_mask.nii.gz,moving_mask.nii.gz]'") != std::string::npos);
  CHECK(preview.find("--metric 'MeanSquares[aux_fixed.nii.gz,aux_moving.nii.gz,0.250000,4]'") != std::string::npos);
}

TEST_CASE("ANTs command generation uses current affine initialization path", "[registration][commands]")
{
  registration::JobSpec job = baseJob(registration::Backend::ANTs);
  job.transformModel = registration::TransformModel::Affine;
  job.useCurrentAffineTransformsForInitialization = true;
  job.initialAffineTransform = "/tmp/reg/initial_affine.mat";

  const std::vector<registration::CommandSpec> commands = registration::generateCommands(job);

  REQUIRE(commands.size() == 1);
  const std::string preview = registration::displayCommand(commands.front());
  CHECK(preview.find("--initial-moving-transform /tmp/reg/initial_affine.mat") != std::string::npos);
}

TEST_CASE("ANTs command generation reslices moving segmentation when requested", "[registration][commands]")
{
  registration::JobSpec job = baseJob(registration::Backend::ANTs);
  job.transformModel = registration::TransformModel::AffineDeformable;
  job.outputs.loadWarpedSegmentation = true;
  job.movingMask = imageRef("moving-seg", "moving_seg.nii.gz");

  const std::vector<registration::CommandSpec> commands = registration::generateCommands(job);

  REQUIRE(commands.size() == 2);
  CHECK(commands.back().description == "ANTs reslice warped segmentation");
  CHECK(commands.back().executable == "antsApplyTransforms");
  const std::string preview = registration::displayCommand(commands.back());
  CHECK(preview.find("-n GenericLabel") != std::string::npos);
  CHECK(preview.find("moving_to_fixed_warped_segmentation_01.nii.gz") != std::string::npos);
  CHECK(preview.find("moving_to_fixed1Warp.nii.gz") != std::string::npos);
  CHECK(preview.find("moving_to_fixed0GenericAffine.mat") != std::string::npos);
}

TEST_CASE("FireANTs command generation uses the bridge module", "[registration][commands]")
{
  registration::JobSpec job = baseJob(registration::Backend::FireANTs);

  const std::vector<registration::CommandSpec> commands = registration::generateCommands(job);

  REQUIRE(commands.size() == 1);
  CHECK(commands.front().executable == "python");
  CHECK(commands.front().args.at(1) == "entropy_fireants_bridge");
  CHECK(registration::displayCommand(commands.front()).find("moving_to_fixed_job.json") != std::string::npos);
}

TEST_CASE("command generation uses configured backend executable paths", "[registration][commands]")
{
  registration::CommandGenerationOptions options;
  options.greedyExecutable = "/opt/greedy/bin/greedy";
  options.antsRegistrationExecutable = "/opt/ants/bin/antsRegistration";
  options.antsApplyTransformsExecutable = "/opt/ants/bin/antsApplyTransforms";
  options.fireAntsPythonExecutable = "/venv/bin/python";
  options.fireAntsBridgeModule = "custom_bridge";

  CHECK(
    registration::generateCommands(baseJob(registration::Backend::Greedy), options).front().executable ==
    "/opt/greedy/bin/greedy");
  CHECK(
    registration::generateCommands(baseJob(registration::Backend::ANTs), options).front().executable ==
    "/opt/ants/bin/antsRegistration");

  registration::JobSpec antsJob = baseJob(registration::Backend::ANTs);
  antsJob.outputs.loadWarpedSegmentation = true;
  antsJob.movingMask = imageRef("moving-seg", "moving_seg.nii.gz");
  const std::vector<registration::CommandSpec> antsCommands = registration::generateCommands(antsJob, options);
  REQUIRE(antsCommands.size() == 2);
  CHECK(antsCommands.back().executable == "/opt/ants/bin/antsApplyTransforms");

  const std::vector<registration::CommandSpec> fireAntsCommands =
    registration::generateCommands(baseJob(registration::Backend::FireANTs), options);
  REQUIRE(fireAntsCommands.size() == 1);
  CHECK(fireAntsCommands.front().executable == "/venv/bin/python");
  CHECK(fireAntsCommands.front().args.at(1) == "custom_bridge");
}
