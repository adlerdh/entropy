#include "ui/LinuxUiScale.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <fstream>

namespace linux_ui_scale = entropy::ui::linux_ui_scale;

TEST_CASE("GNOME monitor scale parser returns primary logical monitor scale", "[ui][scale]")
{
  constexpr const char* xml = R"(<monitors version="2">
  <configuration>
    <logicalmonitor>
      <x>0</x>
      <y>0</y>
      <scale>1</scale>
      <monitor><monitorspec><connector>HDMI-0</connector></monitorspec></monitor>
    </logicalmonitor>
    <logicalmonitor>
      <x>3840</x>
      <y>0</y>
      <scale>2</scale>
      <primary>yes</primary>
      <monitor><monitorspec><connector>DP-0</connector></monitorspec></monitor>
    </logicalmonitor>
  </configuration>
</monitors>)";

  const auto scale = linux_ui_scale::scaleFromMonitorsXml(xml);
  REQUIRE(scale);
  REQUIRE(*scale == Catch::Approx(2.0f));
}

TEST_CASE("GNOME monitor scale parser falls back to first valid logical monitor", "[ui][scale]")
{
  constexpr const char* xml = R"(<monitors version="2">
  <configuration>
    <logicalmonitor><scale>1.25</scale></logicalmonitor>
    <logicalmonitor><scale>2</scale></logicalmonitor>
  </configuration>
</monitors>)";

  const auto scale = linux_ui_scale::scaleFromMonitorsXml(xml);
  REQUIRE(scale);
  REQUIRE(*scale == Catch::Approx(1.25f));
}

TEST_CASE("GNOME monitor scale parser rejects invalid scales", "[ui][scale]")
{
  REQUIRE_FALSE(linux_ui_scale::scaleFromMonitorsXml("<monitors />"));
  REQUIRE_FALSE(linux_ui_scale::scaleFromMonitorsXml("<logicalmonitor><scale>0</scale></logicalmonitor>"));
  REQUIRE_FALSE(linux_ui_scale::scaleFromMonitorsXml("<logicalmonitor><scale>nan</scale></logicalmonitor>"));
  REQUIRE_FALSE(linux_ui_scale::scaleFromMonitorsXml("<logicalmonitor><scale>9</scale></logicalmonitor>"));
  REQUIRE_FALSE(linux_ui_scale::scaleFromMonitorsXml("<logicalmonitor><scale>2x</scale></logicalmonitor>"));
}

TEST_CASE("GNOME monitor scale parser reads monitors XML files", "[ui][scale]")
{
  const std::filesystem::path path =
    std::filesystem::temp_directory_path() / "entropy-linux-ui-scale-test-monitors.xml";
  {
    std::ofstream file(path);
    file << "<monitors><configuration><logicalmonitor><scale>1.75</scale><primary>yes</primary></logicalmonitor></"
            "configuration></monitors>";
  }

  const auto scale = linux_ui_scale::scaleFromMonitorsXmlFile(path);
  std::filesystem::remove(path);

  REQUIRE(scale);
  REQUIRE(*scale == Catch::Approx(1.75f));
}
