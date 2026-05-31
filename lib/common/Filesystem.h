#pragma once

#include <filesystem>
namespace fs = std::filesystem;

#include <spdlog/fmt/ostr.h>
#if FMT_VERSION >= 90000
template<>
struct fmt::formatter<fs::path> : ostream_formatter
{
};
#endif
