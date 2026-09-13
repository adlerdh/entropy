#include "layout/LayoutFileSerialization.h"

#include "layout/LayoutSpecJson.h"

#include <nlohmann/json.hpp>
#include <spdlog/fmt/std.h>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <fstream>
#include <sstream>

namespace layout
{
namespace
{
constexpr int k_layoutFileVersion = 1;
constexpr const char* k_layoutFileFormat = "EntropyLayouts";

std::string indentString(int indent)
{
  return std::string(static_cast<std::size_t>(std::max(indent, 0)), ' ');
}

bool isInlineArray(const nlohmann::json& json)
{
  return json.is_array() &&
         std::all_of(json.begin(), json.end(), [](const nlohmann::json& value) { return value.is_primitive(); });
}

std::string dumpInlineArray(const nlohmann::json& json)
{
  std::ostringstream out;
  out << '[';
  for (auto it = json.begin(); it != json.end(); ++it) {
    if (it != json.begin()) {
      out << ", ";
    }
    out << it->dump();
  }
  out << ']';
  return out.str();
}

std::string dumpPretty(const nlohmann::json& json, int indent = 0)
{
  if (json.is_object()) {
    if (json.empty()) {
      return "{}";
    }

    std::ostringstream out;
    out << "{\n";
    for (auto it = json.begin(); it != json.end(); ++it) {
      if (it != json.begin()) {
        out << ",\n";
      }
      out << indentString(indent + 2) << nlohmann::json(it.key()).dump() << ": " << dumpPretty(it.value(), indent + 2);
    }
    out << '\n' << indentString(indent) << '}';
    return out.str();
  }

  if (json.is_array()) {
    if (json.empty()) {
      return "[]";
    }
    if (isInlineArray(json)) {
      return dumpInlineArray(json);
    }

    std::ostringstream out;
    out << "[\n";
    for (auto it = json.begin(); it != json.end(); ++it) {
      if (it != json.begin()) {
        out << ",\n";
      }
      out << indentString(indent + 2) << dumpPretty(*it, indent + 2);
    }
    out << '\n' << indentString(indent) << ']';
    return out.str();
  }

  return json.dump();
}

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

    if (!json.is_object()) {
      spdlog::error("Layout file {} must contain a JSON object", fileName);
      return false;
    }

    const std::string format = json.value("format", std::string{k_layoutFileFormat});
    if (format != k_layoutFileFormat) {
      spdlog::error("Layout file {} has unsupported format '{}'", fileName, format);
      return false;
    }

    const auto version = json.at("version");
    if (!version.is_object()) {
      spdlog::error("Layout file {} has an invalid version", fileName);
      return false;
    }

    const int majorVersion = version.at("major").get<int>();
    const int minorVersion = version.at("minor").get<int>();
    if (majorVersion != k_layoutFileVersion || minorVersion != 0) {
      spdlog::error("Layout file {} has unsupported version {}.{}", fileName, majorVersion, minorVersion);
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
    {"version", {{"major", k_layoutFileVersion}, {"minor", 0}}},
    {"layouts", file.m_layouts}};
  if (file.m_currentLayoutIndex) {
    json["currentLayout"] = *file.m_currentLayoutIndex;
  }

  std::ofstream out(fileName);
  if (!out) {
    spdlog::error("Could not open layout file {} for writing", fileName);
    return false;
  }

  out << dumpPretty(json) << '\n';
  return true;
}

} // namespace layout
