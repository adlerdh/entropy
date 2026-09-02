set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(CMAKE_VERBOSE_MAKEFILE "Show full native build commands" OFF)
option(Entropy_ENABLE_TRACE_LOGGING "Compile trace-level logging calls into Entropy" OFF)
set(Entropy_SUPERBUILD_PARALLEL "" CACHE STRING
  "Parallel level for ExternalProject builds; empty defers to CMAKE_BUILD_PARALLEL_LEVEL or the native build tool default")
set(Entropy_SUPERBUILD_CONFIG "Release" CACHE STRING "Build config for multi-config SuperBuild generators")
set_property(CACHE Entropy_SUPERBUILD_CONFIG PROPERTY STRINGS Debug Release RelWithDebInfo MinSizeRel)

set(entropy_APP_DIR "${CMAKE_SOURCE_DIR}/app")
set(entropy_LIB_DIR "${CMAKE_SOURCE_DIR}/lib")
set(entropy_UI_DIR "${entropy_LIB_DIR}/ui")
set(entropy_RES_DIR "${CMAKE_SOURCE_DIR}/res")
set(entropy_EXT_DIR "${CMAKE_SOURCE_DIR}/external")
set(entropy_FIREANTS_BRIDGE_DIR "${entropy_LIB_DIR}/registration/fireants_bridge")
set(entropy_ABOUT_ICON_RESOURCE_PATH "res/icons/Linux/hicolor/128x128/apps/io.github.adlerdh.entropy.png")

if(WIN32)
  set(entropy_WINDOWS_APP_ICON "${entropy_RES_DIR}/icons/Windows/Entropy.ico")
  if(NOT EXISTS "${entropy_WINDOWS_APP_ICON}")
    message(FATAL_ERROR "Windows application icon not found: ${entropy_WINDOWS_APP_ICON}")
  endif()
endif()

set(entropy_LINUX_PACKAGE_PLATFORM_LABEL_DEFAULT "Linux")
if(UNIX AND NOT APPLE AND EXISTS "/etc/os-release")
  file(STRINGS "/etc/os-release" entropy_OS_RELEASE_LINES REGEX "^(ID|VERSION_ID)=")
  foreach(entropy_OS_RELEASE_LINE IN LISTS entropy_OS_RELEASE_LINES)
    if(entropy_OS_RELEASE_LINE MATCHES "^ID=\"?([A-Za-z0-9._-]+)\"?$")
      set(entropy_LINUX_OS_ID "${CMAKE_MATCH_1}")
    elseif(entropy_OS_RELEASE_LINE MATCHES "^VERSION_ID=\"?([A-Za-z0-9._-]+)\"?$")
      set(entropy_LINUX_OS_VERSION_ID "${CMAKE_MATCH_1}")
    endif()
  endforeach()

  if(entropy_LINUX_OS_ID)
    if(entropy_LINUX_OS_ID STREQUAL "ubuntu")
      set(entropy_LINUX_OS_LABEL "Ubuntu")
    elseif(entropy_LINUX_OS_ID STREQUAL "fedora")
      set(entropy_LINUX_OS_LABEL "Fedora")
    elseif(entropy_LINUX_OS_ID STREQUAL "debian")
      set(entropy_LINUX_OS_LABEL "Debian")
    else()
      string(SUBSTRING "${entropy_LINUX_OS_ID}" 0 1 entropy_LINUX_OS_ID_FIRST)
      string(SUBSTRING "${entropy_LINUX_OS_ID}" 1 -1 entropy_LINUX_OS_ID_REST)
      string(TOUPPER "${entropy_LINUX_OS_ID_FIRST}" entropy_LINUX_OS_ID_FIRST)
      set(entropy_LINUX_OS_LABEL "${entropy_LINUX_OS_ID_FIRST}${entropy_LINUX_OS_ID_REST}")
    endif()

    if(entropy_LINUX_OS_VERSION_ID)
      set(entropy_LINUX_PACKAGE_PLATFORM_LABEL_DEFAULT
        "${entropy_LINUX_OS_LABEL}-${entropy_LINUX_OS_VERSION_ID}")
    else()
      set(entropy_LINUX_PACKAGE_PLATFORM_LABEL_DEFAULT "${entropy_LINUX_OS_LABEL}")
    endif()
  endif()
endif()

set(Entropy_LINUX_PACKAGE_PLATFORM_LABEL "${entropy_LINUX_PACKAGE_PLATFORM_LABEL_DEFAULT}"
  CACHE STRING "Platform label used in generated Linux package filenames")
set(Entropy_LINUX_CPACK_GENERATORS "DEB;TGZ" CACHE STRING "CPack generators used for Linux packages")

get_property(entropy_IS_MULTI_CONFIG GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)
if(entropy_IS_MULTI_CONFIG)
  set(CMAKE_CONFIGURATION_TYPES "Debug;Release;RelWithDebInfo;MinSizeRel" CACHE STRING "Configs" FORCE)
elseif(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE "RelWithDebInfo" CACHE STRING "Choose the type of build" FORCE)
  set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS Debug Release MinSizeRel RelWithDebInfo)
  message(STATUS "Setting build type to '${CMAKE_BUILD_TYPE}' as none was specified.")
endif()

if(MSVC AND CMAKE_GENERATOR MATCHES "Ninja")
  set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "Embedded" CACHE STRING
    "Use object-embedded debug information for Ninja/MSVC builds to avoid parallel PDB writer contention" FORCE)
endif()

if(MSVC)
  foreach(flag_variable IN ITEMS
      CMAKE_C_FLAGS_DEBUG
      CMAKE_CXX_FLAGS_DEBUG
      CMAKE_C_FLAGS_RELWITHDEBINFO
      CMAKE_CXX_FLAGS_RELWITHDEBINFO)
    if(DEFINED ${flag_variable} AND NOT "${${flag_variable}}" MATCHES "(^| )/FS($| )")
      string(APPEND ${flag_variable} " /FS")
    endif()
  endforeach()
endif()

if(CMAKE_BINARY_DIR STREQUAL PROJECT_SOURCE_DIR)
  message(FATAL_ERROR "Source and build directories are the same.")
endif()

option(BUILD_SHARED_LIBS "Build using shared libraries" OFF)
set(entropy_STATIC_BUNDLED_DEPENDENCIES_DEFAULT OFF)
if(UNIX)
  set(entropy_STATIC_BUNDLED_DEPENDENCIES_DEFAULT ON)
endif()
option(Entropy_STATIC_BUNDLED_DEPENDENCIES
  "Build bundled third-party dependencies as static libraries where practical; Qt and system libraries stay dynamic"
  ${entropy_STATIC_BUNDLED_DEPENDENCIES_DEFAULT})

if(BUILD_SHARED_LIBS)
  set(BUILD_STATIC_LIBS OFF)
else()
  set(BUILD_STATIC_LIBS ON)
endif()

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")

message(STATUS "CMAKE_CXX_FLAGS_DEBUG=${CMAKE_CXX_FLAGS_DEBUG}")
message(STATUS "CMAKE_CXX_FLAGS_RELEASE=${CMAKE_CXX_FLAGS_RELEASE}")
message(STATUS "Entropy_ENABLE_TRACE_LOGGING=${Entropy_ENABLE_TRACE_LOGGING}")
message(STATUS "BUILD_SHARED_LIBS=${BUILD_SHARED_LIBS}")
message(STATUS "BUILD_STATIC_LIBS=${BUILD_STATIC_LIBS}")
message(STATUS "Entropy_STATIC_BUNDLED_DEPENDENCIES=${Entropy_STATIC_BUNDLED_DEPENDENCIES}")
