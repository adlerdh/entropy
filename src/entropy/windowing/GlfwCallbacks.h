#pragma once

struct GLFWwindow;

void errorCallback(int error, const char* description);
void windowContentScaleCallback(GLFWwindow* window, float contentScaleX, float contentScaleY);
void windowCloseCallback(GLFWwindow* window);

/**
 * @note Will never be called on Wayland because there is no way for a client application to know
 * its global position
 */
void windowPositionCallback(GLFWwindow* window, int screenWindowPosX, int screenWindowPosY);

void windowSizeCallback(GLFWwindow* window, int windowWidth, int windowHeight);
void framebufferSizeCallback(GLFWwindow* window, int fbWidth, int fbHeight);
void cursorPosCallback(GLFWwindow* window, double mindowCursorPosX, double mindowCursorPosY);
void mouseButtonCallback(GLFWwindow* window, int button, int action, int mods);
void scrollCallback(GLFWwindow* window, double offsetX, double offsetY);
void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods);
void dropCallback(GLFWwindow* window, int count, const char** paths);
