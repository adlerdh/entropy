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
    BASE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/include"
    FILES
      include/common/AABB.h
      include/common/AnatomicalLabels.h
      include/common/AsyncTasks.h
      include/common/ClipboardFormats.h
      include/common/ClipboardPayload.h
      include/common/CoordinateFrame.h
      include/common/DirectionMaps.h
      include/common/Exception.hpp
      include/common/Geometry.h
      include/common/HistogramSettings.h
      include/common/InputParams.h
      include/common/InputParser.h
      include/common/IntersectionTypes.h
      include/common/LoggingDefaults.h
      include/common/LoggingSettings.h
      include/common/MathFuncs.h
      include/common/ObjectCounter.hpp
      include/common/PublicTypes.h
      include/common/SegmentationTypes.h
      include/common/Types.h
      include/common/Utility.hpp
      include/common/UuidRange.h
      include/common/UuidUtility.h
      include/common/Viewport.h
      include/common/VnlMath.h
)

if(Entropy_ENABLE_IWYU OR Entropy_ENABLE_CLANG_TIDY)
  set_source_files_properties(
    "${entropy_EXT_DIR}/wrfranklin/pnpoly.cpp"
    PROPERTIES SKIP_LINTING ON
  )
endif()

target_include_directories(EntropyCommon
  PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
  PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}"
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
