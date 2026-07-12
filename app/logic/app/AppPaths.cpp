#include "logic/app/AppPaths.h"

#include <cstdlib>
#include <filesystem>
#include <string>
#include <vector>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#include <ShlObj_core.h>
#endif

#if defined(__linux__)
#include <unistd.h>
#endif

namespace
{

namespace fs = std::filesystem;

#ifdef _WIN32
fs::path knownFolderPath(const KNOWNFOLDERID& folderId)
{
  PWSTR path = nullptr;
  if (SUCCEEDED(SHGetKnownFolderPath(folderId, KF_FLAG_DEFAULT, nullptr, &path)) && path != nullptr) {
    fs::path result{path};
    CoTaskMemFree(path);
    return result;
  }

  return {};
}

fs::path windowsEnvironmentPath(const char* name)
{
  char* appData = nullptr;
  std::size_t appDataLength = 0;
  if (_dupenv_s(&appData, &appDataLength, name) == 0 && appData != nullptr) {
    fs::path result{appData};
    std::free(appData);
    return result;
  }

  return {};
}

fs::path windowsConfigRoot()
{
  if (const fs::path roamingAppData = knownFolderPath(FOLDERID_RoamingAppData); !roamingAppData.empty()) {
    return roamingAppData / "Entropy";
  }

  if (const fs::path appData = windowsEnvironmentPath("APPDATA"); !appData.empty()) {
    return appData / "Entropy";
  }

  return "Entropy";
}

fs::path windowsCacheRoot()
{
  if (const fs::path localAppData = knownFolderPath(FOLDERID_LocalAppData); !localAppData.empty()) {
    return localAppData / "Entropy" / "Cache";
  }

  if (const fs::path localAppData = windowsEnvironmentPath("LOCALAPPDATA"); !localAppData.empty()) {
    return localAppData / "Entropy" / "Cache";
  }

  return windowsConfigRoot() / "Cache";
}

fs::path executableDirectory()
{
  std::vector<wchar_t> buffer(MAX_PATH);

  while (true) {
    const DWORD length = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
    if (length == 0) {
      return {};
    }

    if (length < buffer.size() - 1) {
      return fs::path{buffer.data()}.parent_path();
    }

    buffer.resize(buffer.size() * 2);
  }
}
#endif

#if defined(__linux__)
fs::path environmentPath(const char* name)
{
  if (const char* value = std::getenv(name); value != nullptr && *value != '\0') {
    return fs::path{value};
  }

  return {};
}

fs::path xdgPathOrDefault(const char* environmentVariable, const fs::path& defaultRelativePath)
{
  if (const fs::path environmentValue = environmentPath(environmentVariable);
      !environmentValue.empty() && environmentValue.is_absolute())
  {
    return environmentValue;
  }

  if (const char* home = std::getenv("HOME"); home != nullptr && *home != '\0') {
    return fs::path{home} / defaultRelativePath;
  }

  return ".";
}

fs::path linuxConfigRoot()
{
  return xdgPathOrDefault("XDG_CONFIG_HOME", fs::path{".config"});
}

fs::path linuxStateRoot()
{
  return xdgPathOrDefault("XDG_STATE_HOME", fs::path{".local"} / "state");
}

fs::path linuxCacheRoot()
{
  return xdgPathOrDefault("XDG_CACHE_HOME", fs::path{".cache"});
}

fs::path executableDirectory()
{
  std::vector<char> buffer(1024);

  while (true) {
    const ssize_t length = readlink("/proc/self/exe", buffer.data(), buffer.size());
    if (length < 0) {
      return {};
    }

    if (static_cast<std::size_t>(length) < buffer.size()) {
      return fs::path{std::string{buffer.data(), static_cast<std::size_t>(length)}}.parent_path();
    }

    buffer.resize(buffer.size() * 2);
  }
}
#endif

} // namespace

namespace app_paths
{

void configureFromCommandLine(int, char*[]) {}

bool isRunningFromMacOSAppBundle()
{
  return false;
}

bool usesPlatformUserDirectories()
{
#if defined(_WIN32) || defined(__linux__)
  return true;
#else
  return false;
#endif
}

std::filesystem::path resourceDirectory()
{
#ifdef _WIN32
  if (const fs::path exeDir = executableDirectory(); !exeDir.empty()) {
    return exeDir / "share" / "entropy";
  }
#endif

#if defined(__linux__)
  if (const fs::path exeDir = executableDirectory(); !exeDir.empty()) {
    return (exeDir / ".." / "share" / "entropy").lexically_normal();
  }
#endif

#ifdef ENTROPY_RESOURCE_DIR
  return ENTROPY_RESOURCE_DIR;
#else
  return ".";
#endif
}

std::filesystem::path logDirectory()
{
#ifdef _WIN32
  return windowsConfigRoot() / "Logs";
#elif defined(__linux__)
  return linuxStateRoot() / "entropy" / "logs";
#else
  return "log";
#endif
}

std::filesystem::path cacheDirectory()
{
#ifdef _WIN32
  return windowsCacheRoot();
#elif defined(__linux__)
  return linuxCacheRoot() / "entropy";
#else
  return "cache";
#endif
}

std::filesystem::path userDataDirectory()
{
#ifdef _WIN32
  return windowsConfigRoot();
#elif defined(__linux__)
  return linuxConfigRoot() / "entropy";
#else
  return ".";
#endif
}

std::filesystem::path userSettingsFile()
{
  return userDataDirectory() / "settings.json";
}

} // namespace app_paths
