#include "mesh/MeshCpuRecord.h"

MeshCpuRecord::MeshCpuRecord(vtkSmartPointer<vtkPolyData> polyData, MeshInfo meshInfo)
  : m_polyData(polyData)
  , m_meshInfo(std::move(meshInfo))
{
}

const vtkSmartPointer<vtkPolyData> MeshCpuRecord::polyData() const
{
  return m_polyData;
}

vtkSmartPointer<vtkPolyData> MeshCpuRecord::polyData()
{
  return m_polyData;
}

const MeshInfo& MeshCpuRecord::meshInfo() const
{
  return m_meshInfo;
}

const MeshProperties& MeshCpuRecord::properties() const
{
  return m_properties;
}

MeshProperties& MeshCpuRecord::properties()
{
  return m_properties;
}

void MeshCpuRecord::setProperties(MeshProperties properties)
{
  m_properties = std::move(properties);
}
