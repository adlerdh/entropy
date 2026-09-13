#pragma once

#include "registration/Config.h"
#include "registration/Types.h"

#include <nlohmann/json_fwd.hpp>

namespace registration
{

void to_json(nlohmann::json& j, const Backend& value);
void from_json(const nlohmann::json& j, Backend& value);

void to_json(nlohmann::json& j, const TransformModel& value);
void from_json(const nlohmann::json& j, TransformModel& value);

void to_json(nlohmann::json& j, const Metric& value);
void from_json(const nlohmann::json& j, Metric& value);

void to_json(nlohmann::json& j, const Feature& value);
void from_json(const nlohmann::json& j, Feature& value);

void to_json(nlohmann::json& j, const ParameterKind& value);
void from_json(const nlohmann::json& j, ParameterKind& value);

void to_json(nlohmann::json& j, const DataSource& value);
void from_json(const nlohmann::json& j, DataSource& value);

void to_json(nlohmann::json& j, const Interpolation& value);
void from_json(const nlohmann::json& j, Interpolation& value);

void to_json(nlohmann::json& j, const ProgressEventKind& value);
void from_json(const nlohmann::json& j, ProgressEventKind& value);

void to_json(nlohmann::json& j, const DataRef& value);
void from_json(const nlohmann::json& j, DataRef& value);

void to_json(nlohmann::json& j, const AuxiliaryImagePair& value);
void from_json(const nlohmann::json& j, AuxiliaryImagePair& value);

void to_json(nlohmann::json& j, const LandmarkOptions& value);
void from_json(const nlohmann::json& j, LandmarkOptions& value);

void to_json(nlohmann::json& j, const OutputRequests& value);
void from_json(const nlohmann::json& j, OutputRequests& value);

void to_json(nlohmann::json& j, const ParameterSchema& value);
void from_json(const nlohmann::json& j, ParameterSchema& value);

void to_json(nlohmann::json& j, const ParameterValue& value);
void from_json(const nlohmann::json& j, ParameterValue& value);

void to_json(nlohmann::json& j, const BackendCapabilities& value);
void from_json(const nlohmann::json& j, BackendCapabilities& value);

void to_json(nlohmann::json& j, const JobSpec& value);
void from_json(const nlohmann::json& j, JobSpec& value);

void to_json(nlohmann::json& j, const ProgressEvent& value);
void from_json(const nlohmann::json& j, ProgressEvent& value);

void to_json(nlohmann::json& j, const ResultManifest& value);
void from_json(const nlohmann::json& j, ResultManifest& value);

void to_json(nlohmann::json& j, const BackendConfig& value);
void from_json(const nlohmann::json& j, BackendConfig& value);

} // namespace registration
