#include "common/InputParams.h"

std::ostream& operator<<(std::ostream& os, const InputParams& p)
{
  for (size_t i = 0; i < p.imageFiles.size(); ++i) {
    os << "Image[" << i << "]: " << p.imageFiles[i].image;

    for (size_t j = 0; j < p.imageFiles[i].segmentations.size(); ++j) {
      os << "\nSegmentation[" << i << "][" << j << "]: " << p.imageFiles[i].segmentations[j];
    }

    os << "\n";
  }

  if (p.projectFile) {
    os << "\nProject file: " << *p.projectFile;
  }

  os << "\nConsole log level: " << p.consoleLogLevel;

  return os;
}
