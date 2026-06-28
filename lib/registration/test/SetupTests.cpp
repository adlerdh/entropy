#include "registration/Setup.h"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <string>

namespace
{

registration::SetupImageChoice
imageChoice(std::string uid, std::string name, bool isReference = false, bool isActive = false)
{
  registration::SetupImageChoice choice;
  choice.image.uid = std::move(uid);
  choice.image.displayName = std::move(name);
  choice.image.fileName = choice.image.displayName + ".nii.gz";
  choice.image.source = registration::DataSource::LoadedImage;
  choice.dimension = 3;
  choice.isReference = isReference;
  choice.isActive = isActive;
  return choice;
}

bool containsParameter(const std::vector<registration::ParameterSchema>& parameters, std::string_view key)
{
  return std::any_of(parameters.begin(), parameters.end(), [&](const registration::ParameterSchema& parameter) {
    return parameter.key == key;
  });
}

} // namespace

TEST_CASE("registration setup defaults to reference fixed image and active moving image", "[registration][setup]")
{
  const std::vector<registration::SetupImageChoice> images = {
    imageChoice("fixed", "Fixed Image", true, false),
    imageChoice("moving", "Moving Image", false, true),
    imageChoice("other", "Other Image")};

  const registration::SetupState state =
    registration::createSetupState(images, registration::Backend::Greedy, "/tmp/entropy-reg");

  CHECK(state.job.fixedImage.uid == "fixed");
  CHECK(state.job.movingImage.uid == "moving");
  CHECK(state.job.outputPrefix == "Moving_Image_to_Fixed_Image");
  CHECK_FALSE(state.job.parameterValues.empty());
  CHECK(state.validation.canLaunch());
}

TEST_CASE("registration setup picks a different moving image when active is the reference", "[registration][setup]")
{
  const std::vector<registration::SetupImageChoice> images = {
    imageChoice("fixed", "Fixed", true, true),
    imageChoice("moving", "Moving")};

  const registration::SetupState state =
    registration::createSetupState(images, registration::Backend::Greedy, "/tmp/entropy-reg");

  CHECK(state.job.fixedImage.uid == "fixed");
  CHECK(state.job.movingImage.uid == "moving");
  CHECK(state.validation.canLaunch());
}

TEST_CASE(
  "registration setup reports validation errors when fewer than two images are available",
  "[registration][setup]")
{
  const std::vector<registration::SetupImageChoice> images = {imageChoice("fixed", "Fixed", true, true)};

  const registration::SetupState state =
    registration::createSetupState(images, registration::Backend::Greedy, "/tmp/entropy-reg");

  CHECK_FALSE(state.validation.canLaunch());
}

TEST_CASE("registration setup backend switch clamps unsupported transform and metric", "[registration][setup]")
{
  registration::SetupState state = registration::createSetupState(
    {imageChoice("fixed", "Fixed", true, false), imageChoice("moving", "Moving", false, true)},
    registration::Backend::ANTs,
    "/tmp/entropy-reg");

  state.job.transformModel = registration::TransformModel::TimeVaryingVelocity;
  state.job.metric = registration::Metric::PointSet;
  registration::setBackend(state, registration::Backend::Greedy);

  CHECK(state.job.backend == registration::Backend::Greedy);
  CHECK(registration::supportsTransformModel(state.capabilities, state.job.transformModel));
  CHECK(registration::supportsMetric(state.capabilities, state.job.metric));
  CHECK(state.validation.canLaunch());
}

TEST_CASE(
  "registration setup parameter disclosure hides advanced and expert values by default",
  "[registration][setup]")
{
  registration::SetupState state = registration::createSetupState(
    {imageChoice("fixed", "Fixed", true, false), imageChoice("moving", "Moving", false, true)},
    registration::Backend::Greedy,
    "/tmp/entropy-reg");

  std::vector<registration::ParameterSchema> parameters = registration::visibleParameters(state);
  CHECK(containsParameter(parameters, "iterations"));
  CHECK_FALSE(containsParameter(parameters, "threads"));
  CHECK_FALSE(containsParameter(parameters, "extraArgs"));

  state.showAdvancedParameters = true;
  parameters = registration::visibleParameters(state);
  CHECK(containsParameter(parameters, "threads"));
  CHECK_FALSE(containsParameter(parameters, "extraArgs"));

  state.showExpertParameters = true;
  parameters = registration::visibleParameters(state);
  CHECK(containsParameter(parameters, "extraArgs"));
}

TEST_CASE("registration setup preserves matching parameter values when switching backend", "[registration][setup]")
{
  registration::SetupState state = registration::createSetupState(
    {imageChoice("fixed", "Fixed", true, false), imageChoice("moving", "Moving", false, true)},
    registration::Backend::Greedy,
    "/tmp/entropy-reg");

  registration::ParameterValue* iterations = registration::findParameterValue(state, "iterations");
  REQUIRE(iterations);
  iterations->value = "25x10";

  registration::setBackend(state, registration::Backend::ANTs);

  iterations = registration::findParameterValue(state, "iterations");
  REQUIRE(iterations);
  CHECK(iterations->value == "25x10");
  REQUIRE_FALSE(state.job.parameterValues.empty());
  CHECK(std::any_of(
    state.job.parameterValues.begin(),
    state.job.parameterValues.end(),
    [](const registration::ParameterValue& value) { return value.key == "iterations" && value.value == "25x10"; }));
}

TEST_CASE("registration setup returns command previews only for launchable jobs", "[registration][setup]")
{
  registration::SetupState state = registration::createSetupState(
    {imageChoice("fixed", "Fixed", true, false), imageChoice("moving", "Moving", false, true)},
    registration::Backend::Greedy,
    "/tmp/entropy-reg");

  CHECK_FALSE(registration::commandPreviews(state).empty());

  registration::ParameterValue* iterations = registration::findParameterValue(state, "iterations");
  REQUIRE(iterations);
  iterations->value = "7x3";
  const std::vector<std::string> previews = registration::commandPreviews(state);
  REQUIRE_FALSE(previews.empty());
  CHECK(previews.front().find("7x3") != std::string::npos);

  state.job.movingImage = {};
  registration::refreshValidation(state);
  CHECK(registration::commandPreviews(state).empty());
}
