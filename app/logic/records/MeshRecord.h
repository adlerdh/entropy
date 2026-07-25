#pragma once

#include "logic/records/RenderableRecord.h"
#include "rendering/records/MeshGpuRecord.h"

class MeshCpuRecord;

using MeshRecord = RenderableRecord<MeshCpuRecord, MeshGpuRecord>;
