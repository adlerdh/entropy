#include "registration/Commands.h"

#include <catch2/catch_test_macros.hpp>

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

TEST_CASE("FireANTs command generation uses the bridge module", "[registration][commands]")
{
  registration::JobSpec job = baseJob(registration::Backend::FireANTs);

  const std::vector<registration::CommandSpec> commands = registration::generateCommands(job);

  REQUIRE(commands.size() == 1);
  CHECK(commands.front().executable == "python");
  CHECK(commands.front().args.at(1) == "entropy_fireants_bridge");
  CHECK(registration::displayCommand(commands.front()).find("moving_to_fixed_job.json") != std::string::npos);
}
