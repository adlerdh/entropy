#include "logic/app/AppPaths.h"

#include <filesystem>

namespace app_paths
{

void configureFromCommandLine(int, char*[]) {}

bool isRunningFromMacOSAppBundle()
{
  return false;
}

bool usesPlatformUserDirectories()
{
  return false;
}

std::filesystem::path logDirectory()
{
  return "log";
}

std::filesystem::path userDataDirectory()
{
  return ".";
}

} // namespace app_paths
