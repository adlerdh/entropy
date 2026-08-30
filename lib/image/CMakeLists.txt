# Insight Toolkit (ITK) is an implementation detail of EntropyImage
find_package(ITK ${itk_VERSION} REQUIRED
  COMPONENTS ${entropy_ITK_COMPONENTS}
  HINTS "${itk_PREFIX}/build")
message(STATUS "Found ITK in ${ITK_DIR}")
entropy_configure_itk_directory()

add_library(EntropyImage STATIC)
add_library(Entropy::Image ALIAS EntropyImage)

target_sources(EntropyImage PRIVATE
  DicomSeries.cpp
  ImageComponentBuffers.cpp
  Image.cpp
  ImageColorMap.cpp
  ImageDerivedData.cpp
  ImageHeader.cpp
  ImageQuantiles.cpp
  ImageIoInfo.cpp
  ImageSettings.cpp
  ImageSpatialMetadata.cpp
  ImageTimeAxis.cpp
  ImageTransformations.cpp
  ImageUtility.cpp
  ImageWindowDefaults.cpp
  SegUtil.cpp
  TimePlaybackController.cpp
  WarpInversion.cpp
)

if(Entropy_ENABLE_IWYU OR Entropy_ENABLE_CLANG_TIDY)
  # Analyzer Clang frontends do not understand GCC 13's std::expected setup here
  set_source_files_properties(
    ImageDerivedData.cpp
    WarpInversion.cpp
    PROPERTIES SKIP_LINTING ON
  )
endif()

set_source_files_properties(WarpInversion.cpp PROPERTIES
  COMPILE_OPTIONS "$<$<COMPILE_LANG_AND_ID:CXX,GNU,Clang,AppleClang>:-Wno-extra-semi>"
)

target_include_directories(EntropyImage
  PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/.."
)
target_include_directories(EntropyImage SYSTEM PRIVATE ${ITK_INCLUDE_DIRS})

target_link_libraries(EntropyImage
  PUBLIC
    Entropy::Common
  PRIVATE
    ${ITK_LIBRARIES}
    entropy_warnings
)

entropy_enable_coverage_for_target(EntropyImage)

set_target_properties(EntropyImage PROPERTIES
  CXX_STANDARD 23
  CXX_STANDARD_REQUIRED ON
  CXX_EXTENSIONS OFF
)

if(BUILD_TESTING)
  add_subdirectory(test)
endif()
