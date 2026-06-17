#pragma once

struct GLFWwindow;

/**
 * @brief Log GLFW errors through the application logger.
 */
void errorCallback(int error, const char* description);

/**
 * @brief Update cached UI/content scale for the window.
 */
void windowContentScaleCallback(GLFWwindow* window, float contentScaleX, float contentScaleY);

/**
 * @brief Request application shutdown from a GLFW close event.
 */
void windowCloseCallback(GLFWwindow* window);

/**
 * @note Will never be called on Wayland because there is no way for a client application to know
 * its global position
 */
void windowPositionCallback(GLFWwindow* window, int screenWindowPosX, int screenWindowPosY);

/**
 * @brief Update cached logical window size.
 */
void windowSizeCallback(GLFWwindow* window, int windowWidth, int windowHeight);

/**
 * @brief Update cached framebuffer size and refresh rendering resources.
 */
void framebufferSizeCallback(GLFWwindow* window, int fbWidth, int fbHeight);

/**
 * @brief Dispatch cursor movement to the active interaction state.
 */
void cursorPosCallback(GLFWwindow* window, double windowCursorPosX, double windowCursorPosY);

/**
 * @brief Dispatch mouse button presses and releases.
 */
void mouseButtonCallback(GLFWwindow* window, int button, int action, int mods);

/**
 * @brief Dispatch mouse wheel scrolling.
 */
void scrollCallback(GLFWwindow* window, double offsetX, double offsetY);

/**
 * @brief Dispatch keyboard input.
 */
void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods);

/**
 * @brief Dispatch file drops.
 */
void dropCallback(GLFWwindow* window, int count, const char** paths);
