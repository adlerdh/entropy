#include "logic/serialization/ProjectSerialization.h"
#include "logic/annotation/SerializeAnnot.h"

#include <safeclib/strerrorlen_s.h>

#include <spdlog/fmt/std.h>
#include <spdlog/spdlog.h>

#include <cerrno>
#include <cstdint>
#include <exception>
#include <fstream>
#include <iterator>
#include <memory>
#include <sstream>
#include <string>
#include <system_error>
#include <vector>

#if !defined(_MSC_VER)
#define HAS_IOS_BASE_FAILURE_DERIVED_FROM_SYSTEM_ERROR 1
#else
#define HAS_IOS_BASE_FAILURE_DERIVED_FROM_SYSTEM_ERROR 0
#endif

using json = nlohmann::json;

namespace fs = std::filesystem;

namespace
{
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

    std::vector<std::vector<double>> rows;
    std::string temp;

    while (std::getline(inFile, temp)) {
      std::istringstream buffer(temp);

      const std::vector<double> row{(std::istream_iterator<double>(buffer)), std::istream_iterator<double>()};

      if (4 != row.size()) {
        throw std::length_error(
          fmt::format("4x4 affine matrix row {} read with invalid length {}", rows.size() + 1, row.size()));
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

    int lineNum = 1;

    std::string line;
    std::string colName;
    int numCols = 0;

    // Read the first line
    std::getline(inFile, line);
    ++lineNum;

    // SPDLOG_TRACE( "\n\nReading line: {}\n\n\n", line );

    std::istringstream ssHeader(line);

    // Read the column headers into colName (they are not used)
    while (std::getline(ssHeader, colName, ',')) {
      spdlog::debug("Read column name {}", colName);
      ++numCols;
    }

    // The expected columns are (with the last column optional)
    // index ,X ,Y ,Z [,name]
    if (numCols < 4) {
      spdlog::error(
        "Expected at least four columns (id, x, y, z) when reading landmarks CSV file {}, "
        "but only read {} columns",
        csvFileName,
        numCols);
      return false;
    }

    // Is the name column provided?
    const bool nameProvided = (numCols >= 5);

    // Read all lines containing landmark data
    while (std::getline(inFile, line)) {
      // SPDLOG_TRACE( "Reading line: {}", line );

      std::stringstream ssLm(line);

      int landmarkIndex = 0;
      glm::vec3 landmarkPos{0.0f};
      std::optional<std::string> landmarkName;

      int col = 0;
      std::string val;

      while (std::getline(ssLm, val, ',')) {
        // SPDLOG_TRACE( "\tval: {}", val );

        switch (col) {
          case 0: {
            landmarkIndex = std::stoi(val);
            break;
          }
          case 1: {
            landmarkPos.x = std::stof(val);
            break;
          }
          case 2: {
            landmarkPos.y = std::stof(val);
            break;
          }
          case 3: {
            landmarkPos.z = std::stof(val);
            break;
          }
          case 4: {
            if (nameProvided) {
              landmarkName = val;
            }
            break;
          }
          default:
            break; // ignore any more columns
        }

        // If the next token is a comma, ignore it
        if (',' == ssLm.peek()) {
          ssLm.ignore();
        }

        ++col;
      }

      if (nameProvided && (col < numCols - 1)) {
        // The name is optional, so only check col against numCols - 1
        spdlog::error(
          "Line {} of landmarks CSV file {} has {} entries, which is less than the expected {} "
          "entries",
          lineNum,
          csvFileName,
          col,
          numCols - 1);
        return false;
      }
      else if (!nameProvided && (col < numCols)) {
        spdlog::error(
          "Line {} of landmarks CSV file {} has {} entries, which is less than the expected {} "
          "entries",
          lineNum,
          csvFileName,
          col,
          numCols);
        return false;
      }

      if (landmarkIndex < 0) {
        spdlog::error(
          "Invalid negative landmark index ({}) on line {} of landmarks CSV file {}",
          landmarkIndex,
          lineNum,
          csvFileName);
        return false;
      }

      const auto r = landmarks.try_emplace(static_cast<uint32_t>(landmarkIndex), landmarkPos, *landmarkName);
      if (!r.second) {
        spdlog::warn("Unable to insert landmark '{}', because index {} is already used", *landmarkName, landmarkIndex);
      }

      ++lineNum;
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

    for (const auto& lm : landmarks) {
      const auto id = lm.first;
      const auto pos = lm.second.getPosition();
      const auto name = lm.second.getName();
      outFile << id << "," << pos.x << "," << pos.y << "," << pos.z << "," << name << "\n";
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
  std::ofstream outFile;
  outFile.exceptions(outFile.exceptions() | std::ofstream::badbit | std::ofstream::failbit);

  try {
    outFile.open(jsonFileName, std::ofstream::out);

    if (!outFile) {
      throw std::system_error(
        errno,
        std::system_category(),
        "Failed to open output JSON file " + jsonFileName.string());
    }

    outFile << j.dump(2);

    spdlog::debug("Saved to JSON file {}:\n{}", jsonFileName, j.dump(2));
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
