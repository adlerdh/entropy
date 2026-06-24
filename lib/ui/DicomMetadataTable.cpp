#include "ui/DicomMetadataTable.h"

#include <imgui/imgui.h>

#include <algorithm>
#include <cstring>
#include <functional>
#include <string>

namespace
{
float fittedColumnWidth(
  const char* header,
  const std::vector<dicom::MetadataEntry>& entries,
  const std::function<const std::string&(const dicom::MetadataEntry&)>& value)
{
  float width = ImGui::CalcTextSize(header).x;
  for (const auto& entry : entries) {
    width = std::max(width, ImGui::CalcTextSize(value(entry).c_str()).x);
  }

  const float padding = 2.0f * (ImGui::GetStyle().CellPadding.x + ImGui::GetStyle().ItemSpacing.x);
  return width + padding;
}
} // namespace

void renderDicomMetadataTable(
  const char* tableId,
  const std::vector<dicom::MetadataEntry>& entries,
  const ImVec2& tableSize,
  const char* tagColumnName,
  const char* nameColumnName,
  const char* valueColumnName,
  float tagColumnWidth,
  float nameColumnWidth)
{
  if (ImGui::BeginTable(
        tableId,
        3,
        ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable | ImGuiTableFlags_Reorderable |
          ImGuiTableFlags_ScrollX | ImGuiTableFlags_ScrollY | ImGuiTableFlags_Sortable,
        tableSize))
  {
    if (tagColumnWidth <= 0.0f) {
      tagColumnWidth =
        fittedColumnWidth(tagColumnName, entries, [](const dicom::MetadataEntry& entry) -> const std::string& {
          return entry.tag;
        });
    }
    if (nameColumnWidth <= 0.0f) {
      nameColumnWidth =
        fittedColumnWidth(nameColumnName, entries, [](const dicom::MetadataEntry& entry) -> const std::string& {
          return entry.name;
        });
    }

    ImGui::TableSetupColumn(tagColumnName, ImGuiTableColumnFlags_WidthFixed, tagColumnWidth);
    ImGui::TableSetupColumn(nameColumnName, ImGuiTableColumnFlags_WidthFixed, nameColumnWidth);
    ImGui::TableSetupColumn(valueColumnName, ImGuiTableColumnFlags_WidthStretch, 640.0f);
    ImGui::TableHeadersRow();

    std::vector<const dicom::MetadataEntry*> rows;
    rows.reserve(entries.size());
    for (const auto& entry : entries) {
      rows.push_back(&entry);
    }

    if (ImGuiTableSortSpecs* sortSpecs = ImGui::TableGetSortSpecs()) {
      if (sortSpecs->SpecsCount > 0) {
        const ImGuiTableColumnSortSpecs& spec = sortSpecs->Specs[0];
        std::sort(rows.begin(), rows.end(), [&spec](const auto* a, const auto* b) {
          const std::string* left = &a->tag;
          const std::string* right = &b->tag;
          if (spec.ColumnIndex == 1) {
            left = &a->name;
            right = &b->name;
          }
          else if (spec.ColumnIndex == 2) {
            left = &a->value;
            right = &b->value;
          }
          const int result = std::strcmp(left->c_str(), right->c_str());
          return spec.SortDirection == ImGuiSortDirection_Descending ? result > 0 : result < 0;
        });
      }
    }

    for (const auto* entry : rows) {
      ImGui::TableNextRow();
      ImGui::TableSetColumnIndex(0);
      ImGui::TextWrapped("%s", entry->tag.c_str());
      ImGui::TableSetColumnIndex(1);
      ImGui::TextWrapped("%s", entry->name.c_str());
      ImGui::TableSetColumnIndex(2);
      ImGui::TextWrapped("%s", entry->value.c_str());
    }
    ImGui::EndTable();
  }
}
