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
 * @param tagColumnName Label for the metadata key/tag column.
 * @param nameColumnName Label for the metadata name/type column.
 * @param valueColumnName Label for the metadata value column.
 * @param tagColumnWidth Fixed key/tag column width, or a non-positive value to fit the column contents.
 * @param nameColumnWidth Fixed name/type column width, or a non-positive value to fit the column contents.
 */
void renderDicomMetadataTable(
  const char* tableId,
  const std::vector<dicom::MetadataEntry>& entries,
  const ImVec2& tableSize,
  const char* tagColumnName = "Tag",
  const char* nameColumnName = "Name",
  const char* valueColumnName = "Value",
  float tagColumnWidth = 118.0f,
  float nameColumnWidth = 260.0f);
