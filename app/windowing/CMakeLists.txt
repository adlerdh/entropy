target_sources(entropy PRIVATE
  ControlFrame.cpp
  GlfwCallbacks.cpp
  GlfwWrapper.cpp
  Layout.cpp
  View.cpp
  ViewCameraDefaults.cpp
  WindowData.cpp
)

if(BUILD_TESTING)
  add_subdirectory(test)
endif()
