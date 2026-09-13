# Insight Toolkit (ITK) is an implementation detail of EntropyImage
find_package(ITK ${itk_VERSION} REQUIRED
  COMPONENTS ${entropy_ITK_COMPONENTS}
  HINTS "${itk_PREFIX}/build"
)
message(STATUS "Found ITK in ${ITK_DIR}")
entropy_configure_itk_directory()

add_library(EntropyImage STATIC)
add_library(Entropy::Image ALIAS EntropyImage)

target_sources(EntropyImage
  PRIVATE
    CtWindowing.cpp
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
    ImageWriter.cpp
    RegionStatistics.cpp
    SegUtil.cpp
    TimePlaybackController.cpp
    WarpInversion.cpp
  PUBLIC
    FILE_SET public_headers
    TYPE HEADERS
    BASE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/include"
    FILES
      include/image/CtWindowing.h
      include/image/DicomSeries.h
      include/image/Image.h
      include/image/ImageColorMap.h
      include/image/ImageDerivedData.h
      include/image/ImageHeader.h
      include/image/ImageHeaderOverrides.h
      include/image/ImageIoInfo.h
      include/image/ImageSettings.h
      include/image/ImageSpatialMetadata.h
      include/image/ImageTimeAxis.h
      include/image/ImageTransformations.h
      include/image/ImageTypes.h
      include/image/ImageUtility.h
      include/image/ImageWindowDefaults.h
      include/image/ImageWriter.h
      include/image/Isosurface.h
      include/image/RegionStatistics.h
      include/image/SegUtil.h
      include/image/TimePlaybackController.h
      include/image/WarpInversion.h
      include/image/external/TDigest.h
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
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
  PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}"
)
target_include_directories(EntropyImage SYSTEM PRIVATE ${ITK_INCLUDE_DIRS})
target_include_directories(EntropyImage SYSTEM PRIVATE
  "${itk_PREFIX}/build/Modules/ThirdParty/GDCM/src/gdcm/Source/Common"
  "${itk_PREFIX}/src/Modules/ThirdParty/GDCM/src/gdcm/Source/Common"
  "${itk_PREFIX}/src/Modules/ThirdParty/GDCM/src/gdcm/Source/DataDictionary"
  "${itk_PREFIX}/src/Modules/ThirdParty/GDCM/src/gdcm/Source/DataStructureAndEncodingDefinition"
  "${itk_PREFIX}/src/Modules/ThirdParty/GDCM/src/gdcm/Source/MediaStorageAndFileFormat"
)

target_link_libraries(EntropyImage
  PUBLIC
    Entropy::Common
  PRIVATE
    ${ITK_LIBRARIES}
    gdcmMSFF
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
