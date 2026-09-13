add_library(EntropyCommon STATIC)
add_library(Entropy::Common ALIAS EntropyCommon)

target_sources(EntropyCommon
  PRIVATE
    common/AnatomicalLabels.cpp
    common/ClipboardFormats.cpp
    common/CoordinateFrame.cpp
    common/DirectionMaps.cpp
    "${entropy_EXT_DIR}/wrfranklin/pnpoly.cpp"
    common/InputParams.cpp
    common/InputParser.cpp
    common/LoggingSettings.cpp
    common/MathFuncs.cpp
    common/Types.cpp
    common/UuidUtility.cpp
    common/Viewport.cpp
  PUBLIC
    FILE_SET public_headers
    TYPE HEADERS
    BASE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}"
    FILES
      common/AABB.h
      common/AnatomicalLabels.h
      common/AsyncTasks.h
      common/ClipboardFormats.h
      common/ClipboardPayload.h
      common/CoordinateFrame.h
      common/DirectionMaps.h
      common/Exception.hpp
      common/Geometry.h
      common/HistogramSettings.h
      common/InputParams.h
      common/InputParser.h
      common/IntersectionTypes.h
      common/LoggingDefaults.h
      common/LoggingSettings.h
      common/MathFuncs.h
      common/ObjectCounter.hpp
      common/PublicTypes.h
      common/SegmentationTypes.h
      common/Types.h
      common/Utility.hpp
      common/UuidRange.h
      common/UuidUtility.h
      common/Viewport.h
      common/VnlMath.h
)

if(Entropy_ENABLE_IWYU OR Entropy_ENABLE_CLANG_TIDY)
  set_source_files_properties(
    "${entropy_EXT_DIR}/wrfranklin/pnpoly.cpp"
    PROPERTIES SKIP_LINTING ON
  )
endif()

target_include_directories(EntropyCommon
  PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>"
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
  PRIVATE
    "${CMAKE_BINARY_DIR}" # for BuildStamp.h
    "${entropy_EXT_DIR}"
    "${iconfont_INCLUDE_DIR}"
)

target_link_libraries(EntropyCommon
  PUBLIC
    entropy_logging_level
    glm::glm
    spdlog::spdlog
    stduuid
  PRIVATE
    CLI11::CLI11
    entropy_warnings
)

entropy_enable_coverage_for_target(EntropyCommon)

set_target_properties(EntropyCommon PROPERTIES
  CXX_STANDARD 23
  CXX_STANDARD_REQUIRED ON
  CXX_EXTENSIONS OFF
)

if(BUILD_TESTING)
  add_subdirectory(test)
endif()
