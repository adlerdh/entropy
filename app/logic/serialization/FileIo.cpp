#include "logic/serialization/ProjectSerialization.h"
#include "logic/annotation/SerializeAnnot.h"

#include <safeclib/strerrorlen_s.h>

#include <spdlog/fmt/std.h>
#include <spdlog/spdlog.h>

#include <cerrno>
#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <exception>
#include <fstream>
#include <iomanip>
#include <limits>
#include <cmath>
#include <memory>
#include <sstream>
#include <string>
#include <system_error>
#include <vector>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#endif

#if !defined(_MSC_VER)
#define HAS_IOS_BASE_FAILURE_DERIVED_FROM_SYSTEM_ERROR 1
#else
#define HAS_IOS_BASE_FAILURE_DERIVED_FROM_SYSTEM_ERROR 0
#endif

using json = nlohmann::json;

namespace fs = std::filesystem;

namespace
{
fs::path temporaryJsonSiblingPath(const fs::path& destination)
{
  static std::atomic_uint64_t sequence{0};
  const auto timestamp = std::chrono::system_clock::now().time_since_epoch().count();
  fs::path name{"."};
  name += destination.filename().native();
  name += ".tmp-";
  name += std::to_string(timestamp);
  name += "-";
  name += std::to_string(++sequence);
  return destination.parent_path() / name;
}

void replaceJsonFile(const fs::path& temporary, const fs::path& destination)
{
#if defined(_WIN32)
  if (!MoveFileExW(temporary.c_str(), destination.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
    throw std::system_error(static_cast<int>(GetLastError()), std::system_category(), "Failed to replace JSON file");
  }
#else
  fs::rename(temporary, destination);
#endif
}

bool parseCsvRow(const std::string& line, std::vector<std::string>& fields)
{
  fields.clear();
  std::string field;
  bool quoted = false;
  bool closedQuote = false;
  for (std::size_t i = 0; i < line.size(); ++i) {
    const char ch = line[i];
    if (quoted) {
      if (ch == '"' && i + 1 < line.size() && line[i + 1] == '"') {
        field += '"';
        ++i;
      }
      else if (ch == '"') {
        quoted = false;
        closedQuote = true;
      }
      else {
        field += ch;
      }
    }
    else if (ch == ',') {
      fields.push_back(std::move(field));
      field.clear();
      closedQuote = false;
    }
    else if (ch == '"' && field.empty() && !closedQuote) {
      quoted = true;
    }
    else if (closedQuote || ch == '"') {
      return false;
    }
    else {
      field += ch;
    }
  }
  if (quoted) {
    return false;
  }
  fields.push_back(std::move(field));
  return true;
}

bool readCsvRow(std::istream& input, std::vector<std::string>& fields, std::size_t& lineNumber)
{
  std::string line;
  if (!std::getline(input, line)) {
    return false;
  }
  ++lineNumber;
  if (!line.empty() && line.back() == '\r') {
    line.pop_back();
  }
  while (!parseCsvRow(line, fields)) {
    std::string continuation;
    if (!std::getline(input, continuation)) {
      return false;
    }
    ++lineNumber;
    if (!continuation.empty() && continuation.back() == '\r') {
      continuation.pop_back();
    }
    line += '\n';
    line += continuation;
  }
  return true;
}

template<typename T, typename Parse>
bool parseCsvNumber(const std::string& value, T& result, Parse parse)
{
  try {
    std::size_t used = 0;
    result = parse(value, &used);
    return used == value.size();
  }
  catch (const std::exception&) {
    return false;
  }
}

std::string quoteCsvField(const std::string& value)
{
  if (value.find_first_of(",\"\r\n") == std::string::npos) {
    return value;
  }
  std::string quoted = "\"";
  for (const char ch : value) {
    if (ch == '"') {
      quoted += '"';
    }
    quoted += ch;
  }
  quoted += '"';
  return quoted;
}

#if !HAS_IOS_BASE_FAILURE_DERIVED_FROM_SYSTEM_ERROR
void logStdErrno()
{
  const size_t errmsglen = safeclib::strerrorlen_s(errno) + 1;
  std::unique_ptr<char[]> errmsg(new char[errmsglen]);
  strerror_s(errmsg.get(), errmsglen, errno);

  spdlog::error("Error #{}: {}", errno, errmsg.get());
}
#endif
} // namespace

namespace serialize
{

bool openAffineTxFile(glm::dmat4& matrix, const fs::path& fileName)
{
  std::ifstream inFile;
  inFile.exceptions(inFile.exceptions() | std::ifstream::badbit);

  try {
    inFile.open(fileName, std::ios_base::in);

    if (!inFile) {
      throw std::system_error(errno, std::system_category(), "Failed to open input file " + fileName.string());
    }

    std::vector<std::array<double, 4>> rows;
    std::string temp;

    while (std::getline(inFile, temp)) {
      std::istringstream buffer(temp);

      std::array<double, 4> row{};
      bool validRow = true;
      for (double& value : row) {
        if (!(buffer >> value)) {
          validRow = false;
          break;
        }
      }
      double extraValue = 0.0;
      if (!validRow || (buffer >> extraValue)) {
        throw std::length_error(
          fmt::format("4x4 affine matrix row {} must contain exactly four numbers", rows.size() + 1));
      }

      rows.push_back(row);
    }

    if (4 != rows.size()) {
      throw std::length_error(fmt::format("4x4 affine matrix read with invalid number of rows ({})", rows.size()));
    }

    for (uint32_t c = 0; c < 4; ++c) {
      for (uint32_t r = 0; r < 4; ++r) {
        matrix[static_cast<int>(c)][static_cast<int>(r)] = rows[r][c];
      }
    }

    return true;
  }
  catch (const std::ios_base::failure& e) {
#if HAS_IOS_BASE_FAILURE_DERIVED_FROM_SYSTEM_ERROR
    // e.code() is only available if the lib actually follows ISO §27.5.3.1.1
    // and derives ios_base::failure from system_error
    spdlog::error("Error #{} on opening file {}: {}", e.code().value(), fileName, e.code().message());

    if (std::make_error_condition(std::io_errc::stream) == e.code()) {
      spdlog::error("Stream error opening file {}", fileName);
    }
    else {
      spdlog::error("Unknown failure opening file {}", fileName);
    }
#else
    logStdErrno();
#endif

    spdlog::error("Failure reading affine transformation from file {}: {}", fileName, e.what());
    return false;
  }
  catch (const std::exception& e) {
    spdlog::error("Invalid 4x4 affine transformation matrix in file {}: {}", fileName, e.what());
    return false;
  }
}

bool saveAffineTxFile(const glm::dmat4& matrix, const fs::path& fileName)
{
  std::ofstream outFile;
  outFile.exceptions(outFile.exceptions() | std::ofstream::badbit | std::ofstream::failbit);

  try {
    outFile.open(fileName, std::ofstream::out);

    if (!outFile) {
      throw std::system_error(errno, std::system_category(), "Failed to open output file " + fileName.string());
    }

    for (int r = 0; r < 4; ++r) {
      for (int c = 0; c < 4; ++c) {
        outFile << matrix[c][r] << " ";
      }
      outFile << "\n";
    }

    return true;
  }
  catch (const std::ios_base::failure& e) {
#if HAS_IOS_BASE_FAILURE_DERIVED_FROM_SYSTEM_ERROR
    // e.code() is only available if the lib actually follows ISO §27.5.3.1.1
    // and derives ios_base::failure from system_error
    spdlog::error("Error #{} on opening file {}: {}", e.code().value(), fileName, e.code().message());

    if (std::make_error_condition(std::io_errc::stream) == e.code()) {
      spdlog::error("Stream error opening file {}", fileName);
    }
    else {
      spdlog::error("Unknown failure opening file {}", fileName);
    }
#else
    logStdErrno();
#endif

    spdlog::error("Failure writing affine transformation to file {}: {}", fileName, e.what());
    return false;
  }
  catch (const std::exception& e) {
    spdlog::error("Could not write 4x4 affine transformation matrix to file {}: {}", fileName, e.what());
    return false;
  }
}

bool openLandmarkGroupCsvFile(std::map<std::size_t, PointRecord<glm::vec3>>& landmarks, const fs::path& csvFileName)
{
  std::ifstream inFile;
  inFile.exceptions(inFile.exceptions() | std::ifstream::badbit);

  try {
    spdlog::debug("Opening landmarks CSV file {}", csvFileName);
    inFile.open(csvFileName, std::ios_base::in);

    if (!inFile || !inFile.good()) {
      spdlog::error("Error opening landmarks CSV file {}", csvFileName);
      throw std::system_error(errno, std::system_category(), "Failed to open CSV file " + csvFileName.string());
    }

    std::vector<std::string> fields;
    std::size_t lineNum = 0;
    if (!readCsvRow(inFile, fields, lineNum) || (fields.size() != 4 && fields.size() != 5)) {
      spdlog::error("Landmarks CSV file {} must have four or five columns", csvFileName);
      return false;
    }
    const bool nameProvided = fields.size() == 5;
    std::map<std::size_t, PointRecord<glm::vec3>> parsed;
    while (inFile.peek() != std::char_traits<char>::eof()) {
      if (!readCsvRow(inFile, fields, lineNum)) {
        spdlog::error("Invalid CSV record ending on line {} of landmarks file {}", lineNum, csvFileName);
        return false;
      }
      if (fields.size() == 1 && fields.front().empty()) {
        continue;
      }
      if (fields.size() != (nameProvided ? 5u : 4u)) {
        spdlog::error("Invalid columns on line {} of landmarks CSV file {}", lineNum, csvFileName);
        return false;
      }
      std::size_t index = 0;
      glm::vec3 position{0.0f};
      if (
        !parseCsvNumber(
          fields[0],
          index,
          [](const std::string& s, std::size_t* used) { return std::stoull(s, used); }) ||
        fields[0].empty() || fields[0].front() == '-' ||
        !parseCsvNumber(
          fields[1],
          position.x,
          [](const std::string& s, std::size_t* used) { return std::stof(s, used); }) ||
        !parseCsvNumber(
          fields[2],
          position.y,
          [](const std::string& s, std::size_t* used) { return std::stof(s, used); }) ||
        !parseCsvNumber(
          fields[3],
          position.z,
          [](const std::string& s, std::size_t* used) { return std::stof(s, used); }) ||
        !std::isfinite(position.x) || !std::isfinite(position.y) || !std::isfinite(position.z))
      {
        spdlog::error("Invalid landmark ID or position on line {} of CSV file {}", lineNum, csvFileName);
        return false;
      }
      if (!parsed.try_emplace(index, position, nameProvided ? fields[4] : "").second) {
        spdlog::error("Duplicate landmark ID {} on line {} of CSV file {}", index, lineNum, csvFileName);
        return false;
      }
    }
    landmarks = std::move(parsed);
    return true;
  }
  catch (const std::ios_base::failure& e) {
#if HAS_IOS_BASE_FAILURE_DERIVED_FROM_SYSTEM_ERROR
    // e.code() is only available if the lib actually follows ISO §27.5.3.1.1
    // and derives ios_base::failure from system_error
    spdlog::error("Error #{} on opening CSV file {}: {}", e.code().value(), csvFileName, e.code().message());

    if (std::make_error_condition(std::io_errc::stream) == e.code()) {
      spdlog::error("Stream error opening CSV file {}", csvFileName);
    }
    else {
      spdlog::error("Unknown failure opening CSV file {}", csvFileName);
    }
#else
    logStdErrno();
#endif

    spdlog::error("Failure reading landmark CSV file {}: {}", csvFileName, e.what());
    return false;
  }
  catch (const std::exception& e) {
    spdlog::error("Invalid landmark CSV file {}: {}", csvFileName, e.what());
    return false;
  }
}

bool saveLandmarkGroupCsvFile(
  const std::map<std::size_t, PointRecord<glm::vec3>>& landmarks,
  const fs::path& csvFileName)
{
  std::ofstream outFile;
  outFile.exceptions(outFile.exceptions() | std::ofstream::badbit | std::ofstream::failbit);

  try {
    outFile.open(csvFileName, std::ofstream::out);

    if (!outFile) {
      throw std::system_error(errno, std::system_category(), "Failed to open output CSV file " + csvFileName.string());
    }

    static const std::string sk_header("ID,X,Y,Z,Name");

    outFile << sk_header << "\n";

    outFile << std::setprecision(std::numeric_limits<float>::max_digits10);
    for (const auto& lm : landmarks) {
      const auto id = lm.first;
      const auto pos = lm.second.getPosition();
      const auto name = lm.second.getName();
      outFile << id << "," << pos.x << "," << pos.y << "," << pos.z << "," << quoteCsvField(name) << "\n";
    }

    return true;
  }
  catch (const std::ios_base::failure& e) {
#if HAS_IOS_BASE_FAILURE_DERIVED_FROM_SYSTEM_ERROR
    // e.code() is only available if the lib actually follows ISO §27.5.3.1.1
    // and derives ios_base::failure from system_error
    spdlog::error("Error #{} on opening CSV file {}: {}", e.code().value(), csvFileName, e.code().message());

    if (std::make_error_condition(std::io_errc::stream) == e.code()) {
      spdlog::error("Stream error opening CSV file {}", csvFileName);
    }
    else {
      spdlog::error("Unknown failure opening CSV file {}", csvFileName);
    }
#else
    logStdErrno();
#endif

    spdlog::error("Failure writing landmarks to CSV file {}: {}", csvFileName, e.what());
    return false;
  }
  catch (const std::exception& e) {
    spdlog::error("Could not write landmarks to CSV file {}: {}", csvFileName, e.what());
    return false;
  }
}

bool openAnnotationsFromJsonFile(std::vector<Annotation>& annots, const fs::path& jsonFileName)
{
  std::ifstream inFile;
  inFile.exceptions(inFile.exceptions() | std::ifstream::badbit);

  try {
    spdlog::debug("Opening annotations JSON file {}", jsonFileName);
    inFile.open(jsonFileName, std::ios_base::in);

    if (!inFile || !inFile.good()) {
      spdlog::error("Error opening annotations JSON file {}", jsonFileName);
      throw std::system_error(errno, std::system_category(), "Failed to open JSON file " + jsonFileName.string());
    }

    json j;
    inFile >> j;

    annots = annotationsFromJson(j);
    spdlog::debug("Parsed {} annotation(s) from JSON:\n{}", annots.size(), j.dump(2));
    return true;
  }
  catch (const std::ios_base::failure& e) {
#if HAS_IOS_BASE_FAILURE_DERIVED_FROM_SYSTEM_ERROR
    // e.code() is only available if the lib actually follows ISO §27.5.3.1.1
    // and derives ios_base::failure from system_error
    spdlog::error(
      "Error #{} on opening annotations JSON file {}: {}",
      e.code().value(),
      jsonFileName,
      e.code().message());

    if (std::make_error_condition(std::io_errc::stream) == e.code()) {
      spdlog::error("Stream error opening annotations JSON file {}", jsonFileName);
    }
    else {
      spdlog::error("Unknown failure opening annotations JSON file {}", jsonFileName);
    }
#else
    logStdErrno();
#endif

    spdlog::error("Failure reading annotations JSON file {}: {}", jsonFileName, e.what());
    return false;
  }
  catch (const std::exception& e) {
    spdlog::error("Invalid annotations JSON file {}: {}", jsonFileName, e.what());
    return false;
  }
}

bool saveToJsonFile(const nlohmann::json& j, const fs::path& jsonFileName)
{
  try {
    const std::string serialized = j.dump(2);
    const fs::path temporaryFile = temporaryJsonSiblingPath(jsonFileName);
    try {
      std::ofstream outFile;
      outFile.exceptions(std::ofstream::badbit | std::ofstream::failbit);
      outFile.open(temporaryFile, std::ios::out | std::ios::trunc);
      outFile << serialized << '\n';
      outFile.flush();
      outFile.close();
      replaceJsonFile(temporaryFile, jsonFileName);
    }
    catch (...) {
      std::error_code ignored;
      fs::remove(temporaryFile, ignored);
      throw;
    }

    spdlog::debug("Saved to JSON file {}:\n{}", jsonFileName, serialized);
    spdlog::info("Saved to JSON file {}", jsonFileName);
    return true;
  }
  catch (const std::ios_base::failure& e) {
#if HAS_IOS_BASE_FAILURE_DERIVED_FROM_SYSTEM_ERROR
    // e.code() is only available if the lib actually follows ISO §27.5.3.1.1
    // and derives ios_base::failure from system_error
    spdlog::error("Error #{} on opening JSON file {}: {}", e.code().value(), jsonFileName, e.code().message());

    if (std::make_error_condition(std::io_errc::stream) == e.code()) {
      spdlog::error("Stream error opening JSON file {}", jsonFileName);
    }
    else {
      spdlog::error("Unknown failure opening JSON file {}", jsonFileName);
    }
#else
    logStdErrno();
#endif

    spdlog::error("Failure writing to JSON file {}: {}", jsonFileName, e.what());
    return false;
  }
  catch (const std::exception& e) {
    spdlog::error("Could not write to JSON file {}: {}", jsonFileName, e.what());
    return false;
  }
}

} // namespace serialize
