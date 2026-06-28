#include "registration/Progress.h"

#include "registration/Json.h"

#include <nlohmann/json.hpp>

#include <algorithm>

namespace registration
{
namespace
{

bool isBlank(std::string_view value)
{
  return std::all_of(value.begin(), value.end(), [](const char ch) {
    return ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r';
  });
}

} // namespace

std::optional<ProgressEvent> parseProgressEventLine(std::string_view line, std::string* error)
{
  if (error) {
    error->clear();
  }
  if (isBlank(line)) {
    return std::nullopt;
  }

  try {
    const nlohmann::json json = nlohmann::json::parse(line);
    if (!json.is_object() || !json.contains("event")) {
      if (error) {
        *error = "Progress line is not a JSON object with an event field";
      }
      return std::nullopt;
    }
    return json.get<ProgressEvent>();
  }
  catch (const std::exception& e) {
    if (error) {
      *error = e.what();
    }
    return std::nullopt;
  }
}

std::string progressEventLine(const ProgressEvent& event)
{
  const nlohmann::json json = event;
  return json.dump();
}

} // namespace registration
