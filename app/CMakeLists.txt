if(APPLE)
  add_executable(entropy MACOSX_BUNDLE)
else()
  add_executable(entropy)
endif()

target_sources(entropy PRIVATE
  main.cpp
  AppCallbacks.cpp
  AppLifecycle.cpp
  AppProject.cpp
  AppRegistration.cpp
  EntropyApp.cpp
  "${entropy_EXT_DIR}/safeclib/strerrorlen_s.cpp"
)

if(Entropy_ENABLE_IWYU OR Entropy_ENABLE_CLANG_TIDY)
  set_source_files_properties("${entropy_EXT_DIR}/safeclib/strerrorlen_s.cpp" PROPERTIES SKIP_LINTING ON)
endif()

if(WIN32)
  set(entropy_WINDOWS_VERSION_RC "${CMAKE_CURRENT_BINARY_DIR}/EntropyWindowsVersion.rc")
  configure_file("${CMAKE_SOURCE_DIR}/cmake/EntropyWindowsVersion.rc.in" "${entropy_WINDOWS_VERSION_RC}" @ONLY)
  target_sources(entropy PRIVATE "${entropy_WINDOWS_VERSION_RC}")
endif()

add_subdirectory(logic)
add_subdirectory(rendering)
add_subdirectory(windowing)

entropy_disable_linting_for_targets(entropy)

target_compile_definitions(entropy PRIVATE
  ENTROPY_ABOUT_ICON_RESOURCE_PATH="res/icons/Linux/hicolor/128x128/apps/io.github.adlerdh.entropy.png"
  ENTROPY_RESOURCE_DIR="${CMAKE_INSTALL_FULL_DATADIR}/entropy"
)

target_link_libraries(entropy PRIVATE
  CLI11::CLI11
  Entropy::Common
  Entropy::Image
  Entropy::Layout
  Entropy::UI
  Entropy::Viewer
  glfw
  glad
  glm::glm
  imgui
  implot
  nanovg
  nfd::nfd
  nlohmann_json::nlohmann_json
  Qt6::Core
  stduuid
  entropy::colormaps
  entropy::fonts
  entropy::icons
  entropy::licenses
  entropy::shaders
  entropy_warnings
  ${VTK_LIBRARIES}
)

entropy_enable_coverage_for_target(entropy)

if(APPLE)
  find_library(COCOA_LIBRARY Cocoa REQUIRED)
  target_link_libraries(entropy PRIVATE "${COCOA_LIBRARY}")
endif()

if(WIN32)
  set_target_properties(entropy PROPERTIES WIN32_EXECUTABLE "$<$<NOT:$<CONFIG:Debug>>:TRUE>")
  if(MSVC)
    target_link_options(entropy PRIVATE
      "/MANIFESTDEPENDENCY:type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'")
  endif()
  target_link_libraries(entropy PRIVATE comctl32 shell32 user32 ws2_32)
endif()

target_include_directories(entropy PRIVATE
  "${CMAKE_BINARY_DIR}"
  "${entropy_APP_DIR}"
  "${entropy_LIB_DIR}"
  "${entropy_RES_DIR}"
  "${glad_INCLUDE_DIR}"
  "${iconfont_INCLUDE_DIR}"
  "${tinyfsm_INCLUDE_DIR}"
)
target_include_directories(entropy SYSTEM PRIVATE
  "${imgui_SRC_DIR}"
  "${imgui_SRC_DIR}/imgui"
  "${entropy_EXT_DIR}"
  "${entropy_EXT_DIR}/imGuIZMO.quat"
  "${entropy_EXT_DIR}/imgui-knobs"
  "${implot_SRC_DIR}"
  "${nanovg_SRC_DIR}"
)

entropy_copy_runtime_dlls(entropy)
set_target_properties(entropy PROPERTIES CXX_STANDARD 23 CXX_STANDARD_REQUIRED ON CXX_EXTENSIONS OFF)

if(APPLE)
  set(entropy_MACOS_APP_ICON "${entropy_RES_DIR}/icons/macOS/Entropy.icon")
  set(entropy_MACOS_APP_ICON_NAME "Entropy")
  entropy_configure_macos_runtime(entropy)
endif()
