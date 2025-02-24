#pragma once

// On Apple platforms, we must use the alternative ghc::filesystem,
// because it is not fully implemented or supported prior to macOS 10.15.
#if !defined(__APPLE__)
#if defined(__cplusplus) && __cplusplus >= 201703L && defined(__has_include)
#if __has_include(<filesystem>)
#define GHC_USE_STD_FS
#include <filesystem>
namespace fs = std::filesystem;
#endif
#endif
#endif

#ifndef GHC_USE_STD_FS
#include <ghc/filesystem.hpp>
namespace fs = ghc::filesystem;
#endif

#include <spdlog/fmt/ostr.h>
#if FMT_VERSION >= 90000
template<>
struct fmt::formatter<fs::path> : ostream_formatter
{
};
#endif
