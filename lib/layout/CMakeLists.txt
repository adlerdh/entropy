add_library(EntropyLayout STATIC)
add_library(Entropy::Layout ALIAS EntropyLayout)

target_sources(EntropyLayout PRIVATE
  ImageIndexMapping.cpp
  LayoutFileSerialization.cpp
  LayoutKindInfo.cpp
  LayoutSpecJson.cpp
  SyncGroupIndexMap.cpp
)

target_include_directories(EntropyLayout
  PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/.."
)

target_link_libraries(EntropyLayout
  PUBLIC
    Entropy::Viewer
    nlohmann_json::nlohmann_json
  PRIVATE
    entropy_logging_level
    entropy_warnings
    spdlog::spdlog
)

entropy_enable_coverage_for_target(EntropyLayout)

set_target_properties(EntropyLayout PROPERTIES
  CXX_STANDARD 23
  CXX_STANDARD_REQUIRED ON
  CXX_EXTENSIONS OFF
)

if(BUILD_TESTING)
  add_subdirectory(test)
endif()
