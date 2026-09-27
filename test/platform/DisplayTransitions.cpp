#include "ui/UiScaleManager.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <imgui.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <vector>

namespace
{
struct DisplaySession
{
  GLFWwindow* window = nullptr;

  DisplaySession()
  {
    if (!glfwInit()) return;
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    window = glfwCreateWindow(320, 240, "Entropy mixed-DPI validation — long image properties label", nullptr, nullptr);
    ImGui::CreateContext();
    ImGui::GetIO().IniFilename = nullptr;
  }

  ~DisplaySession()
  {
    if (ImGui::GetCurrentContext()) ImGui::DestroyContext();
    if (window) glfwDestroyWindow(window);
    glfwTerminate();
  }
};
} // namespace

TEST_CASE("Physical monitor transitions update window scale and stable UI sizing", "[platform][hardware]")
{
  DisplaySession session;
  REQUIRE(session.window);
  int count = 0;
  GLFWmonitor** monitors = glfwGetMonitors(&count);
  REQUIRE(monitors);
  REQUIRE(count >= 2);
  std::vector<float> scales;
  UiScaleManager manager;
  manager.captureBaseStyle(ImGui::GetStyle());

  for (int pass = 0; pass < 2; ++pass) {
    for (int i = 0; i < count; ++i) {
      int x = 0, y = 0, width = 0, height = 0;
      glfwGetMonitorWorkarea(monitors[i], &x, &y, &width, &height);
      REQUIRE(width >= 320);
      REQUIRE(height >= 240);

      float expectedX = 0, expectedY = 0;
      glfwGetMonitorContentScale(monitors[i], &expectedX, &expectedY);
      REQUIRE(std::isfinite(expectedX));
      REQUIRE(expectedX > 0);

      if (pass == 0) scales.push_back(expectedX);
      INFO("Monitor " << glfwGetMonitorName(monitors[i]) << ": scale " << expectedX << "," << expectedY);
      glfwSetWindowPos(session.window, x + (width - 320) / 2, y + (height - 240) / 2);

      const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds{5};
      float actualX = 0, actualY = 0;

      do {
        glfwPollEvents();
        glfwGetWindowContentScale(session.window, &actualX, &actualY);
        if (std::abs(actualX - expectedX) < 0.01f && std::abs(actualY - expectedY) < 0.01f) break;
        glfwWaitEventsTimeout(0.01);
      } while (std::chrono::steady_clock::now() < deadline);

      CHECK(actualX == Catch::Approx(expectedX).margin(0.01f));
      CHECK(actualY == Catch::Approx(expectedY).margin(0.01f));

      manager.applyContentScale(actualX);
      const auto padding = ImGui::GetStyle().WindowPadding;

      for (int poll = 0; poll < 20; ++poll) {
        glfwPollEvents();
        CHECK_FALSE(manager.applyContentScale(actualX));
        CHECK(ImGui::GetStyle().WindowPadding.x == padding.x);
        CHECK(ImGui::GetStyle().WindowPadding.y == padding.y);
      }
    }
  }
  
  const auto [minimum, maximum] = std::minmax_element(scales.begin(), scales.end());
  REQUIRE(*maximum - *minimum >= 0.1f); // This lane must really exercise mixed DPI.
}
