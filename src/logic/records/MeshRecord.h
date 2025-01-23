#ifndef MESH_RECORD_H
#define MESH_RECORD_H

#include "logic/records/RenderableRecord.h"
#include "mesh/MeshCpuRecord.h"
#include "rendering/records/MeshGpuRecord.h"

using MeshRecord = RenderableRecord<MeshCpuRecord, MeshGpuRecord>;

#endif // MESH_RECORD_H
