#include "registration/AffineTransformIO.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <fstream>
#include <iterator>
#include <sstream>
#include <string>
#include <vector>

namespace registration
{
namespace
{

std::string trim(std::string value)
{
  const auto first = std::find_if_not(value.begin(), value.end(), [](unsigned char ch) { return std::isspace(ch); });
  const auto last =
    std::find_if_not(value.rbegin(), value.rend(), [](unsigned char ch) { return std::isspace(ch); }).base();
  if (first >= last) {
    return {};
  }
  return {first, last};
}

std::vector<double> parseNumbers(std::string_view text)
{
  std::istringstream stream{std::string{text}};
  return {std::istream_iterator<double>(stream), std::istream_iterator<double>()};
}

std::optional<glm::dmat4> readPlainMatrix(const std::vector<std::string>& lines)
{
  std::vector<std::vector<double>> rows;
  for (const std::string& line : lines) {
    const std::string trimmed = trim(line);
    if (trimmed.empty() || trimmed.starts_with('#')) {
      continue;
    }
    std::vector<double> row = parseNumbers(trimmed);
    if (row.empty()) {
      continue;
    }
    if (row.size() != 4) {
      return std::nullopt;
    }
    rows.push_back(std::move(row));
  }

  if (rows.size() != 4) {
    return std::nullopt;
  }

  glm::dmat4 matrix{1.0};
  for (int row = 0; row < 4; ++row) {
    for (int column = 0; column < 4; ++column) {
      matrix[column][row] = rows[static_cast<std::size_t>(row)][static_cast<std::size_t>(column)];
    }
  }
  return matrix;
}

glm::dmat4 rasLpsFlip()
{
  glm::dmat4 flip{1.0};
  flip[0][0] = -1.0;
  flip[1][1] = -1.0;
  return flip;
}

glm::dmat4 convertRasAffineToLps(const glm::dmat4& matrix)
{
  const glm::dmat4 flip = rasLpsFlip();
  return flip * matrix * flip;
}

bool writePlainMatrix(const std::filesystem::path& fileName, const glm::dmat4& matrix)
{
  std::ofstream stream{fileName};
  if (!stream) {
    return false;
  }

  for (int row = 0; row < 4; ++row) {
    for (int column = 0; column < 4; ++column) {
      stream << matrix[column][row];
      if (column < 3) {
        stream << ' ';
      }
    }
    stream << '\n';
  }
  return static_cast<bool>(stream);
}

std::optional<std::vector<double>> valuesAfterPrefix(const std::vector<std::string>& lines, std::string_view prefix)
{
  for (const std::string& line : lines) {
    const std::string trimmed = trim(line);
    if (!trimmed.starts_with(prefix)) {
      continue;
    }
    return parseNumbers(std::string_view{trimmed}.substr(prefix.size()));
  }
  return std::nullopt;
}

std::optional<glm::dmat4> readItkAffine(const std::vector<std::string>& lines)
{
  const std::optional<std::vector<double>> parameters = valuesAfterPrefix(lines, "Parameters:");
  if (!parameters || (parameters->size() != 6 && parameters->size() != 12)) {
    return std::nullopt;
  }

  const int dimension = parameters->size() == 6 ? 2 : 3;
  const std::optional<std::vector<double>> fixedParameters = valuesAfterPrefix(lines, "FixedParameters:");

  glm::dmat4 matrix{1.0};
  for (int row = 0; row < dimension; ++row) {
    for (int column = 0; column < dimension; ++column) {
      const auto index =
        static_cast<std::size_t>(row) * static_cast<std::size_t>(dimension) + static_cast<std::size_t>(column);
      matrix[column][row] = parameters->at(index);
    }
  }

  glm::dvec4 center{0.0, 0.0, 0.0, 1.0};
  if (fixedParameters && fixedParameters->size() >= static_cast<std::size_t>(dimension)) {
    for (int row = 0; row < dimension; ++row) {
      center[row] = fixedParameters->at(static_cast<std::size_t>(row));
    }
  }

  glm::dvec4 translation{0.0, 0.0, 0.0, 0.0};
  const std::size_t translationOffset = static_cast<std::size_t>(dimension) * static_cast<std::size_t>(dimension);
  for (int row = 0; row < dimension; ++row) {
    translation[row] = parameters->at(translationOffset + static_cast<std::size_t>(row));
  }

  const glm::dvec4 linearCenter = matrix * glm::dvec4{center.x, center.y, center.z, 0.0};
  const glm::dvec4 offset = translation + center - linearCenter;
  for (int row = 0; row < dimension; ++row) {
    matrix[3][row] = offset[row];
  }
  return matrix;
}

} // namespace

std::optional<glm::dmat4> readAffineTransform(const std::filesystem::path& fileName)
{
  std::ifstream stream{fileName};
  if (!stream) {
    return std::nullopt;
  }

  std::vector<std::string> lines;
  std::string line;
  while (std::getline(stream, line)) {
    lines.push_back(std::move(line));
  }
  if (lines.empty()) {
    return std::nullopt;
  }

  if (const std::optional<glm::dmat4> matrix = readPlainMatrix(lines)) {
    return matrix;
  }
  return readItkAffine(lines);
}

std::optional<glm::dmat4> readGreedyAffineTransform(const std::filesystem::path& fileName)
{
  const std::optional<glm::dmat4> rasMatrix = readAffineTransform(fileName);
  if (!rasMatrix) {
    return std::nullopt;
  }
  return convertRasAffineToLps(*rasMatrix);
}

bool writeItkAffineTransform(const std::filesystem::path& fileName, const glm::dmat4& matrix, int dimension)
{
  dimension = std::clamp(dimension, 2, 3);
  std::ofstream stream{fileName};
  if (!stream) {
    return false;
  }

  stream << "#Insight Transform File V1.0\n";
  stream << "#Transform 0\n";
  stream << "Transform: AffineTransform_double_" << dimension << '_' << dimension << '\n';
  stream << "Parameters:";
  for (int row = 0; row < dimension; ++row) {
    for (int column = 0; column < dimension; ++column) {
      stream << ' ' << matrix[column][row];
    }
  }
  for (int row = 0; row < dimension; ++row) {
    stream << ' ' << matrix[3][row];
  }
  stream << "\nFixedParameters:";
  for (int row = 0; row < dimension; ++row) {
    stream << " 0";
  }
  stream << '\n';
  return static_cast<bool>(stream);
}

bool writeGreedyAffineTransform(const std::filesystem::path& fileName, const glm::dmat4& matrix)
{
  return writePlainMatrix(fileName, convertRasAffineToLps(matrix));
}

} // namespace registration
