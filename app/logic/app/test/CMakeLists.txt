add_executable(TestAppSettings
  AnnotationSerializationTests.cpp
  AppPathsTests.cpp
  ImageScaleInteractionTests.cpp
  ImageSelectionPolicyTests.cpp
  LoadingStatusItemsTests.cpp
  ProjectSnapshotComparisonTests.cpp
  ProjectSnapshotSettingsTests.cpp
  StackTraceTests.cpp
  UserPreferencesTests.cpp
  WindowTitleStatusTests.cpp
)

target_sources(TestAppSettings PRIVATE
  "${entropy_APP_DIR}/logic/app/ImageScaleInteraction.cpp"
  "${entropy_APP_DIR}/logic/app/ImageSelectionPolicy.cpp"
  "${entropy_APP_DIR}/logic/app/LoadingStatusItems.cpp"
  "${entropy_APP_DIR}/logic/app/ParcellationLabelTable.cpp"
  "${entropy_APP_DIR}/logic/app/ProjectSnapshotComparison.cpp"
  "${entropy_APP_DIR}/logic/app/Settings.cpp"
  "${entropy_APP_DIR}/logic/app/StackTrace.cpp"
  "${entropy_APP_DIR}/logic/app/UserPreferences.cpp"
  "${entropy_APP_DIR}/logic/app/WindowTitleStatus.cpp"
)

if(APPLE)
  target_sources(TestAppSettings PRIVATE
    "${entropy_APP_DIR}/logic/app/AppPathsMac.mm"
  )
else()
  target_sources(TestAppSettings PRIVATE
    "${entropy_APP_DIR}/logic/app/AppPaths.cpp"
  )
endif()

target_link_libraries(TestAppSettings PRIVATE
  Catch2::Catch2WithMain
  Entropy::Layout
  Entropy::Registration
  Entropy::Viewer
  nlohmann_json::nlohmann_json
  entropy_warnings
)

if(APPLE)
  target_link_libraries(TestAppSettings PRIVATE
    ${COCOA_LIBRARY}
  )
endif()

entropy_enable_coverage_for_target(TestAppSettings)
entropy_register_coverage_test_target(TestAppSettings)

target_include_directories(TestAppSettings PRIVATE
  "${entropy_APP_DIR}"
  "${entropy_EXT_DIR}"
  "${entropy_EXT_DIR}/glad/glad_gl_3.3_core/include"
  "${imgui_SRC_DIR}"
  "${imgui_SRC_DIR}/imgui"
)

set_target_properties(TestAppSettings PROPERTIES
  CXX_STANDARD 23
  CXX_STANDARD_REQUIRED ON
  CXX_EXTENSIONS OFF
)

entropy_copy_runtime_dlls(TestAppSettings)

catch_discover_tests(TestAppSettings)
