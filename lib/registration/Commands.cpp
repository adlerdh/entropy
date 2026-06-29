#include "registration/Commands.h"

#include "registration/Artifacts.h"

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <optional>
#include <sstream>
#include <string_view>

namespace registration
{
namespace
{

std::string pathString(const std::filesystem::path& path)
{
  return path.string();
}

bool hasInputRef(const DataRef& ref)
{
  return !ref.uid.empty() || !ref.fileName.empty();
}

std::filesystem::path inputPath(const DataRef& ref, const std::filesystem::path& plannedPath)
{
  return ref.fileName.empty() ? plannedPath : ref.fileName;
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

int integerParameterValueOr(const JobSpec& job, std::string_view key, int fallback)
{
  const std::optional<std::string> value = parameterValue(job, key);
  if (!value) {
    return fallback;
  }

  char* end = nullptr;
  const long parsed = std::strtol(value->c_str(), &end, 10);
  return end && *end == '\0' ? static_cast<int>(parsed) : fallback;
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

std::string greedySmoothingValue(const std::string& value, const std::string& units)
{
  const bool alreadyHasUnits = value.find("vox") != std::string::npos || value.find("mm") != std::string::npos;
  return alreadyHasUnits ? value : value + units;
}

std::string greedyChoiceToken(const std::string& value)
{
  const std::size_t open = value.find('(');
  const std::size_t close = open == std::string::npos ? std::string::npos : value.find(')', open + 1u);
  if (open == std::string::npos || close == std::string::npos || close <= open + 1u) {
    return value;
  }
  return value.substr(open + 1u, close - open - 1u);
}

void appendGreedyAuxiliaryPairs(std::vector<std::string>& args, const JobSpec& job)
{
  for (std::size_t index = 0; index < job.auxiliaryImagePairs.size(); ++index) {
    const AuxiliaryImagePair& pair = job.auxiliaryImagePairs[index];
    if (!hasInputRef(pair.fixed) || !hasInputRef(pair.moving)) {
      continue;
    }
    args.push_back("-w");
    args.push_back(std::to_string(pair.weight));
    args.push_back("-i");
    args.push_back(pathString(inputPath(pair.fixed, artifactPath(job, ArtifactRole::AuxiliaryFixedImage, index))));
    args.push_back(pathString(inputPath(pair.moving, artifactPath(job, ArtifactRole::AuxiliaryMovingImage, index))));
  }
}

void appendGreedyFixedMaskArgs(std::vector<std::string>& args, const JobSpec& job)
{
  if (hasInputRef(job.fixedMask)) {
    args.push_back("-gm");
    args.push_back(pathString(inputPath(job.fixedMask, artifactPath(job, ArtifactRole::FixedMask))));
  }
  if (const std::optional<std::string> trim = parameterValue(job, "fixedMaskTrim")) {
    args.push_back("-gm-trim");
    args.push_back(*trim);
  }
}

void appendGreedyAffineAdvancedArgs(std::vector<std::string>& args, const JobSpec& job)
{
  const int searchIterations = integerParameterValueOr(job, "affineSearchIterations", 0);
  if (searchIterations > 0) {
    args.push_back("-search");
    args.push_back(std::to_string(searchIterations));
    args.push_back(greedyChoiceToken(parameterValueOr(job, "affineSearchRotation", "Any rotation (any)")));
    args.push_back(parameterValueOr(job, "affineSearchTranslation", "50"));
  }

  const std::string determinant = greedyChoiceToken(parameterValueOr(job, "affineDeterminant", "None (free)"));
  if (determinant == "1" || determinant == "-1") {
    args.push_back("-det");
    args.push_back(determinant);
  }

  if (const std::optional<std::string> jitter = parameterValue(job, "affineJitter")) {
    args.push_back("-jitter");
    args.push_back(*jitter);
  }
}

void appendGreedyDeformableAdvancedArgs(std::vector<std::string>& args, const JobSpec& job)
{
  if (const std::optional<std::string> smoothing = parameterValue(job, "smoothingSigma")) {
    std::string gradientSigma;
    std::string warpSigma;
    std::istringstream stream{*smoothing};
    std::getline(stream, gradientSigma, ',');
    std::getline(stream, warpSigma, ',');
    if (!gradientSigma.empty() && !warpSigma.empty()) {
      const std::string units = parameterValueOr(job, "smoothingUnits", "vox");
      args.push_back("-s");
      args.push_back(greedySmoothingValue(gradientSigma, units));
      args.push_back(greedySmoothingValue(warpSigma, units));
    }
  }

  if (const std::optional<std::string> stepSize = parameterValue(job, "stepSize")) {
    args.push_back("-e");
    args.push_back(*stepSize);
  }

  const std::string velocityModel =
    greedyChoiceToken(parameterValueOr(job, "velocityModel", "Stationary velocity (sv)"));
  if (velocityModel == "svlb") {
    args.push_back("-svlb");
  }
  else if (velocityModel == "sv-incompr") {
    args.push_back("-sv-incompr");
  }
  else {
    args.push_back("-sv");
  }

  if (const std::optional<std::string> warpPrecision = parameterValue(job, "warpPrecision")) {
    args.push_back("-wp");
    args.push_back(*warpPrecision);
  }
}

int greedyAffineDof(TransformModel model)
{
  switch (model) {
    case TransformModel::Rigid:
      return 6;
    case TransformModel::Similarity:
      return 7;
    case TransformModel::Affine:
    case TransformModel::RigidAffine:
    case TransformModel::AffineDeformable:
      return 12;
    case TransformModel::Translation:
    case TransformModel::Deformable:
    case TransformModel::BSplineDisplacement:
    case TransformModel::GaussianDisplacement:
    case TransformModel::TimeVaryingVelocity:
      return 12;
  }
  return 12;
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

  if (includesAffineTransform(job.transformModel)) {
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
    appendGreedyAuxiliaryPairs(command.args, job);
    command.args.push_back("-dof");
    command.args.push_back(std::to_string(greedyAffineDof(job.transformModel)));
    if (!job.initialAffineTransform.empty()) {
      command.args.push_back("-ia");
      command.args.push_back(pathString(job.initialAffineTransform));
    }
    else if (const std::string moments = greedyChoiceToken(parameterValueOr(job, "affineMoments", "Off"));
             moments == "1" || moments == "2")
    {
      command.args.push_back("-ia-moments");
      command.args.push_back(moments);
    }
    else if (job.useImageCentersForInitialization) {
      command.args.push_back("-ia-image-centers");
    }
    appendGreedyFixedMaskArgs(command.args, job);
    appendGreedyAffineAdvancedArgs(command.args, job);
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

  if (includesDeformableTransform(job.transformModel)) {
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
      "-n",
      iterations};
    appendGreedyAuxiliaryPairs(command.args, job);
    if (includesAffineTransform(job.transformModel)) {
      command.args.push_back("-it");
      command.args.push_back(pathString(affine));
    }
    appendGreedyFixedMaskArgs(command.args, job);
    if (hasInputRef(job.movingMask)) {
      command.args.push_back("-mm");
      command.args.push_back(pathString(inputPath(job.movingMask, artifactPath(job, ArtifactRole::MovingMask))));
    }
    appendGreedyDeformableAdvancedArgs(command.args, job);
    if (job.outputs.loadForwardWarp) {
      if (const std::optional<std::string> invExp = parameterValue(job, "inverseExponentiation")) {
        command.args.push_back("-exp");
        command.args.push_back(*invExp);
      }
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
    if (includesDeformableTransform(job.transformModel)) {
      command.args.push_back(pathString(inverseWarp));
    }
    if (includesAffineTransform(job.transformModel)) {
      command.args.push_back(pathString(affine));
    }
    if (threads != "0") {
      command.args.push_back("-threads");
      command.args.push_back(threads);
    }
    commands.push_back(std::move(command));
  }

  if (job.outputs.loadWarpedSegmentation && hasInputRef(job.movingMask)) {
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
      pathString(inputPath(job.movingMask, artifactPath(job, ArtifactRole::MovingMask))),
      pathString(artifactPath(job, ArtifactRole::WarpedSegmentation)),
      "-r"};
    if (includesDeformableTransform(job.transformModel)) {
      command.args.push_back(pathString(inverseWarp));
    }
    if (includesAffineTransform(job.transformModel)) {
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
  for (std::size_t index = 0; index < job.auxiliaryImagePairs.size(); ++index) {
    const AuxiliaryImagePair& pair = job.auxiliaryImagePairs[index];
    if (!hasInputRef(pair.fixed) || !hasInputRef(pair.moving)) {
      continue;
    }
    registrationCommand.args.push_back("--metric");
    registrationCommand.args.push_back(
      antsMetric(pair.metric) + "[" +
      pathString(inputPath(pair.fixed, artifactPath(job, ArtifactRole::AuxiliaryFixedImage, index))) + "," +
      pathString(inputPath(pair.moving, artifactPath(job, ArtifactRole::AuxiliaryMovingImage, index))) + "," +
      std::to_string(pair.weight) + ",4]");
  }
  if (!job.initialAffineTransform.empty()) {
    registrationCommand.args.push_back("--initial-moving-transform");
    registrationCommand.args.push_back(pathString(job.initialAffineTransform));
  }
  if (hasInputRef(job.fixedMask) || hasInputRef(job.movingMask)) {
    registrationCommand.args.push_back("--masks");
    registrationCommand.args.push_back(
      "[" +
      (hasInputRef(job.fixedMask) ? pathString(inputPath(job.fixedMask, artifactPath(job, ArtifactRole::FixedMask)))
                                  : std::string{"NULL"}) +
      "," +
      (hasInputRef(job.movingMask) ? pathString(inputPath(job.movingMask, artifactPath(job, ArtifactRole::MovingMask)))
                                   : std::string{"NULL"}) +
      "]");
  }
  appendExpertArguments(registrationCommand.args, job);
  commands.push_back(std::move(registrationCommand));

  if (job.outputs.loadWarpedSegmentation && hasInputRef(job.movingMask)) {
    CommandSpec command;
    command.description = "ANTs reslice warped segmentation";
    command.executable = options.antsApplyTransformsExecutable;
    command.args = {
      "-d",
      std::to_string(job.dimension),
      "-i",
      pathString(inputPath(job.movingMask, artifactPath(job, ArtifactRole::MovingMask))),
      "-r",
      pathString(job.fixedImage.fileName),
      "-o",
      pathString(artifactPath(job, ArtifactRole::WarpedSegmentation)),
      "-n",
      "GenericLabel"};
    if (includesDeformableTransform(job.transformModel)) {
      command.args.push_back("-t");
      command.args.push_back(pathString(antsWarpPath(job)));
    }
    if (includesAffineTransform(job.transformModel)) {
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
