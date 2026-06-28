#pragma once

#include "registration/Types.h"

namespace registration
{

/**
 * @brief Return the capability and parameter schema for a registration backend.
 * @param backend Backend to describe.
 * @return Supported features, transform models, metrics, interpolation modes, and UI parameters.
 */
BackendCapabilities capabilitiesForBackend(Backend backend);

/**
 * @brief Return whether a backend advertises a feature.
 * @param capabilities Backend capabilities to inspect.
 * @param feature Feature to query.
 * @return True iff `feature` is listed in `capabilities`.
 */
bool supportsFeature(const BackendCapabilities& capabilities, Feature feature);

/**
 * @brief Return whether a backend advertises a transform model.
 * @param capabilities Backend capabilities to inspect.
 * @param model Transform model to query.
 * @return True iff `model` is listed in `capabilities`.
 */
bool supportsTransformModel(const BackendCapabilities& capabilities, TransformModel model);

/**
 * @brief Return whether a backend advertises a metric.
 * @param capabilities Backend capabilities to inspect.
 * @param metric Metric to query.
 * @return True iff `metric` is listed in `capabilities`.
 */
bool supportsMetric(const BackendCapabilities& capabilities, Metric metric);

} // namespace registration
