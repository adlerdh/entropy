add_executable(TestUi
  GuiDataTests.cpp
  HeaderCommonTests.cpp
  IsosurfaceRangeModelTests.cpp
  KeyboardShortcutsTests.cpp
  LoadingStatusModelTests.cpp
  LinuxUiScaleTests.cpp
  NativeMessageDialogModelTests.cpp
  OpacityMixerModelTests.cpp
  SettingsModelTests.cpp
  StyleTests.cpp
  UpdateCheckTests.cpp
  ActiveImageSelectionModelTests.cpp
  UiControlsTests.cpp
  UiHeaderIncludeTests.cpp
  UiScaleManagerTests.cpp
  ViewOverlayModelTests.cpp
)

target_sources(TestUi PRIVATE
  "${entropy_UI_DIR}/GuiData.cpp"
  "${entropy_UI_DIR}/IsosurfaceRangeModel.cpp"
  "${entropy_UI_DIR}/windows/KeyboardShortcuts.cpp"
  "${entropy_UI_DIR}/dialogs/NativeMessageDialogModel.cpp"
  "${entropy_UI_DIR}/LinuxUiScale.cpp"
  "${entropy_UI_DIR}/Style.cpp"
  "${entropy_UI_DIR}/settings/SettingsModel.cpp"
  "${entropy_UI_DIR}/updates/UpdateCheck.cpp"
  "${entropy_UI_DIR}/UiScaleManager.cpp"
  "${entropy_UI_DIR}/widgets/ActiveImageSelectionModel.cpp"
  "${entropy_UI_DIR}/windows/OpacityMixerModel.cpp"
  "${entropy_UI_DIR}/windows/LoadingStatusModel.cpp"
  "${entropy_UI_DIR}/windows/ViewOverlayModel.cpp"
)

target_link_libraries(TestUi PRIVATE
  Catch2::Catch2WithMain
  Entropy::Common
  Entropy::Registration
  CURL::libcurl
  imgui
  nlohmann_json::nlohmann_json
  spdlog::spdlog
  entropy_warnings
)

entropy_enable_coverage_for_target(TestUi)
entropy_register_coverage_test_target(TestUi)

target_include_directories(TestUi PRIVATE
  "${entropy_APP_DIR}"
  "${entropy_LIB_DIR}"
  "${entropy_EXT_DIR}/glad/glad_gl_3.3_core/include"
  "${imgui_SRC_DIR}"
  "${imgui_SRC_DIR}/imgui"
  "${implot_SRC_DIR}"
)

set_target_properties(TestUi PROPERTIES
  CXX_STANDARD 23
  CXX_STANDARD_REQUIRED ON
  CXX_EXTENSIONS OFF
)

entropy_copy_runtime_dlls(TestUi)

catch_discover_tests(TestUi)
