#include "ui/windows/ExportStatusWindow.h"

#include "ui/ExportJobService.h"
#include "ui/Scaling.h"

#include <imgui/imgui.h>

#include <algorithm>
#include <format>
#include <string>

namespace ui::export_jobs
{
namespace
{
std::string statusTitle(const Snapshot& snapshot)
{
  switch (snapshot.outcome) {
    case Outcome::Running:
      return "Exporting...";
    case Outcome::Succeeded:
      return "Export Complete";
    case Outcome::Failed:
      return "Export Failed";
    case Outcome::Cancelled:
      return "Export Cancelled";
  }
  return "Export";
}

std::string destinationText(const Snapshot& snapshot)
{
  if (snapshot.outputFileNames.size() == 1u) {
    return snapshot.outputFileNames.front().string();
  }
  if (snapshot.outputFileNames.size() > 1u) {
    return std::format("{} files written near {}", snapshot.outputFileNames.size(), snapshot.destination.string());
  }
  return snapshot.destination.string();
}
} // namespace

void renderStatusWindow(Service& service)
{
  const Snapshot snapshot = service.snapshot();
  if (!snapshot.hasJob) {
    return;
  }

  const ImGuiViewport* viewport = ImGui::GetMainViewport();
  const float margin = ui::scaledPixel(12.0f);
  const ImVec2 pos{
    viewport->WorkPos.x + viewport->WorkSize.x - margin,
    viewport->WorkPos.y + viewport->WorkSize.y - margin};
  ImGui::SetNextWindowPos(pos, ImGuiCond_Always, ImVec2{1.0f, 1.0f});

  constexpr ImGuiWindowFlags flags = ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings |
                                     ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoFocusOnAppearing |
                                     ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_AlwaysAutoResize;
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ui::scaledSize(12.0f, 10.0f));

  const std::string title = statusTitle(snapshot) + "###ExportStatus";
  if (ImGui::Begin(title.c_str(), nullptr, flags)) {
    if (snapshot.compact) {
      ImGui::TextUnformatted(Outcome::Running == snapshot.outcome ? "Exporting..." : statusTitle(snapshot).c_str());
      ImGui::SameLine();
      if (ImGui::SmallButton("Show")) {
        service.setCompact(false);
      }
    }
    else {
      if (!snapshot.description.empty()) {
        ImGui::TextUnformatted(snapshot.description.c_str());
      }
      if (!snapshot.phase.empty()) {
        ImGui::TextDisabled("%s", snapshot.phase.c_str());
      }

      if (Outcome::Running == snapshot.outcome) {
        const float progress = snapshot.progress.value_or(0.0f);
        const char* overlay = snapshot.indeterminate ? "Working..." : nullptr;
        ImGui::ProgressBar(progress, ImVec2{ui::scaledPixel(360.0f), 0.0f}, overlay);
      }

      const std::string destination = destinationText(snapshot);
      if (!destination.empty()) {
        ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + ui::scaledPixel(360.0f));
        ImGui::TextDisabled("%s", destination.c_str());
        ImGui::PopTextWrapPos();
      }
      if (!snapshot.message.empty()) {
        const ImVec4 color = Outcome::Failed == snapshot.outcome ? ImVec4{0.9f, 0.3f, 0.3f, 1.0f}
                                                                 : ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled);
        ImGui::PushStyleColor(ImGuiCol_Text, color);
        ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + ui::scaledPixel(360.0f));
        ImGui::TextUnformatted(snapshot.message.c_str());
        ImGui::PopTextWrapPos();
        ImGui::PopStyleColor();
      }

      if (Outcome::Running == snapshot.outcome) {
        if (
          ImGui::Button(snapshot.cancellationRequested ? "Cancelling..." : "Cancel") && !snapshot.cancellationRequested)
        {
          service.requestCancel();
        }
        ImGui::SameLine();
        if (ImGui::Button("Hide")) {
          service.setCompact(true);
        }
      }
      else if (ImGui::Button("Dismiss")) {
        service.dismiss();
      }
    }
  }
  ImGui::End();
  ImGui::PopStyleVar();
}
} // namespace ui::export_jobs
