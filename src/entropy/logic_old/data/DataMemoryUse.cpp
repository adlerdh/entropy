#include "logic/data/DataMemoryUse.h"

namespace data
{

CpuAndGpuMemoryUse memoryUse(const ImageColorMapRecord&)
{
  return std::make_pair(0, 0);
}

CpuAndGpuMemoryUse memoryUse(const ImageRecord&)
{
  return std::make_pair(0, 0);
}

CpuAndGpuMemoryUse memoryUse(const LabelTableRecord&)
{
  return std::make_pair(0, 0);
}

CpuAndGpuMemoryUse memoryUse(const MeshRecord&)
{
  return std::make_pair(0, 0);
}

CpuAndGpuMemoryUse memoryUse(const ParcellationRecord&)
{
  return std::make_pair(0, 0);
}

CpuAndGpuMemoryUse memoryUse(const SlideRecord&)
{
  return std::make_pair(0, 0);
}

CpuAndGpuMemoryUse imageColorMapsMemoryUse(DataManager&)
{
  return std::make_pair(0, 0);
}

CpuAndGpuMemoryUse imagesMemoryUse(DataManager&)
{
  return std::make_pair(0, 0);
}

CpuAndGpuMemoryUse labelTablesMemoryUse(DataManager&)
{
  return std::make_pair(0, 0);
}

CpuAndGpuMemoryUse parcellationsMemoryUse(DataManager&)
{
  return std::make_pair(0, 0);
}

CpuAndGpuMemoryUse slidesMemoryUse(DataManager&)
{
  return std::make_pair(0, 0);
}

} // namespace data
