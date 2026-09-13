add_library(EntropyLayout STATIC)
add_library(Entropy::Layout ALIAS EntropyLayout)

target_sources(EntropyLayout
  PRIVATE
    layout/ImageIndexMapping.cpp
    layout/LayoutFileSerialization.cpp
    layout/LayoutKindInfo.cpp
    layout/LayoutSpecJson.cpp
    layout/SyncGroupIndexMap.cpp
  PUBLIC
    FILE_SET public_headers
    TYPE HEADERS
    BASE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}"
    FILES
      layout/ImageIndexMapping.h
      layout/LayoutFileSerialization.h
      layout/LayoutKindInfo.h
      layout/LayoutSpec.h
      layout/LayoutSpecJson.h
      layout/SyncGroupIndexMap.h
)

target_include_directories(EntropyLayout
  PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>"
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
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
