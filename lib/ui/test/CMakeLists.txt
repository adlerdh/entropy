add_executable(TestUi
  GuiDataTests.cpp
  HeaderCommonTests.cpp
  InspectionWindowSizingTests.cpp
  InputLoadErrorDialogTests.cpp
  IsosurfaceRangeModelTests.cpp
  KeyboardShortcutsTests.cpp
  LoadingStatusModelTests.cpp
  LinuxUiScaleTests.cpp
  NativeMessageDialogModelTests.cpp
  OpacityMixerModelTests.cpp
  RegionStatisticsExportTests.cpp
  RegionStatisticsControllerTests.cpp
  SettingsModelTests.cpp
  StyleTests.cpp
  UpdateCheckTests.cpp
  ActiveImageSelectionModelTests.cpp
  ExportJobServiceTests.cpp
  UiControlsTests.cpp
  UiHeaderIncludeTests.cpp
  UiScaleManagerTests.cpp
  ViewOverlayModelTests.cpp
)

if(Entropy_ENABLE_IWYU OR Entropy_ENABLE_CLANG_TIDY)
  set_source_files_properties(
    UiHeaderIncludeTests.cpp
    PROPERTIES SKIP_LINTING ON
  )
endif()

target_sources(TestUi PRIVATE
  "${entropy_UI_DIR}/ui/GuiData.cpp"
  "${entropy_UI_DIR}/ui/ExportJobService.cpp"
  "${entropy_UI_DIR}/ui/IsosurfaceRangeModel.cpp"
  "${entropy_UI_DIR}/ui/windows/KeyboardShortcuts.cpp"
  "${entropy_UI_DIR}/ui/windows/InspectionWindowSizing.cpp"
  "${entropy_UI_DIR}/ui/dialogs/InputLoadErrorDialog.cpp"
  "${entropy_UI_DIR}/ui/dialogs/NativeMessageDialogModel.cpp"
  "${entropy_UI_DIR}/ui/LinuxUiScale.cpp"
  "${entropy_UI_DIR}/ui/Style.cpp"
  "${entropy_UI_DIR}/ui/settings/SettingsModel.cpp"
  "${entropy_UI_DIR}/ui/updates/UpdateCheck.cpp"
  "${entropy_UI_DIR}/ui/UiScaleManager.cpp"
  "${entropy_UI_DIR}/ui/widgets/ActiveImageSelectionModel.cpp"
  "${entropy_UI_DIR}/ui/windows/OpacityMixerModel.cpp"
  "${entropy_UI_DIR}/ui/RegionStatisticsExport.cpp"
  "${entropy_UI_DIR}/ui/windows/RegionStatisticsController.cpp"
  "${entropy_UI_DIR}/ui/windows/LoadingStatusModel.cpp"
  "${entropy_UI_DIR}/ui/windows/ViewOverlayModel.cpp"
)

target_link_libraries(TestUi PRIVATE
  Catch2::Catch2WithMain
  Entropy::Common
  Entropy::Image
  Entropy::Layout
  Entropy::Mesh
  Entropy::Registration
  Entropy::Rendering
  Entropy::Viewer
  CURL::libcurl
  imgui
  nlohmann_json::nlohmann_json
  entropy_warnings
)

entropy_enable_coverage_for_target(TestUi)
entropy_register_coverage_test_target(TestUi)

target_include_directories(TestUi PRIVATE
  "${entropy_APP_DIR}"
  "${entropy_UI_DIR}"
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
