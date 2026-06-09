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

fs::path windowsUserDataRoot()
{
  if (const fs::path localAppData = knownFolderPath(FOLDERID_LocalAppData); !localAppData.empty()) {
    return localAppData / "Entropy";
  }

  char* localAppData = nullptr;
  std::size_t localAppDataLength = 0;
  if (_dupenv_s(&localAppData, &localAppDataLength, "LOCALAPPDATA") == 0 && localAppData != nullptr) {
    fs::path result{localAppData};
    std::free(localAppData);
    return result / "Entropy";
  }

  return "Entropy";
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
#ifdef _WIN32
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
  return windowsUserDataRoot() / "Logs";
#else
  return "log";
#endif
}

std::filesystem::path userDataDirectory()
{
#ifdef _WIN32
  return windowsUserDataRoot();
#else
  return ".";
#endif
}

} // namespace app_paths
