add_executable(EntropyUiTests
  GuiDataTests.cpp
  LinuxUiScaleTests.cpp
  NativeMessageDialogModelTests.cpp
  OpacityMixerModelTests.cpp
  SettingsModelTests.cpp
  StyleTests.cpp
  UiControlsTests.cpp
  UiHeaderIncludeTests.cpp
  UiScaleManagerTests.cpp
  ViewOverlayModelTests.cpp
)

target_sources(EntropyUiTests PRIVATE
  "${entropy_UI_DIR}/GuiData.cpp"
  "${entropy_UI_DIR}/dialogs/NativeMessageDialogModel.cpp"
  "${entropy_UI_DIR}/LinuxUiScale.cpp"
  "${entropy_UI_DIR}/Style.cpp"
  "${entropy_UI_DIR}/settings/SettingsModel.cpp"
  "${entropy_UI_DIR}/UiScaleManager.cpp"
  "${entropy_UI_DIR}/windows/OpacityMixerModel.cpp"
  "${entropy_UI_DIR}/windows/ViewOverlayModel.cpp"
)

target_link_libraries(EntropyUiTests PRIVATE
  Catch2::Catch2WithMain
  Entropy::Common
  imgui
  spdlog::spdlog
  entropy_warnings
)

entropy_enable_coverage_for_target(EntropyUiTests)
entropy_register_coverage_test_target(EntropyUiTests)

target_include_directories(EntropyUiTests PRIVATE
  "${entropy_APP_DIR}"
  "${entropy_LIB_DIR}"
  "${entropy_EXT_DIR}/glad/glad_gl_3.3_core/include"
  "${imgui_SRC_DIR}"
  "${imgui_SRC_DIR}/imgui"
  "${implot_SRC_DIR}"
)

set_target_properties(EntropyUiTests PROPERTIES
  CXX_STANDARD 23
  CXX_STANDARD_REQUIRED ON
  CXX_EXTENSIONS OFF
)

entropy_copy_runtime_dlls(EntropyUiTests)

catch_discover_tests(EntropyUiTests)
