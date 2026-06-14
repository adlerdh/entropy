add_executable(EntropyLayoutTests
  "${entropy_APP_DIR}/logic/serialization/test/ProjectSerializationTests.cpp"
  "${entropy_APP_DIR}/windowing/test/LayoutFileSerializationTests.cpp"
  "${entropy_APP_DIR}/windowing/test/LayoutSpecJsonTests.cpp"
)

target_sources(EntropyLayoutTests PRIVATE
  "${entropy_APP_DIR}/logic/serialization/ProjectSerialization.cpp"
  "${entropy_APP_DIR}/windowing/LayoutFileSerialization.cpp"
  "${entropy_APP_DIR}/windowing/LayoutSpecJson.cpp"
  "${entropy_EXT_DIR}/safeclib/strerrorlen_s.cpp"
)

target_link_libraries(EntropyLayoutTests PRIVATE
  Catch2::Catch2WithMain
  Entropy::Common
  nlohmann_json::nlohmann_json
  spdlog::spdlog
  entropy_warnings
)

entropy_enable_coverage_for_target(EntropyLayoutTests)
entropy_register_coverage_test_target(EntropyLayoutTests)

target_include_directories(EntropyLayoutTests PRIVATE
  "${entropy_APP_DIR}"
  "${entropy_EXT_DIR}"
)

set_target_properties(EntropyLayoutTests PROPERTIES
  CXX_STANDARD 23
  CXX_STANDARD_REQUIRED ON
  CXX_EXTENSIONS OFF
)

entropy_copy_runtime_dlls(EntropyLayoutTests)

catch_discover_tests(EntropyLayoutTests)

add_executable(EntropySyncTests
  "${entropy_APP_DIR}/logic/sync/test/EntropyInstanceSyncProtocolTests.cpp"
  "${entropy_APP_DIR}/logic/sync/test/ItkSnapSyncProtocolTests.cpp"
)

target_sources(EntropySyncTests PRIVATE
  "${entropy_APP_DIR}/logic/sync/EntropyInstanceSyncProtocol.cpp"
  "${entropy_APP_DIR}/logic/sync/ItkSnapSyncProtocol.cpp"
)

target_link_libraries(EntropySyncTests PRIVATE
  Catch2::Catch2WithMain
  Entropy::Common
  nlohmann_json::nlohmann_json
  entropy_warnings
)

entropy_enable_coverage_for_target(EntropySyncTests)
entropy_register_coverage_test_target(EntropySyncTests)

target_include_directories(EntropySyncTests PRIVATE
  "${entropy_APP_DIR}"
)

set_target_properties(EntropySyncTests PROPERTIES
  CXX_STANDARD 23
  CXX_STANDARD_REQUIRED ON
  CXX_EXTENSIONS OFF
)

entropy_copy_runtime_dlls(EntropySyncTests)

catch_discover_tests(EntropySyncTests)

add_executable(EntropyUiTests
  "${entropy_APP_DIR}/rendering/test/ScaleBarGeometryTests.cpp"
  "${entropy_UI_DIR}/test/NativeMessageDialogModelTests.cpp"
  "${entropy_UI_DIR}/test/OpacityMixerModelTests.cpp"
  "${entropy_UI_DIR}/test/SettingsModelTests.cpp"
  "${entropy_UI_DIR}/test/StyleTests.cpp"
  "${entropy_UI_DIR}/test/UiHeaderIncludeTests.cpp"
  "${entropy_UI_DIR}/test/UiScaleManagerTests.cpp"
  "${entropy_UI_DIR}/test/ViewOverlayModelTests.cpp"
  "${entropy_APP_DIR}/windowing/test/LinuxUiScaleTests.cpp"
)

target_sources(EntropyUiTests PRIVATE
  "${entropy_APP_DIR}/rendering/ScaleBarGeometry.cpp"
  "${entropy_UI_DIR}/dialogs/NativeMessageDialogModel.cpp"
  "${entropy_UI_DIR}/Style.cpp"
  "${entropy_UI_DIR}/settings/SettingsModel.cpp"
  "${entropy_UI_DIR}/UiScaleManager.cpp"
  "${entropy_UI_DIR}/windows/OpacityMixerModel.cpp"
  "${entropy_UI_DIR}/windows/ViewOverlayModel.cpp"
  "${entropy_APP_DIR}/windowing/LinuxUiScale.cpp"
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
