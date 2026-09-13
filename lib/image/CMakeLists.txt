# Insight Toolkit (ITK) is an implementation detail of EntropyImage
find_package(ITK ${itk_VERSION} REQUIRED
  COMPONENTS ${entropy_ITK_COMPONENTS}
  HINTS "${itk_PREFIX}/build"
)
message(STATUS "Found ITK in ${ITK_DIR}")
entropy_configure_itk_directory()

add_library(EntropyImage STATIC)
add_library(Entropy::Image ALIAS EntropyImage)

target_sources(EntropyImage PRIVATE
  image/CtWindowing.cpp
  image/DicomSeries.cpp
  image/ImageComponentBuffers.cpp
  image/Image.cpp
  image/ImageColorMap.cpp
  image/ImageDerivedData.cpp
  image/ImageHeader.cpp
  image/ImageQuantiles.cpp
  image/ImageIoInfo.cpp
  image/ImageSettings.cpp
  image/ImageSpatialMetadata.cpp
  image/ImageTimeAxis.cpp
  image/ImageTransformations.cpp
  image/ImageUtility.cpp
  image/ImageWindowDefaults.cpp
  image/ImageWriter.cpp
  image/RegionStatistics.cpp
  image/SegUtil.cpp
  image/TimePlaybackController.cpp
  image/WarpInversion.cpp
  PUBLIC
    FILE_SET public_headers
    TYPE HEADERS
    BASE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}"
    FILES
      image/CtWindowing.h
      image/DicomSeries.h
      image/Image.h
      image/ImageColorMap.h
      image/ImageDerivedData.h
      image/ImageHeader.h
      image/ImageHeaderOverrides.h
      image/ImageIoInfo.h
      image/ImageSettings.h
      image/ImageSpatialMetadata.h
      image/ImageTimeAxis.h
      image/ImageTransformations.h
      image/ImageTypes.h
      image/ImageUtility.h
      image/ImageWindowDefaults.h
      image/ImageWriter.h
      image/Isosurface.h
      image/RegionStatistics.h
      image/SegUtil.h
      image/TimePlaybackController.h
      image/WarpInversion.h
      image/external/TDigest.h
)

if(Entropy_ENABLE_IWYU OR Entropy_ENABLE_CLANG_TIDY)
  # Analyzer Clang frontends do not understand GCC 13's std::expected setup here
  set_source_files_properties(
    image/ImageDerivedData.cpp
    image/WarpInversion.cpp
    PROPERTIES SKIP_LINTING ON
  )
endif()

set_source_files_properties(image/WarpInversion.cpp PROPERTIES
  COMPILE_OPTIONS "$<$<COMPILE_LANG_AND_ID:CXX,GNU,Clang,AppleClang>:-Wno-extra-semi>"
)

target_include_directories(EntropyImage
  PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>"
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
  PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/image"
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
