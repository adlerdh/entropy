#include "registration/Json.h"

#include <nlohmann/json.hpp>

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace registration
{
namespace
{

template<typename Enum>
nlohmann::json enumToJson(Enum value)
{
  return std::string{label(value)};
}

template<typename Enum, typename FromString>
Enum enumFromJson(const nlohmann::json& json, FromString fromString)
{
  if (json.is_number_integer()) {
    return static_cast<Enum>(json.get<int>());
  }
  return fromString(json.get<std::string>());
}

std::string pathToString(const std::filesystem::path& path)
{
  return path.string();
}

std::filesystem::path pathFromJson(const nlohmann::json& json)
{
  return json.is_null() ? std::filesystem::path{} : std::filesystem::path{json.get<std::string>()};
}

std::vector<std::string> pathVectorToStrings(const std::vector<std::filesystem::path>& paths)
{
  std::vector<std::string> values;
  values.reserve(paths.size());
  for (const auto& path : paths) {
    values.push_back(pathToString(path));
  }
  return values;
}

std::vector<std::filesystem::path> pathVectorFromJson(const nlohmann::json& json)
{
  std::vector<std::filesystem::path> paths;
  paths.reserve(json.size());
  for (const auto& item : json) {
    paths.push_back(pathFromJson(item));
  }
  return paths;
}

template<typename T>
void getOptional(const nlohmann::json& json, const char* key, T& value)
{
  if (json.contains(key)) {
    json.at(key).get_to(value);
  }
}

void optionalDoubleToJson(nlohmann::json& json, const char* key, const std::optional<double>& value)
{
  if (value) {
    json[key] = *value;
  }
}

std::optional<double> optionalDoubleFromJson(const nlohmann::json& json, const char* key)
{
  if (!json.contains(key) || json.at(key).is_null()) {
    return std::nullopt;
  }
  return json.at(key).get<double>();
}

} // namespace

void to_json(nlohmann::json& j, const Backend& value)
{
  j = enumToJson(value);
}
void from_json(const nlohmann::json& j, Backend& value)
{
  value = enumFromJson<Backend>(j, backendFromString);
}

void to_json(nlohmann::json& j, const TransformModel& value)
{
  j = enumToJson(value);
}
void from_json(const nlohmann::json& j, TransformModel& value)
{
  value = enumFromJson<TransformModel>(j, transformModelFromString);
}

void to_json(nlohmann::json& j, const Metric& value)
{
  j = enumToJson(value);
}
void from_json(const nlohmann::json& j, Metric& value)
{
  value = enumFromJson<Metric>(j, metricFromString);
}

void to_json(nlohmann::json& j, const Feature& value)
{
  j = enumToJson(value);
}
void from_json(const nlohmann::json& j, Feature& value)
{
  value = enumFromJson<Feature>(j, featureFromString);
}

void to_json(nlohmann::json& j, const ParameterKind& value)
{
  j = enumToJson(value);
}
void from_json(const nlohmann::json& j, ParameterKind& value)
{
  value = enumFromJson<ParameterKind>(j, parameterKindFromString);
}

void to_json(nlohmann::json& j, const DataSource& value)
{
  j = enumToJson(value);
}
void from_json(const nlohmann::json& j, DataSource& value)
{
  value = enumFromJson<DataSource>(j, dataSourceFromString);
}

void to_json(nlohmann::json& j, const Interpolation& value)
{
  j = enumToJson(value);
}
void from_json(const nlohmann::json& j, Interpolation& value)
{
  value = enumFromJson<Interpolation>(j, interpolationFromString);
}

void to_json(nlohmann::json& j, const ProgressEventKind& value)
{
  j = enumToJson(value);
}
void from_json(const nlohmann::json& j, ProgressEventKind& value)
{
  value = enumFromJson<ProgressEventKind>(j, progressEventKindFromString);
}

void to_json(nlohmann::json& j, const DataRef& value)
{
  j = nlohmann::json{
    {"uid", value.uid},
    {"fileName", pathToString(value.fileName)},
    {"displayName", value.displayName},
    {"source", enumToJson(value.source)}};
}

void from_json(const nlohmann::json& j, DataRef& value)
{
  getOptional(j, "uid", value.uid);
  if (j.contains("fileName")) {
    value.fileName = pathFromJson(j.at("fileName"));
  }
  getOptional(j, "displayName", value.displayName);
  if (j.contains("source")) {
    value.source = enumFromJson<DataSource>(j.at("source"), dataSourceFromString);
  }
}

void to_json(nlohmann::json& j, const AuxiliaryImagePair& value)
{
  j = nlohmann::json{
    {"fixed", value.fixed},
    {"moving", value.moving},
    {"weight", value.weight},
    {"metric", enumToJson(value.metric)}};
}

void from_json(const nlohmann::json& j, AuxiliaryImagePair& value)
{
  getOptional(j, "fixed", value.fixed);
  getOptional(j, "moving", value.moving);
  getOptional(j, "weight", value.weight);
  if (j.contains("metric")) {
    value.metric = enumFromJson<Metric>(j.at("metric"), metricFromString);
  }
}

void to_json(nlohmann::json& j, const LandmarkOptions& value)
{
  j = nlohmann::json{
    {"enabled", value.enabled},
    {"fixedLandmarks", value.fixedLandmarks},
    {"movingLandmarks", value.movingLandmarks},
    {"pairingMode", value.pairingMode == LandmarkOptions::PairingMode::ByName ? "ByName" : "ByOrder"},
    {"matchedPairs", value.matchedPairs}};
}

void from_json(const nlohmann::json& j, LandmarkOptions& value)
{
  getOptional(j, "enabled", value.enabled);
  getOptional(j, "fixedLandmarks", value.fixedLandmarks);
  getOptional(j, "movingLandmarks", value.movingLandmarks);
  if (j.contains("pairingMode")) {
    const std::string mode = j.at("pairingMode").get<std::string>();
    value.pairingMode =
      mode == "ByOrder" ? LandmarkOptions::PairingMode::ByOrder : LandmarkOptions::PairingMode::ByName;
  }
  getOptional(j, "matchedPairs", value.matchedPairs);
}

void to_json(nlohmann::json& j, const OutputRequests& value)
{
  j = nlohmann::json{
    {"loadWarpedImage", value.loadWarpedImage},
    {"loadAffineTransform", value.loadAffineTransform},
    {"loadInverseWarp", value.loadInverseWarp},
    {"loadForwardWarp", value.loadForwardWarp},
    {"applyWarpToMovingImage", value.applyWarpToMovingImage},
    {"loadWarpedSegmentation", value.loadWarpedSegmentation},
    {"transformLandmarksAndAnnotations", value.transformLandmarksAndAnnotations},
    {"transformSurfaces", value.transformSurfaces},
    {"makeWarpedImageActive", value.makeWarpedImageActive}};
}

void from_json(const nlohmann::json& j, OutputRequests& value)
{
  getOptional(j, "loadWarpedImage", value.loadWarpedImage);
  getOptional(j, "loadAffineTransform", value.loadAffineTransform);
  getOptional(j, "loadInverseWarp", value.loadInverseWarp);
  getOptional(j, "loadForwardWarp", value.loadForwardWarp);
  getOptional(j, "applyWarpToMovingImage", value.applyWarpToMovingImage);
  getOptional(j, "loadWarpedSegmentation", value.loadWarpedSegmentation);
  getOptional(j, "transformLandmarksAndAnnotations", value.transformLandmarksAndAnnotations);
  getOptional(j, "transformSurfaces", value.transformSurfaces);
  getOptional(j, "makeWarpedImageActive", value.makeWarpedImageActive);
}

void to_json(nlohmann::json& j, const ParameterSchema& value)
{
  j = nlohmann::json{
    {"key", value.key},
    {"label", value.label},
    {"tooltip", value.tooltip},
    {"kind", enumToJson(value.kind)},
    {"defaultValue", value.defaultValue},
    {"choices", value.choices},
    {"advanced", value.advanced},
    {"expert", value.expert}};
  optionalDoubleToJson(j, "minValue", value.minValue);
  optionalDoubleToJson(j, "maxValue", value.maxValue);
}

void from_json(const nlohmann::json& j, ParameterSchema& value)
{
  getOptional(j, "key", value.key);
  getOptional(j, "label", value.label);
  getOptional(j, "tooltip", value.tooltip);
  if (j.contains("kind")) {
    value.kind = enumFromJson<ParameterKind>(j.at("kind"), parameterKindFromString);
  }
  getOptional(j, "defaultValue", value.defaultValue);
  value.minValue = optionalDoubleFromJson(j, "minValue");
  value.maxValue = optionalDoubleFromJson(j, "maxValue");
  getOptional(j, "choices", value.choices);
  getOptional(j, "advanced", value.advanced);
  getOptional(j, "expert", value.expert);
}

void to_json(nlohmann::json& j, const ParameterValue& value)
{
  j = nlohmann::json{{"key", value.key}, {"value", value.value}};
}

void from_json(const nlohmann::json& j, ParameterValue& value)
{
  getOptional(j, "key", value.key);
  getOptional(j, "value", value.value);
}

void to_json(nlohmann::json& j, const BackendCapabilities& value)
{
  j = nlohmann::json{
    {"backend", value.backend},
    {"transformModels", value.transformModels},
    {"metrics", value.metrics},
    {"features", value.features},
    {"interpolations", value.interpolations},
    {"parameters", value.parameters}};
}

void from_json(const nlohmann::json& j, BackendCapabilities& value)
{
  if (j.contains("backend")) {
    value.backend = enumFromJson<Backend>(j.at("backend"), backendFromString);
  }
  getOptional(j, "transformModels", value.transformModels);
  getOptional(j, "metrics", value.metrics);
  getOptional(j, "features", value.features);
  getOptional(j, "interpolations", value.interpolations);
  getOptional(j, "parameters", value.parameters);
}

void to_json(nlohmann::json& j, const JobSpec& value)
{
  j = nlohmann::json{
    {"version", value.version},
    {"backend", value.backend},
    {"dimension", value.dimension},
    {"fixedImage", value.fixedImage},
    {"movingImage", value.movingImage},
    {"transformModel", value.transformModel},
    {"metric", value.metric},
    {"iterationSchedule", value.iterationSchedule},
    {"useImageCentersForInitialization", value.useImageCentersForInitialization},
    {"useCurrentAffineTransformsForInitialization", value.useCurrentAffineTransformsForInitialization},
    {"initialAffineTransform", pathToString(value.initialAffineTransform)},
    {"fixedMask", value.fixedMask},
    {"movingMask", value.movingMask},
    {"auxiliaryImagePairs", value.auxiliaryImagePairs},
    {"landmarks", value.landmarks},
    {"surfaces", value.surfaces},
    {"outputs", value.outputs},
    {"outputDirectory", pathToString(value.outputDirectory)},
    {"outputPrefix", value.outputPrefix},
    {"parameterValues", value.parameterValues},
    {"extraArguments", value.extraArguments}};
}

void from_json(const nlohmann::json& j, JobSpec& value)
{
  getOptional(j, "version", value.version);
  if (j.contains("backend")) {
    value.backend = enumFromJson<Backend>(j.at("backend"), backendFromString);
  }
  getOptional(j, "dimension", value.dimension);
  getOptional(j, "fixedImage", value.fixedImage);
  getOptional(j, "movingImage", value.movingImage);
  if (j.contains("transformModel")) {
    value.transformModel = enumFromJson<TransformModel>(j.at("transformModel"), transformModelFromString);
  }
  if (j.contains("metric")) {
    value.metric = enumFromJson<Metric>(j.at("metric"), metricFromString);
  }
  getOptional(j, "iterationSchedule", value.iterationSchedule);
  getOptional(j, "useImageCentersForInitialization", value.useImageCentersForInitialization);
  getOptional(j, "useCurrentAffineTransformsForInitialization", value.useCurrentAffineTransformsForInitialization);
  if (j.contains("initialAffineTransform")) {
    value.initialAffineTransform = pathFromJson(j.at("initialAffineTransform"));
  }
  getOptional(j, "fixedMask", value.fixedMask);
  getOptional(j, "movingMask", value.movingMask);
  getOptional(j, "auxiliaryImagePairs", value.auxiliaryImagePairs);
  getOptional(j, "landmarks", value.landmarks);
  getOptional(j, "surfaces", value.surfaces);
  getOptional(j, "outputs", value.outputs);
  if (j.contains("outputDirectory")) {
    value.outputDirectory = pathFromJson(j.at("outputDirectory"));
  }
  getOptional(j, "outputPrefix", value.outputPrefix);
  getOptional(j, "parameterValues", value.parameterValues);
  getOptional(j, "extraArguments", value.extraArguments);
}

void to_json(nlohmann::json& j, const ProgressEvent& value)
{
  j = nlohmann::json{
    {"event", value.kind},
    {"jobId", value.jobId},
    {"stageName", value.stageName},
    {"message", value.message},
    {"artifactPath", pathToString(value.artifactPath)},
    {"artifactKind", value.artifactKind}};
  if (value.stageIndex) {
    j["stageIndex"] = *value.stageIndex;
  }
  if (value.levelIndex) {
    j["levelIndex"] = *value.levelIndex;
  }
  if (value.iteration) {
    j["iteration"] = *value.iteration;
  }
  if (value.iterations) {
    j["iterations"] = *value.iterations;
  }
  if (value.progress) {
    j["progress"] = *value.progress;
  }
  if (value.loss) {
    j["loss"] = *value.loss;
  }
}

void from_json(const nlohmann::json& j, ProgressEvent& value)
{
  if (j.contains("event")) {
    value.kind = enumFromJson<ProgressEventKind>(j.at("event"), progressEventKindFromString);
  }
  getOptional(j, "jobId", value.jobId);
  getOptional(j, "stageName", value.stageName);
  if (j.contains("stageIndex")) {
    value.stageIndex = j.at("stageIndex").get<int>();
  }
  if (j.contains("levelIndex")) {
    value.levelIndex = j.at("levelIndex").get<int>();
  }
  if (j.contains("iteration")) {
    value.iteration = j.at("iteration").get<int>();
  }
  if (j.contains("iterations")) {
    value.iterations = j.at("iterations").get<int>();
  }
  if (j.contains("progress")) {
    value.progress = j.at("progress").get<double>();
  }
  if (j.contains("loss")) {
    value.loss = j.at("loss").get<double>();
  }
  getOptional(j, "message", value.message);
  if (j.contains("artifactPath")) {
    value.artifactPath = pathFromJson(j.at("artifactPath"));
  }
  getOptional(j, "artifactKind", value.artifactKind);
}

void to_json(nlohmann::json& j, const ResultManifest& value)
{
  j = nlohmann::json{
    {"version", value.version},
    {"success", value.success},
    {"backend", value.backend},
    {"fixedImageUid", value.fixedImageUid},
    {"movingImageUid", value.movingImageUid},
    {"warpedImage", pathToString(value.warpedImage)},
    {"inverseWarp", pathToString(value.inverseWarp)},
    {"forwardWarp", pathToString(value.forwardWarp)},
    {"affineTransform", pathToString(value.affineTransform)},
    {"warpedSegmentations", pathVectorToStrings(value.warpedSegmentations)},
    {"transformedSurfaces", pathVectorToStrings(value.transformedSurfaces)},
    {"transformedLandmarks", pathVectorToStrings(value.transformedLandmarks)},
    {"warnings", value.warnings},
    {"errorMessage", value.errorMessage},
    {"warpConvention", value.warpConvention},
    {"elapsedSeconds", value.elapsedSeconds}};
}

void from_json(const nlohmann::json& j, ResultManifest& value)
{
  getOptional(j, "version", value.version);
  getOptional(j, "success", value.success);
  if (j.contains("backend")) {
    value.backend = enumFromJson<Backend>(j.at("backend"), backendFromString);
  }
  getOptional(j, "fixedImageUid", value.fixedImageUid);
  getOptional(j, "movingImageUid", value.movingImageUid);
  if (j.contains("warpedImage")) {
    value.warpedImage = pathFromJson(j.at("warpedImage"));
  }
  if (j.contains("inverseWarp")) {
    value.inverseWarp = pathFromJson(j.at("inverseWarp"));
  }
  if (j.contains("forwardWarp")) {
    value.forwardWarp = pathFromJson(j.at("forwardWarp"));
  }
  if (j.contains("affineTransform")) {
    value.affineTransform = pathFromJson(j.at("affineTransform"));
  }
  if (j.contains("warpedSegmentations")) {
    value.warpedSegmentations = pathVectorFromJson(j.at("warpedSegmentations"));
  }
  if (j.contains("transformedSurfaces")) {
    value.transformedSurfaces = pathVectorFromJson(j.at("transformedSurfaces"));
  }
  if (j.contains("transformedLandmarks")) {
    value.transformedLandmarks = pathVectorFromJson(j.at("transformedLandmarks"));
  }
  getOptional(j, "warnings", value.warnings);
  getOptional(j, "errorMessage", value.errorMessage);
  getOptional(j, "warpConvention", value.warpConvention);
  getOptional(j, "elapsedSeconds", value.elapsedSeconds);
}

void to_json(nlohmann::json& j, const BackendConfig& value)
{
  j = nlohmann::json{
    {"defaultBackend", value.defaultBackend},
    {"greedyExecutable", pathToString(value.greedyExecutable)},
    {"antsRegistrationExecutable", pathToString(value.antsRegistrationExecutable)},
    {"antsApplyTransformsExecutable", pathToString(value.antsApplyTransformsExecutable)},
    {"fireAntsPythonExecutable", pathToString(value.fireAntsPythonExecutable)},
    {"fireAntsBridgeModule", value.fireAntsBridgeModule},
    {"defaultOutputDirectory", pathToString(value.defaultOutputDirectory)},
    {"keepTemporaryFiles", value.keepTemporaryFiles},
    {"maxConcurrentJobs", value.maxConcurrentJobs},
    {"defaultCpuThreadCount", value.defaultCpuThreadCount},
    {"defaultFireAntsDevice", value.defaultFireAntsDevice},
    {"showExpertOptionsByDefault", value.showExpertOptionsByDefault}};
}

void from_json(const nlohmann::json& j, BackendConfig& value)
{
  if (j.contains("defaultBackend")) {
    value.defaultBackend = enumFromJson<Backend>(j.at("defaultBackend"), backendFromString);
  }
  if (j.contains("greedyExecutable")) {
    value.greedyExecutable = pathFromJson(j.at("greedyExecutable"));
  }
  if (j.contains("antsRegistrationExecutable")) {
    value.antsRegistrationExecutable = pathFromJson(j.at("antsRegistrationExecutable"));
  }
  if (j.contains("antsApplyTransformsExecutable")) {
    value.antsApplyTransformsExecutable = pathFromJson(j.at("antsApplyTransformsExecutable"));
  }
  if (j.contains("fireAntsPythonExecutable")) {
    value.fireAntsPythonExecutable = pathFromJson(j.at("fireAntsPythonExecutable"));
  }
  getOptional(j, "fireAntsBridgeModule", value.fireAntsBridgeModule);
  if (j.contains("defaultOutputDirectory")) {
    value.defaultOutputDirectory = pathFromJson(j.at("defaultOutputDirectory"));
  }
  getOptional(j, "keepTemporaryFiles", value.keepTemporaryFiles);
  getOptional(j, "maxConcurrentJobs", value.maxConcurrentJobs);
  getOptional(j, "defaultCpuThreadCount", value.defaultCpuThreadCount);
  getOptional(j, "defaultFireAntsDevice", value.defaultFireAntsDevice);
  getOptional(j, "showExpertOptionsByDefault", value.showExpertOptionsByDefault);
}

} // namespace registration
