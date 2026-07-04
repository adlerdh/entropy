#include "registration/Commands.h"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <initializer_list>
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

bool hasArgSequence(const std::vector<std::string>& args, std::initializer_list<const char*> sequence)
{
  std::vector<std::string> expected;
  expected.reserve(sequence.size());
  for (const char* value : sequence) {
    expected.emplace_back(value);
  }
  return std::search(args.begin(), args.end(), expected.begin(), expected.end()) != args.end();
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
  CHECK(preview.find("-V 1") != std::string::npos);
}

TEST_CASE("Greedy command generation uses selected verbosity", "[registration][commands]")
{
  registration::JobSpec job = baseJob(registration::Backend::Greedy);
  job.transformModel = registration::TransformModel::Affine;
  job.parameterValues = {{"verbosity", "Verbose (2)"}};

  const std::vector<registration::CommandSpec> commands = registration::generateCommands(job);

  REQUIRE(commands.size() == 2);
  const std::string preview = registration::displayCommand(commands.front());
  CHECK(preview.find("-V 2") != std::string::npos);
}

TEST_CASE("Greedy command generation emits radius only for NCC metrics", "[registration][commands]")
{
  registration::JobSpec job = baseJob(registration::Backend::Greedy);
  job.transformModel = registration::TransformModel::Affine;
  job.parameterValues = {{"wnccRadius", "4"}};

  job.metric = registration::Metric::WNCC;
  std::string preview = registration::displayCommand(registration::generateCommands(job).front());
  CHECK(preview.find("-m WNCC 4x4x4") != std::string::npos);

  job.metric = registration::Metric::NCC;
  preview = registration::displayCommand(registration::generateCommands(job).front());
  CHECK(preview.find("-m NCC 4x4x4") != std::string::npos);

  job.metric = registration::Metric::NMI;
  preview = registration::displayCommand(registration::generateCommands(job).front());
  CHECK(preview.find("-m NMI -i") != std::string::npos);
  CHECK(preview.find("4x4x4") == std::string::npos);

  job.metric = registration::Metric::MI;
  preview = registration::displayCommand(registration::generateCommands(job).front());
  CHECK(preview.find("-m MI -i") != std::string::npos);
  CHECK(preview.find("4x4x4") == std::string::npos);
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
  CHECK(deformablePreview.find("-exp 5") != std::string::npos);
  CHECK(deformablePreview.find("-svlb") != std::string::npos);
  CHECK(deformablePreview.find("-wp 0.01") != std::string::npos);
  CHECK(deformablePreview.find("-oinv") != std::string::npos);
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
  const std::vector<std::string>& affineArgs = commands.at(0).args;
  CHECK(hasArgSequence(affineArgs, {"-gm", "fixed_mask.nii.gz"}));
  CHECK_FALSE(hasArgSequence(affineArgs, {"-mm", "moving_mask.nii.gz"}));
  CHECK(hasArgSequence(affineArgs, {"-w", "0.500000", "-i", "aux_fixed.nii.gz", "aux_moving.nii.gz"}));

  const std::vector<std::string>& deformableArgs = commands.at(1).args;
  CHECK(hasArgSequence(deformableArgs, {"-gm", "fixed_mask.nii.gz"}));
  CHECK(hasArgSequence(deformableArgs, {"-mm", "moving_mask.nii.gz"}));
  CHECK(hasArgSequence(deformableArgs, {"-w", "0.500000", "-i", "aux_fixed.nii.gz", "aux_moving.nii.gz"}));
}

TEST_CASE(
  "Greedy command generation emits stationary velocity exponentiation without inverse warp output",
  "[registration][commands]")
{
  registration::JobSpec job = baseJob(registration::Backend::Greedy);
  job.transformModel = registration::TransformModel::Deformable;
  job.outputs.loadForwardWarp = false;
  job.outputs.transformLandmarksAndAnnotations = false;
  job.outputs.transformSurfaces = false;
  job.parameterValues = {{"inverseExponentiation", "6"}, {"velocityModel", "Stationary velocity (sv)"}};

  const std::vector<registration::CommandSpec> commands = registration::generateCommands(job);

  REQUIRE(commands.size() == 2);
  const std::string preview = registration::displayCommand(commands.front());
  CHECK(preview.find("-exp 6") != std::string::npos);
  CHECK(preview.find("-oinv") == std::string::npos);
}

