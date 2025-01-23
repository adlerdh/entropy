#ifndef MESH_CPU_RECORD_H
#define MESH_CPU_RECORD_H

#include "mesh/MeshInfo.hpp"
#include "mesh/MeshProperties.h"

#include <vtkPolyData.h>
#include <vtkSmartPointer.h>

/**
 * @brief Record for CPU storage of meshes.
 * Under the hood, mesh data is vtkPolyData.
 */
class MeshCpuRecord
{
public:
  MeshCpuRecord(vtkSmartPointer<vtkPolyData> polyData, MeshInfo meshInfo);

  MeshCpuRecord(const MeshCpuRecord&) = default;
  MeshCpuRecord& operator=(const MeshCpuRecord&) = default;

  MeshCpuRecord(MeshCpuRecord&&) = default;
  MeshCpuRecord& operator=(MeshCpuRecord&&) = default;

  ~MeshCpuRecord() = default;

  const vtkSmartPointer<vtkPolyData> polyData() const;
  vtkSmartPointer<vtkPolyData> polyData();

  const MeshInfo& meshInfo() const;

  const MeshProperties& properties() const;
  MeshProperties& properties();
  void setProperties(MeshProperties);

private:
  vtkSmartPointer<vtkPolyData> m_polyData;

  MeshInfo m_meshInfo;
  MeshProperties m_properties;
};

#endif // MESH_CPU_RECORD_H
