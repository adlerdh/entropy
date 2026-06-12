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

add_executable(EntropyUiTests
  "${entropy_APP_DIR}/rendering/test/ScaleBarGeometryTests.cpp"
  "${entropy_APP_DIR}/ui/test/UiScaleManagerTests.cpp"
  "${entropy_APP_DIR}/windowing/test/LinuxUiScaleTests.cpp"
)

target_sources(EntropyUiTests PRIVATE
  "${entropy_APP_DIR}/rendering/ScaleBarGeometry.cpp"
  "${entropy_APP_DIR}/ui/UiScaleManager.cpp"
  "${entropy_APP_DIR}/windowing/LinuxUiScale.cpp"
)

target_link_libraries(EntropyUiTests PRIVATE
  Catch2::Catch2WithMain
  Entropy::Common
  imgui
  spdlog::spdlog
  entropy_warnings
)

target_include_directories(EntropyUiTests PRIVATE
  "${entropy_APP_DIR}"
  "${imgui_SRC_DIR}"
  "${imgui_SRC_DIR}/imgui"
)

set_target_properties(EntropyUiTests PROPERTIES
  CXX_STANDARD 23
  CXX_STANDARD_REQUIRED ON
  CXX_EXTENSIONS OFF
)

entropy_copy_runtime_dlls(EntropyUiTests)

catch_discover_tests(EntropyUiTests)
