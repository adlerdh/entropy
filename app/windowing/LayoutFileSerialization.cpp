#include "windowing/LayoutFileSerialization.h"

#include "windowing/LayoutSpecJson.h"

#include <nlohmann/json.hpp>
#include <spdlog/fmt/std.h>
#include <spdlog/spdlog.h>

#include <fstream>

namespace layout
{
namespace
{
constexpr int k_layoutFileVersion = 1;
constexpr const char* k_layoutFileFormat = "EntropyLayouts";
} // namespace

bool open(LayoutFile& file, const std::filesystem::path& fileName)
{
  std::ifstream in(fileName);
  if (!in) {
    spdlog::error("Could not open layout file {}", fileName);
    return false;
  }

  nlohmann::json json;
  try {
    in >> json;

    if (json.is_array()) {
      file.m_currentLayoutIndex = std::nullopt;
      file.m_layouts = json.get<std::vector<LayoutSpec>>();
      return true;
    }

    if (!json.is_object()) {
      spdlog::error("Layout file {} must contain a JSON object or layout array", fileName);
      return false;
    }

    const std::string format = json.value("format", std::string{k_layoutFileFormat});
    if (format != k_layoutFileFormat) {
      spdlog::error("Layout file {} has unsupported format '{}'", fileName, format);
      return false;
    }

    const int version = json.value("version", k_layoutFileVersion);
    if (version > k_layoutFileVersion) {
      spdlog::error("Layout file {} has unsupported version {}", fileName, version);
      return false;
    }

    if (json.contains("currentLayout") && !json.at("currentLayout").is_null()) {
      file.m_currentLayoutIndex = json.at("currentLayout").get<std::size_t>();
    }
    else {
      file.m_currentLayoutIndex = std::nullopt;
    }

    file.m_layouts = json.at("layouts").get<std::vector<LayoutSpec>>();
  }
  catch (const std::exception& e) {
    spdlog::error("Could not parse layout file {}: {}", fileName, e.what());
    return false;
  }

  return true;
}

bool save(const LayoutFile& file, const std::filesystem::path& fileName)
{
  nlohmann::json json{
    {"format", k_layoutFileFormat},
    {"version", k_layoutFileVersion},
    {"currentLayout", file.m_currentLayoutIndex ? nlohmann::json(*file.m_currentLayoutIndex) : nlohmann::json(nullptr)},
    {"layouts", file.m_layouts}};

  std::ofstream out(fileName);
  if (!out) {
    spdlog::error("Could not open layout file {} for writing", fileName);
    return false;
  }

  out << json.dump(2) << '\n';
  return true;
}

} // namespace layout