TEST_CASE("Greedy command generation writes forward warp when point outputs need it", "[registration][commands]")
{
  registration::JobSpec job = baseJob(registration::Backend::Greedy);
  job.transformModel = registration::TransformModel::Deformable;
  job.outputs.loadForwardWarp = false;
  job.outputs.transformLandmarksAndAnnotations = true;
  job.outputs.transformSurfaces = false;

  const std::vector<registration::CommandSpec> commands = registration::generateCommands(job);

  REQUIRE(commands.size() == 2);
  const std::string preview = registration::displayCommand(commands.front());
  CHECK(preview.find("-oinv") != std::string::npos);
  CHECK(preview.find("moving_to_fixed_forward_warp.nii.gz") != std::string::npos);
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

  REQUIRE(commands.size() == 2);
  CHECK(commands.front().executable == "antsRegistration");
  const std::string preview = registration::displayCommand(commands.front());
  CHECK(preview.find("--transform 'Affine[0.1]'") != std::string::npos);
  CHECK(preview.find("--transform 'SyN[0.1,3,0]'") != std::string::npos);
  CHECK(preview.find("CC[fixed.nii.gz,moving.nii.gz,1.000000,4,Regular,0.25]") != std::string::npos);
  CHECK(preview.find("--verbose 1") != std::string::npos);

  CHECK(commands.back().description == "ANTs convert affine transform");
  CHECK(commands.back().executable == "ConvertTransformFile");
  const std::string convertPreview = registration::displayCommand(commands.back());
  CHECK(convertPreview.find("moving_to_fixed0GenericAffine.mat") != std::string::npos);
  CHECK(convertPreview.find("moving_to_fixed_affine.mat --homogeneousMatrix") != std::string::npos);
}

TEST_CASE("ANTs command generation can disable verbose output", "[registration][commands]")
{
  registration::JobSpec job = baseJob(registration::Backend::ANTs);
  job.parameterValues = {{"verbose", "false"}};

  const std::vector<registration::CommandSpec> commands = registration::generateCommands(job);

  REQUIRE(commands.size() == 2);
  const std::string preview = registration::displayCommand(commands.front());
  CHECK(preview.find("--verbose 0") != std::string::npos);
}

TEST_CASE("ANTs command generation uses selected transform bracket parameters", "[registration][commands]")
{
  registration::JobSpec job = baseJob(registration::Backend::ANTs);
  job.transformModel = registration::TransformModel::AffineDeformable;
  job.parameterValues = {{"antsGradientStep", "0.25"}, {"synUpdateFieldVariance", "5"}, {"synTotalFieldVariance", "1"}};

  std::vector<registration::CommandSpec> commands = registration::generateCommands(job);
  REQUIRE_FALSE(commands.empty());
  std::string preview = registration::displayCommand(commands.front());
  CHECK(preview.find("--transform 'Affine[0.25]'") != std::string::npos);
  CHECK(preview.find("--transform 'SyN[0.25,5,1]'") != std::string::npos);

  job.transformModel = registration::TransformModel::BSplineDisplacement;
  job.parameterValues = {
    {"antsGradientStep", "0.2"},
    {"bsplineSyNUpdateMeshSize", "12"},
    {"bsplineSyNTotalMeshSize", "4"},
    {"bsplineSyNSplineOrder", "2"}};

  commands = registration::generateCommands(job);
  REQUIRE_FALSE(commands.empty());
  preview = registration::displayCommand(commands.front());
  CHECK(preview.find("--transform 'BSplineSyN[0.2,12,4,2]'") != std::string::npos);

  job.transformModel = registration::TransformModel::GaussianDisplacement;
  job.parameterValues = {
    {"antsGradientStep", "0.3"},
    {"gaussianDisplacementUpdateVariance", "6"},
    {"gaussianDisplacementTotalVariance", "2"}};

  commands = registration::generateCommands(job);
  REQUIRE_FALSE(commands.empty());
  preview = registration::displayCommand(commands.front());
  CHECK(preview.find("--transform 'GaussianDisplacementField[0.3,6,2]'") != std::string::npos);

  job.transformModel = registration::TransformModel::TimeVaryingVelocity;
  job.parameterValues = {
    {"antsGradientStep", "0.4"},
    {"timeVaryingVelocityTimeIndices", "5"},
    {"timeVaryingVelocityUpdateVariance", "7"},
    {"timeVaryingVelocityUpdateTimeVariance", "1"},
    {"timeVaryingVelocityTotalVariance", "3"}};

  commands = registration::generateCommands(job);
  REQUIRE_FALSE(commands.empty());
  preview = registration::displayCommand(commands.front());
  CHECK(preview.find("--transform 'TimeVaryingVelocityField[0.4,5,7,1,3]'") != std::string::npos);
}

