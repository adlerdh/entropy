#include "logic/app/AppPaths.h"

#include <catch2/catch_test_macros.hpp>

TEST_CASE("user settings file is stored under the platform user data directory", "[app][settings][paths]")
{
  const std::filesystem::path userDataDirectory = app_paths::userDataDirectory();
  const std::filesystem::path settingsFile = app_paths::userSettingsFile();

  CHECK(settingsFile.filename() == "settings.json");
  CHECK(settingsFile.parent_path() == userDataDirectory);

#ifdef __APPLE__
  CHECK(app_paths::usesPlatformUserDirectories());
  CHECK(settingsFile.string().find("Application Support") != std::string::npos);
  CHECK(settingsFile.string().find("Entropy") != std::string::npos);
  CHECK(settingsFile.is_absolute());
#elif defined(_WIN32)
  CHECK(app_paths::usesPlatformUserDirectories());
  CHECK(settingsFile.string().find("Entropy") != std::string::npos);
  CHECK(settingsFile.is_absolute() || settingsFile.parent_path() == std::filesystem::path{"Entropy"});
#elif defined(__linux__)
  CHECK_FALSE(app_paths::usesPlatformUserDirectories());
  CHECK(settingsFile.string().find("entropy") != std::string::npos);
#endif
}
