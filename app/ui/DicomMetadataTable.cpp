#include "ui/DicomMetadataTable.h"

#include <imgui/imgui.h>

#include <algorithm>
#include <cstring>
#include <string>

void renderDicomMetadataTable(
  const char* tableId,
  const std::vector<dicom::MetadataEntry>& entries,
  const ImVec2& tableSize)
{
  if (ImGui::BeginTable(
        tableId,
        3,
        ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable | ImGuiTableFlags_Reorderable |
          ImGuiTableFlags_ScrollX | ImGuiTableFlags_ScrollY | ImGuiTableFlags_Sortable,
        tableSize))
  {
    ImGui::TableSetupColumn("Tag", ImGuiTableColumnFlags_WidthFixed, 118.0f);
    ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthFixed, 260.0f);
    ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch, 640.0f);
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
