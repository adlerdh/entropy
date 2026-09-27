#pragma once
#include "registration/Types.h"

class AppData;

namespace registration_inputs
{
/**
 * @brief Export current loaded-image inputs into an exclusively created registration workspace.
 * @param data Application data providing the selected images, segmentations, and affine transforms.
 * @param job Job to prepare; updates input paths, ownedInputFiles, and optional affine initialization settings.
 * @return True when all required exports succeed; false on workspace creation, validation, or export failure.
 * @details Exports the selected component and frame with current pixels and geometry. Existing external-file inputs
 * are retained. Failure can leave a partially prepared workspace and updated job fields; this is not a transaction.
 */
bool materialize(AppData& data, registration::JobSpec& job);
} // namespace registration_inputs
