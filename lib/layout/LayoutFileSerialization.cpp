#include "layout/LayoutFileSerialization.h"

#include <nlohmann/json.hpp>
#include <spdlog/fmt/std.h>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <fstream>
#include <sstream>
#include <stdexcept>

namespace layout
{
namespace
{
constexpr int k_layoutFileVersion = 1;
constexpr const char* k_layoutFileFormat = "EntropyLayouts";

std::optional<std::size_t> imageIndexFromJson(const nlohmann::json& json)
{
  if (json.is_null()) {
    return std::nullopt;
  }
  if (json.is_string()) {
    const std::string value = json.get<std::string>();
    if ("reference" == value) {
      return 0;
    }
    throw std::runtime_error("unsupported image selector: " + value);
  }
  return json.get<std::size_t>();
}

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

void to_json(nlohmann::json& json, const LayoutPreset& preset)
{
  json = nlohmann::json{{"type", preset.m_type}};
  if (!preset.m_view.empty()) {
    json["view"] = preset.m_view;
  }
  if (!preset.m_images.empty()) {
    json["images"] = preset.m_images;
  }
  else if (!preset.m_imageIndices.empty()) {
    json["images"] = preset.m_imageIndices;
  }
}

void from_json(const nlohmann::json& json, LayoutPreset& preset)
{
  json.at("type").get_to(preset.m_type);
  preset.m_view = json.value("view", std::string{});
  preset.m_images.clear();
  preset.m_imageIndices.clear();

  if (json.contains("images")) {
    const auto& images = json.at("images");
    if (images.is_string()) {
      preset.m_images = images.get<std::string>();
    }
    else {
      preset.m_imageIndices = images.get<std::vector<std::size_t>>();
    }
  }
  else if (json.contains("image")) {
    if (const auto imageIndex = imageIndexFromJson(json.at("image"))) {
      preset.m_imageIndices = {*imageIndex};
    }
  }
}

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
      file.m_layouts = json.get<std::vector<LayoutPreset>>();
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

    file.m_layouts = json.at("layouts").get<std::vector<LayoutPreset>>();
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

  out << dumpPretty(json) << '\n';
  return true;
}

} // namespace layout
