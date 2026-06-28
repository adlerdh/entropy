#include "ui/windows/RegistrationWindow.h"

#include "logic/app/Data.h"
#include "ui/Helpers.h"

#include "registration/Config.h"
#include "registration/Setup.h"

#include <imgui/imgui.h>
#include <uuid.h>

#include <array>
#include <algorithm>
#include <filesystem>
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
  ImGui::Button("Start registration");
  ImGui::EndDisabled();
  if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
    ImGui::SetTooltip("%s", "Starting jobs will be enabled when the backend runner is wired.");
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

  ImGui::TextDisabled("No registration jobs have been started.");
  ImGui::TextWrapped("%s", "Completed jobs will appear here with progress, logs, warnings, and import controls.");
  ImGui::End();
}
