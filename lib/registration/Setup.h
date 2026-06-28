#pragma once

#include "registration/Capabilities.h"
#include "registration/Commands.h"
#include "registration/Validation.h"

#include <filesystem>
#include <string>
#include <vector>

namespace registration
{

/**
 * @brief Available image choice shown by the registration setup UI.
 */
struct SetupImageChoice
{
  DataRef image;            //!< Image that can be used as fixed or moving input.
  int dimension = 3;        //!< Spatial dimension of the image.
  bool isReference = false; //!< True when this is Entropy's reference image.
  bool isActive = false;    //!< True when this is Entropy's active image.
};

/**
 * @brief Current value for one backend parameter in the setup UI.
 */
struct ParameterValue
{
  std::string key;   //!< Stable parameter key.
  std::string value; //!< Current text-form value.
};

/**
 * @brief Backend-neutral state for the Register Image setup dialog.
 */
struct SetupState
{
  JobSpec job;                                 //!< Editable job specification.
  BackendCapabilities capabilities;            //!< Capabilities for the selected backend.
  std::vector<ParameterValue> parameterValues; //!< Current backend parameter values.
  ValidationResult validation;                 //!< Latest validation result.
  bool showAdvancedParameters = false;         //!< Reveal advanced parameters.
  bool showExpertParameters = false;           //!< Reveal expert parameters.
};

/**
 * @brief Create default setup state from Entropy's current images.
 * @param images Candidate images.
 * @param backend Initial backend.
 * @param outputDirectory Default output directory.
 * @return Setup state with reference image as fixed and active image as moving when possible.
 */
SetupState createSetupState(
  const std::vector<SetupImageChoice>& images,
  Backend backend,
  const std::filesystem::path& outputDirectory);

/**
 * @brief Recompute backend capabilities, defaults, and validation after changing backend.
 * @param state Setup state to update.
 * @param backend New backend.
 */
void setBackend(SetupState& state, Backend backend);

/**
 * @brief Recompute setup validation for the current job and capabilities.
 * @param state Setup state to update.
 */
void refreshValidation(SetupState& state);

/**
 * @brief Return parameters visible at the current advanced/expert disclosure level.
 * @param state Setup state to inspect.
 * @return Visible parameter schemas in display order.
 */
std::vector<ParameterSchema> visibleParameters(const SetupState& state);

/**
 * @brief Return the mutable value record for a parameter.
 * @param state Setup state to inspect.
 * @param key Parameter key.
 * @return Value record, or nullptr when absent.
 */
ParameterValue* findParameterValue(SetupState& state, std::string_view key);

/**
 * @brief Return the immutable value record for a parameter.
 * @param state Setup state to inspect.
 * @param key Parameter key.
 * @return Value record, or nullptr when absent.
 */
const ParameterValue* findParameterValue(const SetupState& state, std::string_view key);

/**
 * @brief Return command preview strings for the current setup state.
 * @param state Setup state to inspect.
 * @param commandOptions Executable and bridge options.
 * @return Display commands, or an empty list when validation blocks launch.
 */
std::vector<std::string> commandPreviews(const SetupState& state, const CommandGenerationOptions& commandOptions);

/**
 * @brief Return command preview strings using default executable names.
 * @param state Setup state to inspect.
 * @return Display commands, or an empty list when validation blocks launch.
 */
std::vector<std::string> commandPreviews(const SetupState& state);

} // namespace registration
