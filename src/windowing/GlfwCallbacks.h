#ifndef GLFW_CALLBACKS_H
#define GLFW_CALLBACKS_H

struct GLFWwindow;

void errorCallback(int error, const char* description);
void windowContentScaleCallback(GLFWwindow* window, float contentScaleX, float contentScaleY);
void windowCloseCallback(GLFWwindow* window);
void windowPositionCallback(GLFWwindow* window, int screenWindowPosX, int screenWindowPosY);
void windowSizeCallback(GLFWwindow* window, int windowWidth, int windowHeight);
void framebufferSizeCallback(GLFWwindow* window, int fbWidth, int fbHeight);
void cursorPosCallback(GLFWwindow* window, double mindowCursorPosX, double mindowCursorPosY);
void mouseButtonCallback(GLFWwindow* window, int button, int action, int mods);
void scrollCallback(GLFWwindow* window, double offsetX, double offsetY);
void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods);
void dropCallback(GLFWwindow* window, int count, const char** paths);

#endif // GLFW_CALLBACKS_H