TEST_CASE("ANTs command generation only requests warped image output when needed", "[registration][commands]")
{
  registration::JobSpec job = baseJob(registration::Backend::ANTs);
  job.outputs.loadWarpedImage = true;

  std::string preview = registration::displayCommand(registration::generateCommands(job).front());
  CHECK(
    preview.find("--output '[/tmp/reg/moving_to_fixed,/tmp/reg/moving_to_fixed_warped.nii.gz]'") != std::string::npos);

  job.outputs.loadWarpedImage = false;
  preview = registration::displayCommand(registration::generateCommands(job).front());
  CHECK(preview.find("--output /tmp/reg/moving_to_fixed") != std::string::npos);
  CHECK(preview.find("moving_to_fixed_warped.nii.gz") == std::string::npos);
}

TEST_CASE("ANTs command generation uses metric-specific parameters", "[registration][commands]")
{
  registration::JobSpec job = baseJob(registration::Backend::ANTs);

  job.metric = registration::Metric::MI;
  std::string preview = registration::displayCommand(registration::generateCommands(job).front());
  CHECK(preview.find("MI[fixed.nii.gz,moving.nii.gz,1.000000,32,Regular,0.25]") != std::string::npos);

  job.metric = registration::Metric::Mattes;
  preview = registration::displayCommand(registration::generateCommands(job).front());
  CHECK(preview.find("Mattes[fixed.nii.gz,moving.nii.gz,1.000000,32,Regular,0.25]") != std::string::npos);

  job.metric = registration::Metric::CC;
  preview = registration::displayCommand(registration::generateCommands(job).front());
  CHECK(preview.find("CC[fixed.nii.gz,moving.nii.gz,1.000000,4,Regular,0.25]") != std::string::npos);
}

TEST_CASE("ANTs command generation applies selected metric sampling parameters", "[registration][commands]")
{
  registration::JobSpec job = baseJob(registration::Backend::ANTs);
  job.metric = registration::Metric::MI;
  job.parameterValues = {{"samplingStrategy", "Random"}, {"samplingPercentage", "0.1"}};

  std::string preview = registration::displayCommand(registration::generateCommands(job).front());
  CHECK(preview.find("MI[fixed.nii.gz,moving.nii.gz,1.000000,32,Random,0.1]") != std::string::npos);

  job.parameterValues = {{"samplingStrategy", "None"}, {"samplingPercentage", "0.1"}};
  preview = registration::displayCommand(registration::generateCommands(job).front());
  CHECK(preview.find("MI[fixed.nii.gz,moving.nii.gz,1.000000,32]") != std::string::npos);
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

  REQUIRE(commands.size() == 2);
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

  REQUIRE(commands.size() == 2);
  const std::string preview = registration::displayCommand(commands.front());
  CHECK(preview.find("--masks '[fixed_mask.nii.gz,moving_mask.nii.gz]'") != std::string::npos);
  CHECK(
    preview.find("--metric 'MeanSquares[aux_fixed.nii.gz,aux_moving.nii.gz,0.250000,0,Regular,0.25]'") !=
    std::string::npos);
}

