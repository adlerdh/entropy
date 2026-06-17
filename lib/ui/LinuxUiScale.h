#pragma once

#include <filesystem>
#include <optional>
#include <string_view>

namespace entropy::ui::linux_ui_scale
{
/**
 * @brief Parse GNOME monitors.xml content.
 * @param xml XML text to parse.
 * @return Primary logical monitor scale, or the first valid scale when no primary monitor is marked.
 */
std::optional<float> scaleFromMonitorsXml(std::string_view xml);

/**
 * @brief Parse a GNOME monitors.xml file.
 * @param path File to read.
 * @return Primary logical monitor scale, if available.
 */
std::optional<float> scaleFromMonitorsXmlFile(const std::filesystem::path& path);

/** @brief Default GNOME monitors.xml path for the current user environment. */
std::filesystem::path defaultMonitorsXmlPath();

/** @brief Current GNOME primary logical monitor scale, if available. */
std::optional<float> primaryMonitorScale();
} // namespace entropy::ui::linux_ui_scale
