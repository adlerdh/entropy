#include "registration/Commands.h"

#include "registration/Artifacts.h"

#include <algorithm>
#include <filesystem>
#include <optional>
#include <sstream>
#include <string_view>

namespace registration
{
namespace
{

bool includesAffine(TransformModel model)
{
  return model == TransformModel::Rigid || model == TransformModel::Affine || model == TransformModel::RigidAffine ||
         model == TransformModel::AffineDeformable || model == TransformModel::Similarity ||
         model == TransformModel::Translation;
}

bool includesDeformable(TransformModel model)
{
  return model == TransformModel::Deformable || model == TransformModel::AffineDeformable ||
         model == TransformModel::BSplineDisplacement || model == TransformModel::GaussianDisplacement ||
         model == TransformModel::TimeVaryingVelocity;
}

std::string pathString(const std::filesystem::path& path)
{
  return path.string();
}

std::optional<std::string> parameterValue(const JobSpec& job, std::string_view key)
{
  const auto it =
    std::find_if(job.parameterValues.begin(), job.parameterValues.end(), [&](const ParameterValue& value) {
      return value.key == key;
    });
  if (it == job.parameterValues.end() || it->value.empty()) {
    return std::nullopt;
  }
  return it->value;
}

std::string parameterValueOr(const JobSpec& job, std::string_view key, std::string fallback)
{
  if (const std::optional<std::string> value = parameterValue(job, key)) {
    return *value;
  }
  return fallback;
}

bool booleanParameterValue(const JobSpec& job, std::string_view key)
{
  const std::optional<std::string> value = parameterValue(job, key);
  return value && (*value == "true" || *value == "1" || *value == "yes" || *value == "on");
}

void appendWhitespaceSeparatedArguments(std::vector<std::string>& args, const std::string& text)
{
  std::istringstream stream{text};
  std::string value;
  while (stream >> value) {
    args.push_back(value);
  }
}

void appendExpertArguments(std::vector<std::string>& args, const JobSpec& job)
{
  if (const std::optional<std::string> value = parameterValue(job, "extraArgs")) {
    appendWhitespaceSeparatedArguments(args, *value);
  }
  args.insert(args.end(), job.extraArguments.begin(), job.extraArguments.end());
}

std::string repeatedRadius(const std::string& radius, int dimension)
{
  std::ostringstream stream;
  for (int i = 0; i < std::max(1, dimension); ++i) {
    if (i > 0) {
      stream << 'x';
    }
    stream << radius;
  }
  return stream.str();
}

std::string greedyMetric(Metric metric)
{
  switch (metric) {
    case Metric::SSD:
      return "SSD";
    case Metric::MI:
      return "MI";
    case Metric::NMI:
      return "NMI";
    case Metric::NCC:
      return "NCC";
    case Metric::WNCC:
      return "WNCC";
    default:
      return "WNCC";
  }
}

std::string antsMetric(Metric metric)
{
  switch (metric) {
    case Metric::MI:
      return "MI";
    case Metric::Mattes:
      return "Mattes";
    case Metric::Demons:
      return "Demons";
    case Metric::GC:
      return "GC";
    case Metric::MSE:
    case Metric::SSD:
      return "MeanSquares";
    default:
      return "CC";
  }
}

std::string antsTransform(TransformModel model)
{
  switch (model) {
    case TransformModel::Translation:
      return "Translation[0.1]";
    case TransformModel::Rigid:
      return "Rigid[0.1]";
    case TransformModel::Similarity:
      return "Similarity[0.1]";
    case TransformModel::Affine:
    case TransformModel::RigidAffine:
      return "Affine[0.1]";
    case TransformModel::BSplineDisplacement:
      return "BSplineSyN[0.1,26,0,3]";
    case TransformModel::GaussianDisplacement:
      return "GaussianDisplacementField[0.1,3,0]";
    case TransformModel::TimeVaryingVelocity:
      return "TimeVaryingVelocityField[0.1,4,3,0,0]";
    case TransformModel::Deformable:
    case TransformModel::AffineDeformable:
      return "SyN[0.1,3,0]";
  }
  return "SyN[0.1,3,0]";
}

std::filesystem::path antsOutputPrefixPath(const JobSpec& job)
{
  return job.outputDirectory / outputPrefix(job);
}

std::filesystem::path antsAffineTransformPath(const JobSpec& job)
{
  return antsOutputPrefixPath(job).string() + "0GenericAffine.mat";
}

std::filesystem::path antsWarpPath(const JobSpec& job)
{
  return antsOutputPrefixPath(job).string() + "1Warp.nii.gz";
}

std::vector<CommandSpec> greedyCommands(const JobSpec& job, const CommandGenerationOptions& options)
{
  std::vector<CommandSpec> commands;
  const std::filesystem::path affine = artifactPath(job, ArtifactRole::AffineTransform);
  const std::filesystem::path inverseWarp = artifactPath(job, ArtifactRole::InverseWarp);
  const std::filesystem::path forwardWarp = artifactPath(job, ArtifactRole::ForwardWarp);
  const std::filesystem::path warpedImage = artifactPath(job, ArtifactRole::WarpedImage);
  const std::string iterations = parameterValueOr(job, "iterations", job.iterationSchedule);
  const std::string metricRadius = repeatedRadius(parameterValueOr(job, "wnccRadius", "2"), job.dimension);
  const std::string threads = parameterValueOr(job, "threads", "0");

  if (includesAffine(job.transformModel)) {
    CommandSpec command;
    command.description = "Greedy affine registration";
    command.executable = options.greedyExecutable;
    command.args = {
      "-d",
      std::to_string(job.dimension),
      "-a",
      "-m",
      greedyMetric(job.metric),
      metricRadius,
      "-i",
      pathString(job.fixedImage.fileName),
      pathString(job.movingImage.fileName),
      "-o",
      pathString(affine),
      "-n",
      iterations};
    if (job.useImageCentersForInitialization) {
      command.args.push_back("-ia-image-centers");
    }
    if (!job.fixedMask.fileName.empty()) {
      command.args.push_back("-gm");
      command.args.push_back(pathString(job.fixedMask.fileName));
    }
    if (threads != "0") {
      command.args.push_back("-threads");
      command.args.push_back(threads);
    }
    if (booleanParameterValue(job, "singlePrecision")) {
      command.args.push_back("-float");
    }
    appendExpertArguments(command.args, job);
    commands.push_back(std::move(command));
  }

  if (includesDeformable(job.transformModel)) {
    CommandSpec command;
    command.description = "Greedy deformable registration";
    command.executable = options.greedyExecutable;
    command.args = {
      "-d",
      std::to_string(job.dimension),
      "-m",
      greedyMetric(job.metric),
      metricRadius,
      "-i",
      pathString(job.fixedImage.fileName),
      pathString(job.movingImage.fileName),
      "-o",
      pathString(inverseWarp),
      "-sv",
      "-n",
      iterations};
    if (includesAffine(job.transformModel)) {
      command.args.push_back("-it");
      command.args.push_back(pathString(affine));
    }
    if (!job.fixedMask.fileName.empty()) {
      command.args.push_back("-gm");
      command.args.push_back(pathString(job.fixedMask.fileName));
    }
    if (!job.movingMask.fileName.empty()) {
      command.args.push_back("-mm");
      command.args.push_back(pathString(job.movingMask.fileName));
    }
    if (job.outputs.loadForwardWarp) {
      command.args.push_back("-oinv");
      command.args.push_back(pathString(forwardWarp));
    }
    if (threads != "0") {
      command.args.push_back("-threads");
      command.args.push_back(threads);
    }
    if (booleanParameterValue(job, "singlePrecision")) {
      command.args.push_back("-float");
    }
    appendExpertArguments(command.args, job);
    commands.push_back(std::move(command));
  }

  if (job.outputs.loadWarpedImage) {
    CommandSpec command;
    command.description = "Greedy reslice warped image";
    command.executable = options.greedyExecutable;
    command.args = {
      "-d",
      std::to_string(job.dimension),
      "-rf",
      pathString(job.fixedImage.fileName),
      "-rm",
      pathString(job.movingImage.fileName),
      pathString(warpedImage),
      "-r"};
    if (includesDeformable(job.transformModel)) {
      command.args.push_back(pathString(inverseWarp));
    }
    if (includesAffine(job.transformModel)) {
      command.args.push_back(pathString(affine));
    }
    if (threads != "0") {
      command.args.push_back("-threads");
      command.args.push_back(threads);
    }
    commands.push_back(std::move(command));
  }

  if (job.outputs.loadWarpedSegmentation && !job.movingMask.fileName.empty()) {
    CommandSpec command;
    command.description = "Greedy reslice warped segmentation";
    command.executable = options.greedyExecutable;
    command.args = {
      "-d",
      std::to_string(job.dimension),
      "-rf",
      pathString(job.fixedImage.fileName),
      "-ri",
      "LABEL",
      "0.2vox",
      "-rm",
      pathString(job.movingMask.fileName),
      pathString(artifactPath(job, ArtifactRole::WarpedSegmentation)),
      "-r"};
    if (includesDeformable(job.transformModel)) {
      command.args.push_back(pathString(inverseWarp));
    }
    if (includesAffine(job.transformModel)) {
      command.args.push_back(pathString(affine));
    }
    if (threads != "0") {
      command.args.push_back("-threads");
      command.args.push_back(threads);
    }
    commands.push_back(std::move(command));
  }

  return commands;
}

std::vector<CommandSpec> antsCommands(const JobSpec& job, const CommandGenerationOptions& options)
{
  std::vector<CommandSpec> commands;
  CommandSpec registrationCommand;
  registrationCommand.description = "ANTs registration";
  registrationCommand.executable = options.antsRegistrationExecutable;
  const std::filesystem::path prefix = antsOutputPrefixPath(job);
  const std::filesystem::path warped = artifactPath(job, ArtifactRole::WarpedImage);
  const std::string iterations = parameterValueOr(job, "iterations", job.iterationSchedule);
  const std::string shrinkFactors = parameterValueOr(job, "shrinkFactors", "4x2x1");
  const std::string smoothingSigmas = parameterValueOr(job, "smoothingSigmas", "2x1x0");
  const std::string convergenceThreshold = parameterValueOr(job, "convergenceThreshold", "1e-6");
  const std::string convergenceWindow = parameterValueOr(job, "convergenceWindow", "10");

  registrationCommand.args = {
    "-d",
    std::to_string(job.dimension),
    "--output",
    "[" + pathString(prefix) + "," + pathString(warped) + "]",
    "--transform",
    antsTransform(job.transformModel),
    "--metric",
    antsMetric(job.metric) + "[" + pathString(job.fixedImage.fileName) + "," + pathString(job.movingImage.fileName) +
      ",1,4]",
    "--convergence",
    "[" + iterations + "," + convergenceThreshold + "," + convergenceWindow + "]",
    "--shrink-factors",
    shrinkFactors,
    "--smoothing-sigmas",
    smoothingSigmas + "vox"};
  if (!job.fixedMask.fileName.empty()) {
    registrationCommand.args.push_back("--masks");
    registrationCommand.args.push_back("[" + pathString(job.fixedMask.fileName) + ",NULL]");
  }
  appendExpertArguments(registrationCommand.args, job);
  commands.push_back(std::move(registrationCommand));

  if (job.outputs.loadWarpedSegmentation && !job.movingMask.fileName.empty()) {
    CommandSpec command;
    command.description = "ANTs reslice warped segmentation";
    command.executable = options.antsApplyTransformsExecutable;
    command.args = {
      "-d",
      std::to_string(job.dimension),
      "-i",
      pathString(job.movingMask.fileName),
      "-r",
      pathString(job.fixedImage.fileName),
      "-o",
      pathString(artifactPath(job, ArtifactRole::WarpedSegmentation)),
      "-n",
      "GenericLabel"};
    if (includesDeformable(job.transformModel)) {
      command.args.push_back("-t");
      command.args.push_back(pathString(antsWarpPath(job)));
    }
    if (includesAffine(job.transformModel)) {
      command.args.push_back("-t");
      command.args.push_back(pathString(antsAffineTransformPath(job)));
    }
    commands.push_back(std::move(command));
  }

  return commands;
}

std::vector<CommandSpec> fireAntsCommands(const JobSpec& job, const CommandGenerationOptions& options)
{
  CommandSpec command;
  command.description = "FireANTs bridge registration";
  command.executable = options.fireAntsPythonExecutable;
  command.args = {"-m", options.fireAntsBridgeModule, "run", pathString(artifactPath(job, ArtifactRole::JobSpec))};
  return {command};
}

bool needsQuoting(const std::string& value)
{
  return value.find_first_of(" \t\n\"'[]") != std::string::npos;
}

std::string quoteArg(const std::string& value)
{
  if (!needsQuoting(value)) {
    return value;
  }

  std::string quoted = "'";
  for (const char ch : value) {
    if (ch == '\'') {
      quoted += "'\\''";
    }
    else {
      quoted.push_back(ch);
    }
  }
  quoted.push_back('\'');
  return quoted;
}

} // namespace

std::vector<CommandSpec> generateCommands(const JobSpec& job, const CommandGenerationOptions& options)
{
  switch (job.backend) {
    case Backend::Greedy:
      return greedyCommands(job, options);
    case Backend::ANTs:
      return antsCommands(job, options);
    case Backend::FireANTs:
      return fireAntsCommands(job, options);
  }
  return {};
}

std::vector<CommandSpec> generateCommands(const JobSpec& job)
{
  return generateCommands(job, CommandGenerationOptions{});
}

std::string displayCommand(const CommandSpec& command)
{
  std::ostringstream stream;
  stream << quoteArg(command.executable);
  for (const std::string& arg : command.args) {
    stream << ' ' << quoteArg(arg);
  }
  return stream.str();
}

} // namespace registration
