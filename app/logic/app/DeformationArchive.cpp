#include "logic/app/DeformationArchive.h"

#include "deformation/DeformationFieldIO.h"

#include <nlohmann/json.hpp>

#include <atomic>
#include <array>
#include <chrono>
#include <cstdint>
#include <fstream>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

namespace fs = std::filesystem;
using json = nlohmann::json;
namespace df = deformation;

namespace deformation_archive
{
namespace
{
json vector3(const glm::dvec3& value)
{
  return json::array({value.x, value.y, value.z});
}
glm::dvec3 parseVector3(const json& value)
{
  const auto v = value.get<std::array<double, 3>>();
  return {v[0], v[1], v[2]};
}
json frame3(const glm::dmat3& frame)
{
  json result = json::array();
  for (int axis = 0; axis < 3; ++axis)
    for (int row = 0; row < 3; ++row)
      result.push_back(frame[axis][row]);
  return result;
}
glm::dmat3 parseFrame3(const json& value)
{
  const auto values = value.get<std::array<double, 9>>();
  glm::dmat3 result(1.0);
  for (int axis = 0; axis < 3; ++axis)
    for (int row = 0; row < 3; ++row)
      result[axis][row] = values[axis * 3 + row];
  return result;
}
json domainJson(const df::FieldDomain& domain)
{
  const auto& extent = domain.validExtent();
  return json{
    {"dimension", static_cast<int>(domain.dimension())},
    {"size", domain.size()},
    {"origin", vector3(domain.origin())},
    {"spacing", vector3(domain.spacing())},
    {"directions", frame3(domain.directions())},
    {"validBegin", extent.begin},
    {"validEnd", extent.end}};
}
df::FieldDomain parseDomain(const json& value)
{
  df::DomainGeometry geometry;
  const int dimension = value.at("dimension").get<int>();
  if (dimension != 2 && dimension != 3) throw std::runtime_error("Invalid archived field dimension");
  geometry.dimension = dimension == 2 ? df::SpatialDimension::Plane : df::SpatialDimension::Volume;
  geometry.size = value.at("size").get<df::GridSize>();
  geometry.origin = parseVector3(value.at("origin"));
  geometry.spacing = parseVector3(value.at("spacing"));
  geometry.directions = parseFrame3(value.at("directions"));
  geometry.validExtent =
    df::IndexExtent{value.at("validBegin").get<df::GridSize>(), value.at("validEnd").get<df::GridSize>()};
  return df::FieldDomain(geometry);
}
bool sameDomain(const df::FieldDomain& left, const df::FieldDomain& right)
{
  if (
    left.dimension() != right.dimension() || left.size() != right.size() ||
    left.validExtent().begin != right.validExtent().begin || left.validExtent().end != right.validExtent().end ||
    left.origin() != right.origin() || left.spacing() != right.spacing())
    return false;
  for (int axis = 0; axis < 3; ++axis)
    if (left.directions()[axis] != right.directions()[axis]) return false;
  return true;
}
json directionJson(const df::DirectionQuality& direction)
{
  return json{
    {"requested", direction.requested},
    {"evaluated", direction.evaluated},
    {"outside", direction.outside},
    {"finite", direction.finite},
    {"minDeterminant", direction.minDeterminant},
    {"maxDeterminant", direction.maxDeterminant},
    {"minSingularValue", direction.minSingularValue},
    {"maxSingularValue", direction.maxSingularValue},
    {"maxResidualMm", direction.maxResidualMm},
    {"maxResidualVoxels", direction.maxResidualVoxels}};
}
df::DirectionQuality parseDirection(const json& value)
{
  df::DirectionQuality result;
  result.requested = value.at("requested").get<std::size_t>();
  result.evaluated = value.at("evaluated").get<std::size_t>();
  result.outside = value.at("outside").get<std::size_t>();
  result.finite = value.at("finite").get<bool>();
  result.minDeterminant = value.at("minDeterminant").get<double>();
  result.maxDeterminant = value.at("maxDeterminant").get<double>();
  result.minSingularValue = value.at("minSingularValue").get<double>();
  result.maxSingularValue = value.at("maxSingularValue").get<double>();
  result.maxResidualMm = value.at("maxResidualMm").get<double>();
  result.maxResidualVoxels = value.at("maxResidualVoxels").get<double>();
  return result;
}
json qualityJson(const df::QualityReport& report)
{
  return json{
    {"forward", directionJson(report.forward)},
    {"inverse", directionJson(report.inverse)},
    {"maxProtectionErrorMm", report.maxProtectionErrorMm},
    {"protectionChecked", report.protectionChecked},
    {"maxConvergenceErrorMm", report.maxConvergenceErrorMm},
    {"convergenceChecked", report.convergenceChecked},
    {"cellsVerified", report.cellsVerified}};
}
df::QualityReport parseQuality(const json& value)
{
  df::QualityReport result;
  result.forward = parseDirection(value.at("forward"));
  result.inverse = parseDirection(value.at("inverse"));
  result.maxProtectionErrorMm = value.at("maxProtectionErrorMm").get<double>();
  result.protectionChecked = value.at("protectionChecked").get<bool>();
  result.maxConvergenceErrorMm = value.at("maxConvergenceErrorMm").get<double>();
  result.convergenceChecked = value.at("convergenceChecked").get<bool>();
  result.cellsVerified = value.at("cellsVerified").get<bool>();
  return result;
}
json brushJson(const df::BrushDefinition& brush)
{
  json motion = std::visit(
    [](const auto& value) -> json {
      using T = std::decay_t<decltype(value)>;
      if constexpr (std::is_same_v<T, df::PushMotion>)
        return {{"type", "push"}, {"displacementMm", vector3(value.displacementMm)}};
      else if constexpr (std::is_same_v<T, df::RadialMotion>)
        return {{"type", "radial"}, {"exposure", value.exposure}};
      else
        return {{"type", "twirl"}, {"axis", vector3(value.axis)}, {"angleRadians", value.angleRadians}};
    },
    brush.motion);
  json protection = json::array();
  for (const auto& region : brush.protection) {
    protection.push_back(
      {{"centerMm", vector3(region.centerMm)},
       {"coreRadiusMm", region.coreRadiusMm},
       {"transitionMm", region.transitionMm}});
  }
  return json{
    {"dimension", static_cast<int>(brush.dimension)},
    {"centerMm", vector3(brush.centerMm)},
    {"directions", frame3(brush.directions)},
    {"radiusMm", brush.radiusMm},
    {"strength", brush.strength},
    {"motion", std::move(motion)},
    {"protection", std::move(protection)}};
}
df::BrushDefinition parseBrush(const json& value)
{
  df::BrushDefinition brush;
  const int dimension = value.at("dimension").get<int>();
  if (dimension != 2 && dimension != 3) throw std::runtime_error("Invalid archived brush dimension");
  brush.dimension = dimension == 2 ? df::SpatialDimension::Plane : df::SpatialDimension::Volume;
  brush.centerMm = parseVector3(value.at("centerMm"));
  brush.directions = parseFrame3(value.at("directions"));
  brush.radiusMm = value.at("radiusMm").get<double>();
  brush.strength = value.at("strength").get<double>();
  const auto& motion = value.at("motion");
  const std::string type = motion.at("type").get<std::string>();
  if (type == "push")
    brush.motion = df::PushMotion{parseVector3(motion.at("displacementMm"))};
  else if (type == "radial")
    brush.motion = df::RadialMotion{motion.at("exposure").get<double>()};
  else if (type == "twirl")
    brush.motion = df::TwirlMotion{parseVector3(motion.at("axis")), motion.at("angleRadians").get<double>()};
  else
    throw std::runtime_error("Invalid archived brush motion");
  for (const auto& region : value.at("protection")) {
    brush.protection.push_back(
      {parseVector3(region.at("centerMm")),
       region.at("coreRadiusMm").get<double>(),
       region.at("transitionMm").get<double>()});
  }
  static_cast<void>(df::BrushStep(brush));
  return brush;
}
fs::path revisionFile(df::RevisionId id, df::MapDirection role)
{
  return fs::path("revisions") / std::to_string(id.value) /
         (role == df::MapDirection::Forward ? "forward.nrrd" : "inverse.nrrd");
}
void validateEditId(const std::string& id)
{
  if (
    id.empty() ||
    id.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_-") != std::string::npos)
    throw std::invalid_argument("Edit ID must use letters, digits, underscores, or hyphens");
}
std::string uniqueBundleName()
{
  static std::atomic_uint64_t sequence{0};
  return "bundle-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + "-" +
         std::to_string(++sequence);
}
void writeManifest(const fs::path& staging, const std::string& editId, const df::EditHistory& history)
{
  const auto root = history.find({1});
  if (!root || history.size() == 0) throw std::invalid_argument("Edit history has no root");
  const auto& provenance = root->provenance();
  json manifest{
    {"schemaVersion", 1},
    {"editId", editId},
    {"sourceImageId", provenance.sourceImageId},
    {"referenceImageId", provenance.referenceImageId},
    {"baselineId", provenance.baselineId},
    {"frame", provenance.frame},
    {"sourceDomain", domainJson(root->maps().sourceDomain())},
    {"outputDomain", domainJson(root->maps().outputDomain())},
    {"vectorBasis", "LPS-mm"},
    {"fieldEncoding", "NRRD0005-RGBA32F-validity"},
    {"cursor", history.current().id().value},
    {"revisions", json::array()}};
  for (std::size_t index = 1; index <= history.size(); ++index) {
    const df::RevisionId id{index};
    const auto revision = history.find(id);
    if (!revision) throw std::invalid_argument("Edit history has a missing revision");
    const auto forward = revisionFile(id, df::MapDirection::Forward);
    const auto inverse = revisionFile(id, df::MapDirection::Inverse);
    fs::create_directories((staging / forward).parent_path());
    const df::FieldAsset forwardAsset{
      revision->maps().sourceDomain(),
      df::MapDirection::Forward,
      revision->checkpoint().forward};
    const df::FieldAsset inverseAsset{
      revision->maps().outputDomain(),
      df::MapDirection::Inverse,
      revision->checkpoint().inverse};
    df::validateFieldForExport(forwardAsset);
    df::validateFieldForExport(inverseAsset);
    df::writeNativeField(staging / forward, forwardAsset);
    df::writeNativeField(staging / inverse, inverseAsset);
    json stroke = json::array();
    for (const auto& brush : revision->stroke())
      stroke.push_back(brushJson(brush));
    manifest["revisions"].push_back(
      {{"id", id.value},
       {"parent", revision->parent().value},
       {"policyVersion", revision->maps().policyVersion().value},
       {"quality", qualityJson(revision->quality())},
       {"stroke", std::move(stroke)},
       {"forward", forward.generic_string()},
       {"inverse", inverse.generic_string()},
       {"forwardCrc32", df::nativeFieldPayloadChecksum(revision->checkpoint().forward)},
       {"inverseCrc32", df::nativeFieldPayloadChecksum(revision->checkpoint().inverse)}});
  }
  std::ofstream output(staging / "manifest.json", std::ios::binary | std::ios::trunc);
  if (!output) throw std::runtime_error("Cannot create deformation manifest");
  output << manifest.dump(2) << '\n';
  output.flush();
  if (!output) throw std::runtime_error("Cannot finish deformation manifest");
}
} // namespace

StagedBundle::StagedBundle(const fs::path& projectFile, std::string editId, const df::EditHistory& history)
  : m_editId(std::move(editId)), m_cursor(history.current().id())
{
  validateEditId(m_editId);
  const fs::path base =
    projectFile.parent_path() / (projectFile.filename().string() + ".assets") / "deformations" / m_editId;
  const std::string bundle = uniqueBundleName();
  m_staging = base / ("." + bundle + ".tmp");
  m_final = base / bundle;
  try {
    fs::create_directories(m_staging);
    writeManifest(m_staging, m_editId, history);
  }
  catch (...) {
    std::error_code ignored;
    fs::remove_all(m_staging, ignored);
    throw;
  }
}
StagedBundle::~StagedBundle()
{
  std::error_code ignored;
  fs::remove_all(m_staging, ignored);
  if (m_published && !m_committed) fs::remove_all(m_final, ignored);
}
serialize::ProjectDeformationReference StagedBundle::publish()
{
  if (m_published) throw std::logic_error("Deformation bundle was already published");
  fs::rename(m_staging, m_final);
  m_published = true;
  return {1, m_editId, m_final / "manifest.json", m_cursor.value};
}
void StagedBundle::commit() noexcept
{
  m_committed = true;
}

df::EditHistory loadBundle(
  const serialize::ProjectDeformationReference& reference,
  const std::size_t maxTotalFieldBytes)
{
  validateEditId(reference.m_editId);
  if (reference.m_schemaVersion != 1 || reference.m_acceptedRevision == 0)
    throw std::invalid_argument("Unsupported deformation project reference");
  if (fs::file_size(reference.m_manifestPath) > 32 * 1024 * 1024)
    throw std::runtime_error("Deformation manifest exceeds byte limit");
  std::ifstream input(reference.m_manifestPath, std::ios::binary);
  if (!input) throw std::runtime_error("Cannot open deformation manifest");
  const json manifest = json::parse(input);
  if (
    manifest.at("schemaVersion").get<unsigned>() != 1 ||
    manifest.at("editId").get<std::string>() != reference.m_editId ||
    manifest.at("cursor").get<std::uint64_t>() != reference.m_acceptedRevision ||
    manifest.at("vectorBasis").get<std::string>() != "LPS-mm" ||
    manifest.at("fieldEncoding").get<std::string>() != "NRRD0005-RGBA32F-validity")
    throw std::runtime_error("Incompatible deformation manifest");
  const auto source = parseDomain(manifest.at("sourceDomain"));
  const auto output = parseDomain(manifest.at("outputDomain"));
  const df::EditProvenance provenance{
    manifest.at("sourceImageId").get<std::string>(),
    manifest.at("referenceImageId").get<std::string>(),
    manifest.at("baselineId").get<std::string>(),
    manifest.at("frame").get<std::uint32_t>()};
  const auto& rows = manifest.at("revisions");
  if (!rows.is_array() || rows.empty()) throw std::runtime_error("Deformation manifest has no revisions");
  std::size_t usedBytes = 0;
  if (source.sampleCount() > maxTotalFieldBytes / 16 || output.sampleCount() > maxTotalFieldBytes / 16)
    throw std::runtime_error("Deformation field exceeds byte limit");
  std::shared_ptr<const df::FieldCheckpoint> initial;
  df::NumericalPolicyVersion rootPolicy;
  std::vector<df::ArchivedRevision> children;
  for (std::size_t index = 0; index < rows.size(); ++index) {
    const auto& row = rows.at(index);
    const df::RevisionId id{row.at("id").get<std::uint64_t>()};
    if (id.value != index + 1) throw std::runtime_error("Deformation revision IDs are not contiguous");
    const fs::path forward = row.at("forward").get<std::string>();
    const fs::path inverse = row.at("inverse").get<std::string>();
    if (
      forward != revisionFile(id, df::MapDirection::Forward) || inverse != revisionFile(id, df::MapDirection::Inverse))
      throw std::runtime_error("Deformation asset path disagrees with revision");
    const auto forwardBytes = source.sampleCount() * 16;
    const auto inverseBytes = output.sampleCount() * 16;
    if (forwardBytes > maxTotalFieldBytes - usedBytes || inverseBytes > maxTotalFieldBytes - usedBytes - forwardBytes)
      throw std::runtime_error("Deformation history exceeds byte limit");
    usedBytes += forwardBytes + inverseBytes;
    const auto base = reference.m_manifestPath.parent_path();
    auto forwardAsset = df::readNativeField(base / forward, forwardBytes);
    auto inverseAsset = df::readNativeField(base / inverse, inverseBytes);
    if (
      forwardAsset.role != df::MapDirection::Forward || inverseAsset.role != df::MapDirection::Inverse ||
      !sameDomain(forwardAsset.domain, source) || !sameDomain(inverseAsset.domain, output) ||
      df::nativeFieldPayloadChecksum(forwardAsset.samples) != row.at("forwardCrc32").get<std::uint32_t>() ||
      df::nativeFieldPayloadChecksum(inverseAsset.samples) != row.at("inverseCrc32").get<std::uint32_t>())
      throw std::runtime_error("Deformation revision asset mismatch");
    auto checkpoint = std::make_shared<df::FieldCheckpoint>();
    checkpoint->forward = std::move(forwardAsset.samples);
    checkpoint->inverse = std::move(inverseAsset.samples);
    const df::RevisionId parent{row.at("parent").get<std::uint64_t>()};
    const df::NumericalPolicyVersion policy{row.at("policyVersion").get<std::uint32_t>()};
    if (index == 0) {
      if (parent.value != 0 || !row.at("stroke").empty()) throw std::runtime_error("Invalid deformation root");
      rootPolicy = policy;
      initial = std::move(checkpoint);
    }
    else {
      std::vector<df::BrushDefinition> stroke;
      for (const auto& step : row.at("stroke"))
        stroke.push_back(parseBrush(step));
      children.push_back(
        {id, parent, policy, parseQuality(row.at("quality")), std::move(stroke), std::move(checkpoint)});
    }
  }
  return df::EditHistory::restore(
    source,
    output,
    provenance,
    rootPolicy,
    std::move(initial),
    children,
    df::RevisionId{reference.m_acceptedRevision});
}
} // namespace deformation_archive
