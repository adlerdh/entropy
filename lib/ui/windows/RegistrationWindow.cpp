#include "ui/windows/RegistrationWindow.h"

#include "logic/app/Data.h"
#include "ui/Helpers.h"

#include "registration/Config.h"
#include "registration/Setup.h"

#include <imgui/imgui.h>
#include <uuid.h>

#include <array>
#include <algorithm>
#include <cfloat>
#include <filesystem>
#include <sstream>
#include <string>
#include <vector>

namespace
{

constexpr std::array k_backends{
  registration::Backend::Greedy,
  registration::Backend::ANTs,
  registration::Backend::FireANTs};

std::vector<registration::SetupImageChoice> imageChoices(const AppData& appData)
{
  std::vector<registration::SetupImageChoice> choices;
  choices.reserve(appData.numImages());

  const std::optional<uuids::uuid> refUid = appData.refImageUid();
  const std::optional<uuids::uuid> activeUid = appData.activeImageUid();
  for (const uuids::uuid& imageUid : appData.imageUidsOrdered()) {
    const Image* image = appData.image(imageUid);
    if (!image) {
      continue;
    }

    registration::SetupImageChoice choice;
    choice.image.uid = uuids::to_string(imageUid);
    choice.image.fileName = image->header().fileName();
    choice.image.displayName = image->settings().displayName();
    choice.image.source = registration::DataSource::LoadedImage;
    const glm::uvec3 dims = image->header().pixelDimensions();
    choice.dimension =
      static_cast<int>(std::max(1u, static_cast<unsigned>((dims.x > 1u) + (dims.y > 1u) + (dims.z > 1u))));
    choice.isReference = refUid && *refUid == imageUid;
    choice.isActive = activeUid && *activeUid == imageUid;
    choices.push_back(std::move(choice));
  }

  return choices;
}

int backendIndex(registration::Backend backend)
{
  for (int i = 0; i < static_cast<int>(k_backends.size()); ++i) {
    if (k_backends.at(static_cast<std::size_t>(i)) == backend) {
      return i;
    }
  }
  return 0;
}

void renderDataRefText(const char* label, const registration::DataRef& ref)
{
  ImGui::Text("%s", label);
  ImGui::SameLine();
  ImGui::TextDisabled("%s", ref.displayName.empty() ? "None" : ref.displayName.c_str());
}

void renderValidation(const registration::ValidationResult& validation)
{
  if (validation.messages.empty()) {
    ImGui::TextColored(ImVec4{0.35f, 0.85f, 0.45f, 1.0f}, "Ready");
    return;
  }

  for (const registration::ValidationMessage& message : validation.messages) {
    const bool isError = message.severity == registration::ValidationSeverity::Error;
    const ImVec4 color = isError ? ImVec4{0.95f, 0.35f, 0.30f, 1.0f} : ImVec4{0.95f, 0.75f, 0.25f, 1.0f};
    ImGui::TextColored(color, "%s: %s", isError ? "Error" : "Warning", message.text.c_str());
  }
}

const char* statusText(registration::JobStatus status)
{
  switch (status) {
    case registration::JobStatus::Queued:
      return "Queued";
    case registration::JobStatus::PreparingInputs:
      return "Preparing";
    case registration::JobStatus::Running:
      return "Running";
    case registration::JobStatus::WritingOutputs:
      return "Writing";
    case registration::JobStatus::ImportingOutputs:
      return "Importing";
    case registration::JobStatus::Completed:
      return "Completed";
    case registration::JobStatus::Cancelled:
      return "Cancelled";
    case registration::JobStatus::Failed:
      return "Failed";
  }
  return "Unknown";
}

ImVec4 statusColor(registration::JobStatus status)
{
  switch (status) {
    case registration::JobStatus::Completed:
      return ImVec4{0.35f, 0.85f, 0.45f, 1.0f};
    case registration::JobStatus::Cancelled:
      return ImVec4{0.75f, 0.75f, 0.75f, 1.0f};
    case registration::JobStatus::Failed:
      return ImVec4{0.95f, 0.35f, 0.30f, 1.0f};
    case registration::JobStatus::Queued:
    case registration::JobStatus::PreparingInputs:
    case registration::JobStatus::Running:
    case registration::JobStatus::WritingOutputs:
    case registration::JobStatus::ImportingOutputs:
      return ImVec4{0.35f, 0.70f, 0.95f, 1.0f};
  }
  return ImVec4{0.75f, 0.75f, 0.75f, 1.0f};
}

std::string jobTitle(const registration::JobRecord& job)
{
  const std::string fixed = job.spec.fixedImage.displayName.empty() ? "fixed" : job.spec.fixedImage.displayName;
  const std::string moving = job.spec.movingImage.displayName.empty() ? "moving" : job.spec.movingImage.displayName;
  return moving + " to " + fixed;
}

float progressFraction(const registration::JobRecord& job)
{
  if (const std::optional<double> progress = registration::latestProgress(job)) {
    return static_cast<float>(std::clamp(*progress, 0.0, 1.0));
  }
  return registration::JobStatus::Completed == job.status ? 1.0f : 0.0f;
}

const char* outputStreamLabel(registration::OutputStream stream)
{
  return registration::OutputStream::Stderr == stream ? "stderr" : "stdout";
}

std::string jobLogText(const registration::JobRecord& job)
{
  std::ostringstream stream;
  stream << jobTitle(job) << '\n';
  stream << "Status: " << statusText(job.status) << "\n\n";

  if (!job.commands.empty()) {
    stream << "Commands:\n";
    for (const registration::CommandExecution& command : job.commands) {
      stream << "  " << command.displayString << '\n';
      stream << "    exit: " << command.result.exitCode << '\n';
      if (!command.result.failureMessage.empty()) {
        stream << "    error: " << command.result.failureMessage << '\n';
      }
    }
    stream << '\n';
  }

  if (!job.warnings.empty()) {
    stream << "Warnings:\n";
    for (const std::string& warning : job.warnings) {
      stream << "  " << warning << '\n';
    }
    stream << '\n';
  }

  if (!job.errorMessage.empty()) {
    stream << "Error:\n  " << job.errorMessage << "\n\n";
  }

  stream << "Output:\n";
  for (const registration::ProcessOutputLine& line : job.outputLines) {
    stream << '[' << outputStreamLabel(line.stream) << "] " << line.text << '\n';
  }
  return stream.str();
}

void renderRegistrationJobDetailsPopup(const registration::JobRecord* job)
{
  if (!job) {
    return;
  }

  if (ImGui::BeginPopupModal("Registration Job Details", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
    ImGui::TextWrapped("%s", jobTitle(*job).c_str());
    ImGui::TextColored(statusColor(job->status), "%s", statusText(job->status));

    const std::string logText = jobLogText(*job);
    if (ImGui::Button("Copy Log")) {
      ImGui::SetClipboardText(logText.c_str());
    }
    ImGui::SameLine();
    if (ImGui::Button("Close")) {
      ImGui::CloseCurrentPopup();
    }

    ImGui::Separator();
    if (ImGui::BeginChild("RegistrationJobLog", ImVec2{720.0f, 420.0f}, ImGuiChildFlags_Borders)) {
      ImGui::TextUnformatted(logText.c_str());
    }
    ImGui::EndChild();
    ImGui::EndPopup();
  }
}

void renderVisibleParameters(const registration::SetupState& state)
{
  const std::vector<registration::ParameterSchema> parameters = registration::visibleParameters(state);
  if (parameters.empty()) {
    ImGui::TextDisabled("No parameters visible.");
    return;
  }

  for (const registration::ParameterSchema& parameter : parameters) {
    const registration::ParameterValue* value = registration::findParameterValue(state, parameter.key);
    const std::string valueText = value ? value->value : parameter.defaultValue;
    ImGui::Text("%s", parameter.label.c_str());
    if (!parameter.tooltip.empty() && ImGui::IsItemHovered()) {
      ImGui::SetTooltip("%s", parameter.tooltip.c_str());
    }
    ImGui::SameLine();
    ImGui::TextDisabled("%s", valueText.c_str());
  }
}

void renderCommandPreview(
  const registration::SetupState& state,
  const registration::CommandGenerationOptions& commandOptions)
{
  const std::vector<std::string> commands = registration::commandPreviews(state, commandOptions);
  if (commands.empty()) {
    ImGui::TextDisabled("No command preview available.");
    return;
  }

  for (const std::string& command : commands) {
    ImGui::TextWrapped("%s", command.c_str());
  }
}

} // namespace

void renderRegistrationSetupWindow(AppData& appData)
{
  setNextDockablePanelWindowClass();
  if (!ImGui::Begin("Register Image##RegistrationSetup", &appData.guiData().m_showRegistrationSetupWindow)) {
    ImGui::End();
    return;
  }

  static bool s_showAdvanced = false;
  registration::BackendConfig& config = appData.settings().registrationBackendConfig();
  static bool s_showExpert = config.showExpertOptionsByDefault;

  const std::filesystem::path outputDirectory =
    config.defaultOutputDirectory.empty() ? std::filesystem::temp_directory_path() : config.defaultOutputDirectory;
  registration::SetupState state =
    registration::createSetupState(imageChoices(appData), config.defaultBackend, outputDirectory);
  state.showAdvancedParameters = s_showAdvanced;
  state.showExpertParameters = s_showExpert;

  int selectedBackend = backendIndex(config.defaultBackend);
  const std::string preview{registration::label(k_backends.at(static_cast<std::size_t>(selectedBackend)))};
  if (ImGui::BeginCombo("Backend", preview.c_str())) {
    for (int i = 0; i < static_cast<int>(k_backends.size()); ++i) {
      const registration::Backend backend = k_backends.at(static_cast<std::size_t>(i));
      const bool selected = i == selectedBackend;
      const std::string backendLabel{registration::label(backend)};
      if (ImGui::Selectable(backendLabel.c_str(), selected)) {
        config.defaultBackend = backend;
      }
      if (selected) {
        ImGui::SetItemDefaultFocus();
      }
    }
    ImGui::EndCombo();
  }

  renderDataRefText("Fixed:", state.job.fixedImage);
  renderDataRefText("Moving:", state.job.movingImage);
  ImGui::Text("Transform:");
  ImGui::SameLine();
  ImGui::TextDisabled("%s", registration::label(state.job.transformModel).data());
  ImGui::Text("Metric:");
  ImGui::SameLine();
  ImGui::TextDisabled("%s", registration::label(state.job.metric).data());

  ImGui::Separator();
  renderValidation(state.validation);

  ImGui::Separator();
  ImGui::Checkbox("Show advanced options", &s_showAdvanced);
  ImGui::SameLine();
  ImGui::Checkbox("Show expert options", &s_showExpert);

  if (ImGui::CollapsingHeader("Parameters", ImGuiTreeNodeFlags_DefaultOpen)) {
    renderVisibleParameters(state);
  }

  if (ImGui::CollapsingHeader("Command Preview", ImGuiTreeNodeFlags_DefaultOpen)) {
    renderCommandPreview(state, registration::commandOptions(config));
  }

  ImGui::BeginDisabled(!state.validation.canLaunch());
  if (ImGui::Button("Start registration")) {
    appData.registrationJobs().add(state.job);
    appData.guiData().m_showRegistrationJobsWindow = true;
  }
  ImGui::EndDisabled();
  if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
    ImGui::SetTooltip("%s", "Create a registration job record. Backend execution will be wired in the next phase.");
  }

