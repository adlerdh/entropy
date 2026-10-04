#include "deformation/DeformationFieldIO.h"

#include <bit>
#include <array>
#include <cmath>
#include <cstdint>
#include <glm/geometric.hpp>
#include <fstream>
#include <iomanip>
#include <limits>
#include <locale>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>

namespace deformation
{
namespace
{
constexpr std::uint32_t crcPolynomial = 0xedb88320u;
constexpr std::size_t maxHeaderBytes = 64 * 1024;

bool readHeaderLine(std::istream& input, std::string& line, std::size_t& bytes)
{
  line.clear();
  while (true) {
    const int next = input.get();
    if (next == std::char_traits<char>::eof()) return false;
    if (++bytes > maxHeaderBytes) throw std::runtime_error("Native field header is too large");
    if (next == '\n') return true;
    line.push_back(static_cast<char>(next));
  }
}

std::uint32_t updateCrc(std::uint32_t crc, const unsigned char byte)
{
  crc ^= byte;
  for (int bit = 0; bit < 8; ++bit)
    crc = (crc >> 1) ^ ((crc & 1u) ? crcPolynomial : 0u);
  return crc;
}

std::uint32_t payloadCrc(const std::vector<glm::vec4>& samples)
{
  std::uint32_t crc = ~std::uint32_t{0};
  for (const auto& value : samples) {
    for (int component = 0; component < 4; ++component) {
      const auto bits = std::bit_cast<std::uint32_t>(value[component]);
      for (int byte = 0; byte < 4; ++byte)
        crc = updateCrc(crc, static_cast<unsigned char>(bits >> (8 * byte)));
    }
  }
  return ~crc;
}

std::string required(const std::map<std::string, std::string>& fields, const std::string& key)
{
  const auto it = fields.find(key);
  if (it == fields.end()) throw std::runtime_error("Native field is missing " + key);
  return it->second;
}

template<typename T, std::size_t N>
std::array<T, N> numbers(const std::string& source)
{
  std::istringstream input(source);
  input.imbue(std::locale::classic());
  std::array<T, N> values{};
  for (auto& value : values) {
    if (!(input >> value)) throw std::runtime_error("Invalid native field numeric metadata");
  }
  std::string extra;
  if (input >> extra) throw std::runtime_error("Extra native field numeric metadata");
  return values;
}

std::string roleName(const MapDirection role)
{
  switch (role) {
    case MapDirection::Forward:
      return "forward";
    case MapDirection::Inverse:
      return "inverse";
  }
  throw std::invalid_argument("Unknown field role");
}

std::string directions(const FieldDomain& domain)
{
  std::ostringstream output;
  output.imbue(std::locale::classic());
  output << std::setprecision(std::numeric_limits<double>::max_digits10);
  for (int axis = 0; axis < 3; ++axis) {
    for (int row = 0; row < 3; ++row)
      output << (axis || row ? " " : "") << domain.directions()[axis][row];
  }
  return output.str();
}

glm::dvec3 parseNrrdTuple(const std::string& token)
{
  if (token.size() < 5 || token.front() != '(' || token.back() != ')')
    throw std::runtime_error("Invalid NRRD physical vector");
  std::string coordinates = token.substr(1, token.size() - 2);
  for (char& character : coordinates)
    if (character == ',') character = ' ';
  const auto values = numbers<double, 3>(coordinates);
  return {values[0], values[1], values[2]};
}

void writeAxis(std::ostream& out, const glm::dvec3& axis)
{
  out << '(' << axis.x << ',' << axis.y << ',' << axis.z << ')';
}
} // namespace

void validateFieldForExport(const FieldAsset& asset)
{
  if (asset.samples.size() != asset.domain.sampleCount())
    throw std::invalid_argument("Export field sample count mismatch");
  if (asset.role != MapDirection::Forward && asset.role != MapDirection::Inverse)
    throw std::invalid_argument("Export field role is invalid");
  for (const auto& value : asset.samples) {
    if (value.w != 0.0f && value.w != 1.0f) throw std::invalid_argument("Export field validity must be zero or one");
    if (value.w == 0.0f) continue;
    if (!std::isfinite(value.x) || !std::isfinite(value.y) || !std::isfinite(value.z))
      throw std::invalid_argument("Export field contains a non-finite usable vector");
    if (
      asset.domain.dimension() == SpatialDimension::Plane &&
      std::abs(glm::dot(asset.domain.directions()[2], glm::dvec3(value))) > FieldDomain::planeToleranceMm)
      throw std::invalid_argument("Export field contains an off-plane usable vector");
  }
}

std::uint32_t nativeFieldPayloadChecksum(const std::vector<glm::vec4>& samples) noexcept
{
  return payloadCrc(samples);
}

void writeNativeField(const std::filesystem::path& path, const FieldAsset& asset)
{
  if (asset.samples.size() != asset.domain.sampleCount())
    throw std::invalid_argument("Field sample count disagrees with domain");
  const auto role = roleName(asset.role);
  std::ofstream out(path, std::ios::binary | std::ios::trunc);
  if (!out) throw std::runtime_error("Cannot create native field");
  out.imbue(std::locale::classic());
  out << std::setprecision(std::numeric_limits<double>::max_digits10);
  const auto& domain = asset.domain;
  const bool volume = domain.dimension() == SpatialDimension::Volume;
  out << "NRRD0005\n"
      << "type: float\n"
      << "dimension: " << (volume ? 4 : 3) << "\n"
      << "sizes: 4 " << domain.size()[0] << ' ' << domain.size()[1];
  if (volume) out << ' ' << domain.size()[2];
  out << "\nkinds: vector domain domain";
  if (volume) out << " domain";
  out << "\nendian: little\nencoding: raw\nspace: left-posterior-superior\nspace units: \"mm\" \"mm\" \"mm\"\n";
  out << "space origin: ";
  writeAxis(out, domain.origin());
  out << "\nspace directions: none ";
  writeAxis(out, domain.directions()[0] * domain.spacing().x);
  out << ' ';
  writeAxis(out, domain.directions()[1] * domain.spacing().y);
  if (volume) {
    out << ' ';
    writeAxis(out, domain.directions()[2] * domain.spacing().z);
  }
  out << "\nentropy_field_version:=1\nentropy_dimension:=" << (volume ? 3 : 2) << "\nentropy_role:=" << role
      << "\nentropy_vector_basis:=LPS-mm\nentropy_components:=LPS-x LPS-y LPS-z validity" << "\nentropy_valid_extent:=";
  for (int axis = 0; axis < 3; ++axis)
    out << (axis ? " " : "") << domain.validExtent().begin[axis];
  for (int axis = 0; axis < 3; ++axis)
    out << ' ' << domain.validExtent().end[axis];
  out << "\nentropy_origin:=" << domain.origin().x << ' ' << domain.origin().y << ' ' << domain.origin().z;
  out << "\nentropy_spacing:=" << domain.spacing().x << ' ' << domain.spacing().y << ' ' << domain.spacing().z;
  out << "\nentropy_directions:=" << directions(domain);
  out << "\nentropy_crc32:=" << std::hex << std::setw(8) << std::setfill('0') << payloadCrc(asset.samples) << std::dec
      << "\n\n";
  for (const auto& value : asset.samples) {
    for (int component = 0; component < 4; ++component) {
      const auto bits = std::bit_cast<std::uint32_t>(value[component]);
      for (int byte = 0; byte < 4; ++byte)
        out.put(static_cast<char>(bits >> (8 * byte)));
    }
  }
  out.flush();
  if (!out) throw std::runtime_error("Cannot finish native field");
}

FieldAsset readNativeField(const std::filesystem::path& path, const std::size_t maxBytes)
{
  std::ifstream in(path, std::ios::binary);
  if (!in) throw std::runtime_error("Cannot open native field");
  std::string line;
  std::size_t headerBytes = 0;
  std::map<std::string, std::string> fields;
  if (!readHeaderLine(in, line, headerBytes) || line != "NRRD0005")
    throw std::runtime_error("Unsupported native field header");
  bool ended = false;
  while (readHeaderLine(in, line, headerBytes)) {
    if (line.empty()) {
      ended = true;
      break;
    }
    if (line.starts_with('#')) continue;
    const auto delimiter = line.find(":=");
    const auto ordinary = line.find(':');
    if (ordinary == std::string::npos) throw std::runtime_error("Malformed native field header");
    const bool custom = delimiter != std::string::npos && delimiter == ordinary;
    const auto key = line.substr(0, ordinary);
    const auto value = line.substr(ordinary + (custom ? 2 : 1));
    if (!fields.emplace(key, value.starts_with(' ') ? value.substr(1) : value).second)
      throw std::runtime_error("Duplicate native field header key");
  }
  if (
    !ended || required(fields, "type") != "float" || required(fields, "encoding") != "raw" ||
    required(fields, "endian") != "little" || required(fields, "space") != "left-posterior-superior" ||
    required(fields, "space units") != "\"mm\" \"mm\" \"mm\"" || required(fields, "entropy_field_version") != "1" ||
    required(fields, "entropy_vector_basis") != "LPS-mm" ||
    required(fields, "entropy_components") != "LPS-x LPS-y LPS-z validity")
  {
    throw std::runtime_error("Unsupported native field profile");
  }
  const auto nativeDimension = numbers<unsigned, 1>(required(fields, "entropy_dimension"))[0];
  const bool volume = nativeDimension == 3;
  if (nativeDimension != 2 && !volume) throw std::runtime_error("Invalid native field dimension");
  const auto storedDimension = numbers<unsigned, 1>(required(fields, "dimension"))[0];
  if (storedDimension != (volume ? 4u : 3u)) throw std::runtime_error("Native field axes disagree with dimension");
  if (
    required(fields, "kinds") != (volume ? "vector domain domain domain" : "vector domain domain") ||
    required(fields, "space origin").empty() || required(fields, "space directions").empty())
    throw std::runtime_error("Invalid native field NRRD geometry");
  const auto roleText = required(fields, "entropy_role");
  const auto role = roleText == "forward" ? MapDirection::Forward : MapDirection::Inverse;
  if (roleText != "forward" && roleText != "inverse") throw std::runtime_error("Invalid native field role");
  const auto sizes = volume ? numbers<std::uint32_t, 4>(required(fields, "sizes")) : std::array<std::uint32_t, 4>{};
  const auto planeSizes =
    volume ? std::array<std::uint32_t, 3>{} : numbers<std::uint32_t, 3>(required(fields, "sizes"));
  if ((volume && sizes[0] != 4) || (!volume && planeSizes[0] != 4))
    throw std::runtime_error("Native field must have four components");
  DomainGeometry geometry;
  geometry.dimension = volume ? SpatialDimension::Volume : SpatialDimension::Plane;
  geometry.size = volume ? GridSize{sizes[1], sizes[2], sizes[3]} : GridSize{planeSizes[1], planeSizes[2], 1};
  const auto origin = numbers<double, 3>(required(fields, "entropy_origin"));
  const auto spacing = numbers<double, 3>(required(fields, "entropy_spacing"));
  const auto frame = numbers<double, 9>(required(fields, "entropy_directions"));
  const auto extent = numbers<std::uint32_t, 6>(required(fields, "entropy_valid_extent"));
  geometry.origin = {origin[0], origin[1], origin[2]};
  geometry.spacing = {spacing[0], spacing[1], spacing[2]};
  for (int axis = 0; axis < 3; ++axis)
    for (int row = 0; row < 3; ++row)
      geometry.directions[axis][row] = frame[axis * 3 + row];
  geometry.validExtent = IndexExtent{{extent[0], extent[1], extent[2]}, {extent[3], extent[4], extent[5]}};
  FieldDomain domain(geometry);
  if (parseNrrdTuple(required(fields, "space origin")) != domain.origin())
    throw std::runtime_error("NRRD origin disagrees with native field metadata");
  std::istringstream standardDirections(required(fields, "space directions"));
  std::string token;
  if (!(standardDirections >> token) || token != "none")
    throw std::runtime_error("NRRD vector axis must have no spatial direction");
  for (int axis = 0; axis < (volume ? 3 : 2); ++axis) {
    if (!(standardDirections >> token) || parseNrrdTuple(token) != domain.directions()[axis] * domain.spacing()[axis])
      throw std::runtime_error("NRRD directions disagree with native field metadata");
  }
  if (standardDirections >> token) throw std::runtime_error("Extra NRRD field axis");
  if (domain.sampleCount() > maxBytes / 16) throw std::runtime_error("Native field exceeds byte limit");
  const auto expectedCrcText = required(fields, "entropy_crc32");
  if (expectedCrcText.size() != 8) throw std::runtime_error("Invalid native field checksum");
  std::uint32_t expectedCrc = 0;
  for (const char digit : expectedCrcText) {
    expectedCrc <<= 4;
    if (digit >= '0' && digit <= '9')
      expectedCrc |= static_cast<std::uint32_t>(digit - '0');
    else if (digit >= 'a' && digit <= 'f')
      expectedCrc |= static_cast<std::uint32_t>(digit - 'a' + 10);
    else
      throw std::runtime_error("Invalid native field checksum");
  }
  std::vector<glm::vec4> samples(domain.sampleCount());
  std::uint32_t crc = ~std::uint32_t{0};
  for (auto& value : samples) {
    for (int component = 0; component < 4; ++component) {
      std::uint32_t bits = 0;
      for (int byte = 0; byte < 4; ++byte) {
        const auto next = in.get();
        if (next == std::char_traits<char>::eof()) throw std::runtime_error("Truncated native field payload");
        const auto octet = static_cast<unsigned char>(next);
        bits |= static_cast<std::uint32_t>(octet) << (8 * byte);
        crc = updateCrc(crc, octet);
      }
      value[component] = std::bit_cast<float>(bits);
    }
  }
  if (~crc != expectedCrc || in.get() != std::char_traits<char>::eof())
    throw std::runtime_error("Native field checksum or length mismatch");
  return {std::move(domain), role, std::move(samples)};
}
} // namespace deformation
