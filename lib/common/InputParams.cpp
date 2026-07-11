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

  for (size_t i = 0; i < p.dicomPaths.size(); ++i) {
    os << "DICOM[" << i << "]: " << p.dicomPaths[i] << "\n";
  }

  if (p.projectFile) {
    os << "\nProject file: " << *p.projectFile;
  }
  if (p.layoutsFile) {
    os << "\nLayouts file: " << *p.layoutsFile;
  }

  os << "\nConsole log level: " << p.consoleLogLevel;

  return os;
}
