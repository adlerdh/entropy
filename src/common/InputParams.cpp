#include "common/InputParams.h"

std::ostream& operator<<(std::ostream& os, const InputParams& p)
{
  for (size_t i = 0; i < p.imageFiles.size(); ++i)
  {
    os << "Image " << i << ": " << p.imageFiles[i].first;

    if (p.imageFiles[i].second)
    {
      os << "\nSegmentation " << i << ": " << *p.imageFiles[i].second;
    }
    os << "\n";
  }

  if (p.projectFile)
    os << "\nProject file: " << *p.projectFile;
  os << "\nConsole log level: " << p.consoleLogLevel;

  return os;
}
