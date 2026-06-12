#pragma once

#include <filesystem>
#include <optional>
#include <string_view>

namespace entropy::windowing::linux_ui_scale
{
/**
 * @brief Parse GNOME monitors.xml content and return the primary logical monitor scale.
 *
 * The first valid logical monitor scale is returned when no primary monitor is marked.
 */
std::optional<float> scaleFromMonitorsXml(std::string_view xml);

/**
 * @brief Parse a GNOME monitors.xml file and return its primary logical monitor scale.
 */
std::optional<float> scaleFromMonitorsXmlFile(const std::filesystem::path& path);

/**
 * @brief Return the default GNOME monitors.xml path for the current user environment.
 */
std::filesystem::path defaultMonitorsXmlPath();

/**
 * @brief Return the current GNOME primary logical monitor scale, if available.
 */
std::optional<float> primaryMonitorScale();
} // namespace entropy::windowing::linux_ui_scale