  ImGui::End();
}

void renderRegistrationJobsWindow(AppData& appData)
{
  setNextDockablePanelWindowClass();
  if (!ImGui::Begin("Registration Jobs##RegistrationJobs", &appData.guiData().m_showRegistrationJobsWindow)) {
    ImGui::End();
    return;
  }

  registration::JobStore& jobs = appData.registrationJobs();
  if (jobs.jobs().empty()) {
    ImGui::TextDisabled("No registration jobs have been started.");
    ImGui::TextWrapped("%s", "Started jobs will appear here with progress, logs, warnings, and import controls.");
    ImGui::End();
    return;
  }

  static std::string s_selectedLogJobId;
  const registration::JobRecord* selectedLogJob = nullptr;
  bool openDetailsPopup = false;

  constexpr ImGuiTableFlags tableFlags = ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_RowBg |
                                         ImGuiTableFlags_Resizable | ImGuiTableFlags_SizingStretchProp |
                                         ImGuiTableFlags_ScrollY;
  if (ImGui::BeginTable("RegistrationJobsTable", 8, tableFlags, ImVec2{0.0f, 0.0f})) {
    ImGui::TableSetupColumn("Job", ImGuiTableColumnFlags_WidthStretch, 1.7f);
    ImGui::TableSetupColumn("Backend", ImGuiTableColumnFlags_WidthFixed, 80.0f);
    ImGui::TableSetupColumn("Transform", ImGuiTableColumnFlags_WidthFixed, 120.0f);
    ImGui::TableSetupColumn("Metric", ImGuiTableColumnFlags_WidthFixed, 80.0f);
    ImGui::TableSetupColumn("Status", ImGuiTableColumnFlags_WidthFixed, 90.0f);
    ImGui::TableSetupColumn("Progress", ImGuiTableColumnFlags_WidthFixed, 110.0f);
    ImGui::TableSetupColumn("Message", ImGuiTableColumnFlags_WidthStretch, 1.3f);
    ImGui::TableSetupColumn("Actions", ImGuiTableColumnFlags_WidthFixed, 150.0f);
    ImGui::TableHeadersRow();

    for (const registration::JobRecord& job : jobs.jobs()) {
      ImGui::PushID(job.id.c_str());
      ImGui::TableNextRow();

      ImGui::TableSetColumnIndex(0);
      ImGui::TextWrapped("%s", jobTitle(job).c_str());

      ImGui::TableSetColumnIndex(1);
      const std::string backendLabel{registration::label(job.spec.backend)};
      ImGui::TextUnformatted(backendLabel.c_str());

      ImGui::TableSetColumnIndex(2);
      const std::string transformLabel{registration::label(job.spec.transformModel)};
      ImGui::TextUnformatted(transformLabel.c_str());

      ImGui::TableSetColumnIndex(3);
      const std::string metricLabel{registration::label(job.spec.metric)};
      ImGui::TextUnformatted(metricLabel.c_str());

      ImGui::TableSetColumnIndex(4);
      ImGui::TextColored(statusColor(job.status), "%s", statusText(job.status));

      ImGui::TableSetColumnIndex(5);
      ImGui::ProgressBar(progressFraction(job), ImVec2{-FLT_MIN, 0.0f});

      ImGui::TableSetColumnIndex(6);
      const std::string message = registration::latestMessage(job);
      if (!message.empty()) {
        ImGui::TextWrapped("%s", message.c_str());
      }
      else if (!job.warnings.empty()) {
        ImGui::TextWrapped("%s", job.warnings.back().c_str());
      }
      else {
        ImGui::TextDisabled("Waiting for backend execution");
      }

      ImGui::TableSetColumnIndex(7);
      const bool active = registration::isActiveJobStatus(job.status);
      ImGui::BeginDisabled(!active);
      if (ImGui::SmallButton("Cancel")) {
        jobs.appendProgress(
          job.id,
          registration::ProgressEvent{
            .kind = registration::ProgressEventKind::Cancelled,
            .message = "Registration was cancelled before backend execution"});
      }
      ImGui::EndDisabled();
      ImGui::SameLine();
      ImGui::BeginDisabled(!job.manifest.has_value());
      ImGui::SmallButton("Import");
      ImGui::EndDisabled();
      if (!job.commands.empty() || !job.outputLines.empty() || !job.warnings.empty() || !job.errorMessage.empty()) {
        ImGui::SameLine();
        if (ImGui::SmallButton("Log")) {
          s_selectedLogJobId = job.id;
          openDetailsPopup = true;
        }
      }
      ImGui::PopID();
    }

    ImGui::EndTable();
  }

  if (!s_selectedLogJobId.empty()) {
    selectedLogJob = jobs.find(s_selectedLogJobId);
  }
  if (openDetailsPopup) {
    ImGui::OpenPopup("Registration Job Details");
  }
  renderRegistrationJobDetailsPopup(selectedLogJob);
  ImGui::End();
}

