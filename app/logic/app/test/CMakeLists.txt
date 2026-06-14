add_executable(EntropyAppSettingsTests
  AppPathsTests.cpp
  UserPreferencesTests.cpp
)

target_sources(EntropyAppSettingsTests PRIVATE
  "${entropy_APP_DIR}/logic/app/ParcellationLabelTable.cpp"
  "${entropy_APP_DIR}/logic/app/Settings.cpp"
  "${entropy_APP_DIR}/logic/app/UserPreferences.cpp"
)

if(APPLE)
  target_sources(EntropyAppSettingsTests PRIVATE
    "${entropy_APP_DIR}/logic/app/AppPathsMac.mm"
  )
else()
  target_sources(EntropyAppSettingsTests PRIVATE
    "${entropy_APP_DIR}/logic/app/AppPaths.cpp"
  )
endif()

target_link_libraries(EntropyAppSettingsTests PRIVATE
  Catch2::Catch2WithMain
  Entropy::Common
  nlohmann_json::nlohmann_json
  spdlog::spdlog
  entropy_warnings
)

if(APPLE)
  target_link_libraries(EntropyAppSettingsTests PRIVATE
    ${COCOA_LIBRARY}
  )
endif()

entropy_enable_coverage_for_target(EntropyAppSettingsTests)
entropy_register_coverage_test_target(EntropyAppSettingsTests)

target_include_directories(EntropyAppSettingsTests PRIVATE
  "${entropy_APP_DIR}"
  "${entropy_EXT_DIR}"
  "${entropy_EXT_DIR}/glad/glad_gl_3.3_core/include"
)

set_target_properties(EntropyAppSettingsTests PROPERTIES
  CXX_STANDARD 23
  CXX_STANDARD_REQUIRED ON
  CXX_EXTENSIONS OFF
)

entropy_copy_runtime_dlls(EntropyAppSettingsTests)

catch_discover_tests(EntropyAppSettingsTests)
