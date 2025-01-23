#ifndef DATA_MEMORY_USAGE_HELPER_H
#define DATA_MEMORY_USAGE_HELPER_H

#include "logic/records/ImageColorMapRecord.h"
#include "logic/records/ImageRecord.h"
#include "logic/records/LabelTableRecord.h"
#include "logic/records/MeshRecord.h"
#include "logic/records/ParcellationRecord.h"
#include "logic/records/SlideRecord.h"

#include <utility>

class DataManager;

namespace data
{

/// CPU and GPU memory use, measured in bytes
using CpuAndGpuMemoryUse = std::pair<size_t, size_t>;

CpuAndGpuMemoryUse memoryUse(const ImageColorMapRecord&);
CpuAndGpuMemoryUse memoryUse(const ImageRecord&);
CpuAndGpuMemoryUse memoryUse(const LabelTableRecord&);
CpuAndGpuMemoryUse memoryUse(const MeshRecord&);
CpuAndGpuMemoryUse memoryUse(const ParcellationRecord&);
CpuAndGpuMemoryUse memoryUse(const SlideRecord&);

CpuAndGpuMemoryUse imageColorMapsMemoryUse(DataManager&);
CpuAndGpuMemoryUse imagesMemoryUse(DataManager&);
CpuAndGpuMemoryUse labelTablesMemoryUse(DataManager&);
CpuAndGpuMemoryUse parcellationsMemoryUse(DataManager&);
CpuAndGpuMemoryUse slidesMemoryUse(DataManager&);

} // namespace data

#endif // DATA_MEMORY_USAGE_HELPER_H
