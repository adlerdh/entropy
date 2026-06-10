#pragma once

#include "image/DicomSeries.h"

#include <imgui/imgui.h>

#include <vector>

/**
 * @brief Render a sortable DICOM metadata table.
 *
 * @param tableId Stable ImGui table identifier.
 * @param entries Metadata entries to display.
 * @param tableSize Requested table size.
 */
void renderDicomMetadataTable(
  const char* tableId,
  const std::vector<dicom::MetadataEntry>& entries,
  const ImVec2& tableSize);
