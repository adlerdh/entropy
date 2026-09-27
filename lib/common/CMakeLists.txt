add_library(EntropyCommon STATIC)
add_library(Entropy::Common ALIAS EntropyCommon)

target_sources(EntropyCommon
  PRIVATE
    AnatomicalLabels.cpp
    ClipboardFormats.cpp
    CoordinateFrame.cpp
    DirectionMaps.cpp
    "${entropy_EXT_DIR}/wrfranklin/pnpoly.cpp"
    InputParams.cpp
    InputParser.cpp
    LoggingSettings.cpp
    MathFuncs.cpp
    Types.cpp
    UuidUtility.cpp
    Viewport.cpp
  PUBLIC
    FILE_SET public_headers
    TYPE HEADERS
    BASE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/.."
    FILES
      AABB.h
      AnatomicalLabels.h
      AsyncTasks.h
      ClipboardFormats.h
      ClipboardPayload.h
      ColorMapDefaults.h
      CoordinateFrame.h
      DirectionMaps.h
      Exception.hpp
      Geometry.h
      HistogramSettings.h
      InputParams.h
      InputParser.h
      IntersectionTypes.h
      LoggingDefaults.h
      LoggingSettings.h
      MathFuncs.h
      ObjectCounter.hpp
      PublicTypes.h
      SegmentationTypes.h
      Types.h
      Utility.hpp
      UuidRange.h
      UuidUtility.h
      Viewport.h
      VnlMath.h
)

if(Entropy_ENABLE_IWYU OR Entropy_ENABLE_CLANG_TIDY)
  set_source_files_properties(
    "${entropy_EXT_DIR}/wrfranklin/pnpoly.cpp"
    PROPERTIES SKIP_LINTING ON
  )
endif()

target_include_directories(EntropyCommon
  PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>"
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