TEST_CASE("ANTs command generation uses current affine initialization path", "[registration][commands]")
{
  registration::JobSpec job = baseJob(registration::Backend::ANTs);
  job.transformModel = registration::TransformModel::Affine;
  job.useCurrentAffineTransformsForInitialization = true;
  job.initialAffineTransform = "/tmp/reg/initial_affine.tfm";

  const std::vector<registration::CommandSpec> commands = registration::generateCommands(job);

  REQUIRE(commands.size() == 2);
  const std::string preview = registration::displayCommand(commands.front());
  CHECK(preview.find("--initial-moving-transform /tmp/reg/initial_affine.tfm") != std::string::npos);
}

TEST_CASE("ANTs command generation reslices moving segmentation when requested", "[registration][commands]")
{
  registration::JobSpec job = baseJob(registration::Backend::ANTs);
  job.transformModel = registration::TransformModel::AffineDeformable;
  job.outputs.loadWarpedSegmentation = true;
  job.movingMask = imageRef("moving-seg", "moving_seg.nii.gz");

  const std::vector<registration::CommandSpec> commands = registration::generateCommands(job);

  REQUIRE(commands.size() == 3);
  CHECK(commands.back().description == "ANTs reslice warped segmentation");
  CHECK(commands.back().executable == "antsApplyTransforms");
  const std::string preview = registration::displayCommand(commands.back());
  CHECK(preview.find("-n GenericLabel") != std::string::npos);
  CHECK(preview.find("--verbose 1") != std::string::npos);
  CHECK(preview.find("moving_to_fixed_warped_segmentation_01.nii.gz") != std::string::npos);
  CHECK(preview.find("moving_to_fixed1Warp.nii.gz") != std::string::npos);
  CHECK(preview.find("moving_to_fixed0GenericAffine.mat") != std::string::npos);
}

TEST_CASE(
  "ANTs command generation uses zero-index warp names for deformable-only registration",
  "[registration][commands]")
{
  registration::JobSpec job = baseJob(registration::Backend::ANTs);
  job.transformModel = registration::TransformModel::Deformable;
  job.outputs.loadAffineTransform = false;
  job.outputs.loadWarpedSegmentation = true;
  job.movingMask = imageRef("moving-seg", "moving_seg.nii.gz");

  const std::vector<registration::CommandSpec> commands = registration::generateCommands(job);

  REQUIRE(commands.size() == 2);
  const std::string preview = registration::displayCommand(commands.back());
  CHECK(preview.find("moving_to_fixed0Warp.nii.gz") != std::string::npos);
  CHECK(preview.find("moving_to_fixed1Warp.nii.gz") == std::string::npos);
}

TEST_CASE(
  "ANTs command generation uses one-index warp names for deformable-only registration with affine initialization",
  "[registration][commands]")
{
  registration::JobSpec job = baseJob(registration::Backend::ANTs);
  job.transformModel = registration::TransformModel::Deformable;
  job.outputs.loadAffineTransform = false;
  job.outputs.loadWarpedSegmentation = true;
  job.initialAffineTransform = "/tmp/reg/initial_affine.tfm";
  job.movingMask = imageRef("moving-seg", "moving_seg.nii.gz");

  const std::vector<registration::CommandSpec> commands = registration::generateCommands(job);

  REQUIRE(commands.size() == 2);
  const std::string preview = registration::displayCommand(commands.back());
  CHECK(preview.find("moving_to_fixed1Warp.nii.gz") != std::string::npos);
  CHECK(preview.find("moving_to_fixed0Warp.nii.gz") == std::string::npos);
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
  options.antsConvertTransformFileExecutable = "/opt/ants/bin/ConvertTransformFile";
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
  REQUIRE(antsCommands.size() == 3);
  CHECK(antsCommands.at(1).executable == "/opt/ants/bin/ConvertTransformFile");
  CHECK(antsCommands.back().executable == "/opt/ants/bin/antsApplyTransforms");

  const std::vector<registration::CommandSpec> fireAntsCommands =
    registration::generateCommands(baseJob(registration::Backend::FireANTs), options);
  REQUIRE(fireAntsCommands.size() == 1);
  CHECK(fireAntsCommands.front().executable == "/venv/bin/python");
  CHECK(fireAntsCommands.front().args.at(1) == "custom_bridge");
}
