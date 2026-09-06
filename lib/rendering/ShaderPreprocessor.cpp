#include "rendering/ShaderPreprocessor.h"

#include <cctype>
#include <stdexcept>
#include <string>
#include <string_view>

namespace rendering
{
namespace
{

constexpr std::string_view k_includePrefix{"#include \"entropy/"};
constexpr std::string_view k_includeSuffix{".glsl\""};
constexpr std::size_t k_maxIncludeDepth = 32;

bool isTokenCharacter(const char value)
{
  return std::isupper(static_cast<unsigned char>(value)) != 0 || std::isdigit(static_cast<unsigned char>(value)) != 0 ||
         value == '_';
}

std::string_view trim(std::string_view value)
{
  while (!value.empty() && (value.front() == ' ' || value.front() == '\t' || value.front() == '\r')) {
    value.remove_prefix(1);
  }
  while (!value.empty() && (value.back() == ' ' || value.back() == '\t' || value.back() == '\r')) {
    value.remove_suffix(1);
  }
  return value;
}

std::string includeToken(std::string_view line)
{
  const std::string_view stripped = trim(line);
  if (!stripped.starts_with("#include")) {
    return {};
  }
  if (!stripped.starts_with(k_includePrefix) || !stripped.ends_with(k_includeSuffix)) {
    throw std::runtime_error("Malformed shader include. Expected #include \"entropy/TOKEN.glsl\" on a line by itself");
  }

  const std::string_view token =
    stripped.substr(k_includePrefix.size(), stripped.size() - k_includePrefix.size() - k_includeSuffix.size());
  if (token.empty()) {
    throw std::runtime_error("Shader include token must not be empty");
  }
  for (const char character : token) {
    if (!isTokenCharacter(character)) {
      throw std::runtime_error("Invalid shader include token '" + std::string(token) + "'");
    }
  }
  return std::string(token);
}

std::string preprocess(std::string_view source, const ShaderReplacements& replacements, const std::size_t depth)
{
  if (depth > k_maxIncludeDepth) {
    throw std::runtime_error("Shader includes exceed the maximum nesting depth (possible include cycle)");
  }
  if (source.find("$$") != std::string_view::npos) {
    throw std::runtime_error("Shader source contains retired $$TOKEN$$ substitution syntax");
  }

  std::string withIncludes;
  std::size_t lineStart = 0;
  while (lineStart < source.size()) {
    const std::size_t newline = source.find('\n', lineStart);
    const bool hasNewline = newline != std::string_view::npos;
    const std::size_t lineEnd = hasNewline ? newline : source.size();
    const std::string_view line = source.substr(lineStart, lineEnd - lineStart);
    const std::string token = includeToken(line);

    if (token.empty()) {
      withIncludes.append(line);
    }
    else {
      const auto replacement = replacements.find(token);
      if (replacement == replacements.end()) {
        throw std::runtime_error("Missing shader include replacement for '" + token + "'");
      }
      withIncludes += preprocess(replacement->second, replacements, depth + 1);
    }

    if (hasNewline) {
      withIncludes.push_back('\n');
      lineStart = newline + 1;
    }
    else {
      lineStart = source.size();
    }
  }

  std::string result;
  std::size_t cursor = 0;
  while (cursor < withIncludes.size()) {
    const std::size_t marker = withIncludes.find("${", cursor);
    if (marker == std::string::npos) {
      result.append(withIncludes, cursor, std::string::npos);
      break;
    }
    result.append(withIncludes, cursor, marker - cursor);
    const std::size_t close = withIncludes.find('}', marker + 2);
    if (close == std::string::npos) {
      throw std::runtime_error("Unterminated shader substitution starting at byte " + std::to_string(marker));
    }

    const std::string token = withIncludes.substr(marker + 2, close - marker - 2);
    if (token.empty()) {
      throw std::runtime_error("Shader substitution token must not be empty");
    }
    for (const char character : token) {
      if (!isTokenCharacter(character)) {
        throw std::runtime_error("Invalid shader substitution token '" + token + "'");
      }
    }

    const auto replacement = replacements.find(token);
    if (replacement == replacements.end()) {
      throw std::runtime_error("Missing shader substitution replacement for '" + token + "'");
    }
    result += replacement->second;
    cursor = close + 1;
  }

  return result;
}

} // namespace

std::string preprocessShaderSource(const std::string_view source, const ShaderReplacements& replacements)
{
  return preprocess(source, replacements, 0);
}

} // namespace rendering
