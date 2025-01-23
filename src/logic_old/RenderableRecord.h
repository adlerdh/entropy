#ifndef RENDERABLE_RECORD_H
#define RENDERABLE_RECORD_H

#include <uuid.h>

#include <memory>

/**
 * @brief Template class for a record of renderable data that contains both CPU and GPU data.
 */
template<class CpuData, class GpuData>
class RenderableRecord
{
public:
  RenderableRecord(std::unique_ptr<CpuData> cpuData, std::unique_ptr<GpuData> gpuData)
    : m_uid()
    , m_cpuData(std::move(cpuData))
    , m_gpuData(std::move(gpuData))
  {
  }

  RenderableRecord(const RenderableRecord&) = delete;
  RenderableRecord& operator=(const RenderableRecord&) = delete;

  RenderableRecord(RenderableRecord&&) = default;
  RenderableRecord& operator=(RenderableRecord&&) = default;

  ~RenderableRecord() = default;

  void setUid(uuids::uuid uid) { m_uid = std::move(uid); }

  void setCpuData(std::unique_ptr<CpuData> cpuData) { m_cpuData = std::move(cpuData); }

  void setGpuData(std::unique_ptr<GpuData> gpuData) { m_gpuData = std::move(gpuData); }

  const uuids::uuid& uid() const { return m_uid; }

  CpuData* cpuData() { return m_cpuData.get(); }

  const CpuData* cpuData() const { return m_cpuData.get(); }

  GpuData* gpuData() { return m_gpuData.get(); }

  const GpuData* gpuData() const { return m_gpuData.get(); }

private:
  uuids::uuid m_uid; //!< Unique identifier of the record

  std::unique_ptr<CpuData> m_cpuData;
  std::unique_ptr<GpuData> m_gpuData;
};

#endif // RENDERABLE_RECORD_H
