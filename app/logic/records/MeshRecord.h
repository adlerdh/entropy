#pragma once

#include "logic/records/RenderableRecord.h"
#include "mesh/MeshCpuRecord.h"
#include "rendering/records/MeshGpuRecord.h"

using MeshRecord = RenderableRecord<MeshCpuRecord, MeshGpuRecord>;
