#include "registration/Commands.h"

#include <filesystem>
#include <sstream>

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

std::filesystem::path outputPath(const JobSpec& job, const char* suffix)
{
  const std::string prefix = job.outputPrefix.empty() ? "registration" : job.outputPrefix;
  return job.outputDirectory / (prefix + suffix);
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

std::vector<CommandSpec> greedyCommands(const JobSpec& job, const CommandGenerationOptions& options)
{
  std::vector<CommandSpec> commands;
  const std::filesystem::path affine = outputPath(job, "_affine.mat");
  const std::filesystem::path inverseWarp = outputPath(job, "_inverse_warp.nii.gz");
  const std::filesystem::path forwardWarp = outputPath(job, "_forward_warp.nii.gz");
  const std::filesystem::path warpedImage = outputPath(job, "_warped.nii.gz");

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
      "2x2x2",
      "-i",
      pathString(job.fixedImage.fileName),
      pathString(job.movingImage.fileName),
      "-o",
      pathString(affine),
      "-n",
      job.iterationSchedule};
    if (job.useImageCentersForInitialization) {
      command.args.push_back("-ia-image-centers");
    }
    if (!job.fixedMask.fileName.empty()) {
      command.args.push_back("-gm");
      command.args.push_back(pathString(job.fixedMask.fileName));
    }
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
      "2x2x2",
      "-i",
      pathString(job.fixedImage.fileName),
      pathString(job.movingImage.fileName),
      "-o",
      pathString(inverseWarp),
      "-sv",
      "-n",
      job.iterationSchedule};
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
    commands.push_back(std::move(command));
  }

  return commands;
}

std::vector<CommandSpec> antsCommands(const JobSpec& job, const CommandGenerationOptions& options)
{
  CommandSpec command;
  command.description = "ANTs registration";
  command.executable = options.antsRegistrationExecutable;
  const std::filesystem::path prefix =
    job.outputDirectory / (job.outputPrefix.empty() ? "registration" : job.outputPrefix);
  const std::filesystem::path warped = outputPath(job, "_warped.nii.gz");

  command.args = {
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
    "[" + job.iterationSchedule + ",1e-6,10]",
    "--shrink-factors",
    "4x2x1",
    "--smoothing-sigmas",
    "2x1x0vox"};
  if (!job.fixedMask.fileName.empty()) {
    command.args.push_back("--masks");
    command.args.push_back("[" + pathString(job.fixedMask.fileName) + ",NULL]");
  }
  return {command};
}

std::vector<CommandSpec> fireAntsCommands(const JobSpec& job, const CommandGenerationOptions& options)
{
  CommandSpec command;
  command.description = "FireANTs bridge registration";
  command.executable = options.fireAntsPythonExecutable;
  command.args = {"-m", options.fireAntsBridgeModule, "run", pathString(outputPath(job, "_job.json"))};
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
