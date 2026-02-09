#include "common/InputParams.h"

std::ostream& operator<<(std::ostream& os, const InputParams& p)
{
  for (size_t i = 0; i < p.imageFiles.size(); ++i)
  {
    os << "Image[" << i << "]: " << p.imageFiles[i].image;

    if (p.imageFiles[i].seg) {
      os << "\nSegmentation[" << i << "]: " << *p.imageFiles[i].seg;
    }
    os << "\n";
  }

  if (p.projectFile) {
    os << "\nProject file: " << *p.projectFile;
  }

  os << "\nConsole log level: " << p.consoleLogLevel;

  return os;
}
