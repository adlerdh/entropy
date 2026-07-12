#include "logic/app/AppPaths.h"

#include <catch2/catch_test_macros.hpp>

TEST_CASE("user settings file is stored under the platform user data directory", "[app][settings][paths]")
{
  const std::filesystem::path userDataDirectory = app_paths::userDataDirectory();
  const std::filesystem::path logDirectory = app_paths::logDirectory();
  const std::filesystem::path cacheDirectory = app_paths::cacheDirectory();
  const std::filesystem::path settingsFile = app_paths::userSettingsFile();

  CHECK(settingsFile.filename() == "settings.json");
  CHECK(settingsFile.parent_path() == userDataDirectory);
  CHECK_FALSE(logDirectory.empty());
  CHECK_FALSE(cacheDirectory.empty());

#ifdef __APPLE__
  CHECK(app_paths::usesPlatformUserDirectories());
  CHECK(settingsFile.string().find("Application Support") != std::string::npos);
  CHECK(settingsFile.string().find("Entropy") != std::string::npos);
  CHECK(logDirectory.string().find("Logs") != std::string::npos);
  CHECK(logDirectory.string().find("Entropy") != std::string::npos);
  CHECK(cacheDirectory.string().find("Caches") != std::string::npos);
  CHECK(cacheDirectory.string().find("Entropy") != std::string::npos);
  CHECK(settingsFile.is_absolute());
#elif defined(_WIN32)
  CHECK(app_paths::usesPlatformUserDirectories());
  CHECK(settingsFile.string().find("Entropy") != std::string::npos);
  CHECK(logDirectory.string().find("Entropy") != std::string::npos);
  CHECK(logDirectory.string().find("Logs") != std::string::npos);
  CHECK(cacheDirectory.string().find("Entropy") != std::string::npos);
  CHECK(cacheDirectory.string().find("Cache") != std::string::npos);
  const bool hasExpectedWindowsPath =
    settingsFile.is_absolute() || settingsFile.parent_path() == std::filesystem::path{"Entropy"};
  CHECK(hasExpectedWindowsPath);
#elif defined(__linux__)
  CHECK(app_paths::usesPlatformUserDirectories());
  CHECK(settingsFile.string().find("entropy") != std::string::npos);
  CHECK(logDirectory.string().find("entropy") != std::string::npos);
  CHECK(logDirectory.string().find("logs") != std::string::npos);
  CHECK(cacheDirectory.string().find("entropy") != std::string::npos);
#endif
}