void renderRegistrationProgressWindow(AppData& appData)
{
  const registration::JobStore& jobs = appData.registrationJobs();
  if (!jobs.hasActiveJobs()) {
    return;
  }

  const registration::JobRecord* activeJob = nullptr;
  for (const registration::JobRecord& job : jobs.jobs()) {
    if (registration::isActiveJobStatus(job.status)) {
      activeJob = &job;
      break;
    }
  }
  if (!activeJob) {
    return;
  }

  const ImGuiViewport* viewport = ImGui::GetMainViewport();
  const ImVec2 padding = ImGui::GetStyle().WindowPadding;
  const ImVec2 pos{viewport->WorkPos.x + padding.x, viewport->WorkPos.y + viewport->WorkSize.y - padding.y};
  ImGui::SetNextWindowPos(pos, ImGuiCond_Always, ImVec2{0.0f, 1.0f});
  ImGui::SetNextWindowSizeConstraints(ImVec2{280.0f, 0.0f}, ImVec2{520.0f, FLT_MAX});

  constexpr ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize |
                                     ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing |
                                     ImGuiWindowFlags_NoNav;
  if (ImGui::Begin("RegistrationProgress", nullptr, flags)) {
    ImGui::TextUnformatted("Registration job");
    ImGui::TextWrapped("%s", jobTitle(*activeJob).c_str());
    ImGui::TextColored(statusColor(activeJob->status), "%s", statusText(activeJob->status));
    ImGui::ProgressBar(progressFraction(*activeJob), ImVec2{ImGui::GetContentRegionAvail().x, 0.0f});
    const std::string message = registration::latestMessage(*activeJob);
    if (!message.empty()) {
      ImGui::TextWrapped("%s", message.c_str());
    }
    else {
      ImGui::TextDisabled("Waiting for backend execution");
    }
  }
  ImGui::End();
}
