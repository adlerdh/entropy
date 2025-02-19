include(ExternalProject)

if(NOT USE_SYSTEM_ITK)
  set(ITK_PREFIX "${CMAKE_CURRENT_BINARY_DIR}/ITK")
  cmake_path(APPEND ITK_DIR ${ITK_PREFIX} "src/ITK-build")
  message(STATUS "ITK library not provided on system: Cloning external ITK into ${ITK_PREFIX}")

  ExternalProject_Add(ITK
    GIT_REPOSITORY "${GIT_PROTOCOL}://github.com/InsightSoftwareConsortium/ITK.git"
    GIT_TAG "898def645183e6a2d3293058ade451ec416c4514" # v5.4.2
    GIT_PROGRESS true
    PREFIX "${ITK_PREFIX}"
    CMAKE_ARGS
      -DCMAKE_BUILD_TYPE:STRING=${CMAKE_BUILD_TYPE}
      -DBUILD_SHARED_LIBS:BOOL=${BUILD_SHARED_LIBS}
      -DBUILD_STATIC_LIBS:BOOL=${BUILD_STATIC_LIBS}
      -DBUILD_EXAMPLES:BOOL=OFF
      -DBUILD_TESTING:BOOL=OFF
    INSTALL_COMMAND "${CMAKE_COMMAND}" -E echo "Skipping install step")
else()
  find_package(ITK ${MIN_ITK_VERSION} REQUIRED)

  if(ITK_FOUND)
    message(STATUS "Using system ITK in ${ITK_DIR}")
    include(${ITK_USE_FILE})
  else()
    message(FATAL_ERROR "System ITK not found: Please set ITK_DIR")
  endif()
endif()

if(NOT USE_SYSTEM_Boost)
  set(Boost_PREFIX "${CMAKE_CURRENT_BINARY_DIR}/Boost")
  cmake_path(APPEND Boost_INCLUDE_DIR ${Boost_PREFIX} "src/Boost")
  message(STATUS "Boost library not provided on system: Cloning external Boost into ${Boost_PREFIX}")

  if(WIN32)
    set(Boost_BUILD_CMD "bootstrap.bat && b2 headers")
    separate_arguments(Boost_BUILD_CMD_PARSED WINDOWS_COMMAND "${Boost_BUILD_CMD}")
  elseif(UNIX OR APPLE)
    set(Boost_BUILD_CMD "./bootstrap.sh && ./b2 headers")
    separate_arguments(Boost_BUILD_CMD_PARSED UNIX_COMMAND "${Boost_BUILD_CMD}")
  endif()

  ExternalProject_Add(Boost
    GIT_REPOSITORY "${GIT_PROTOCOL}://github.com/boostorg/boost.git"
    GIT_TAG "c89e6267665516192015a9e40955e154466f4f68" # boost-1.87.0
    GIT_SUBMODULES_RECURSE true
    GIT_PROGRESS true
    PREFIX "${Boost_PREFIX}"
    CONFIGURE_COMMAND ""
    BUILD_IN_SOURCE true
    BUILD_COMMAND ${Boost_BUILD_CMD_PARSED}
    INSTALL_COMMAND "${CMAKE_COMMAND}" -E echo "Skipping install step")
else()
  find_package(Boost ${MIN_Boost_VERSION} REQUIRED)

  if(Boost_FOUND)
    message(STATUS "Using system Boost in ${Boost_INCLUDE_DIR}")
    set(Boost_USE_MULTITHREADED ON)
    set(Boost_USE_STATIC_LIBS ${BUILD_STATIC_LIBS})
  else()
    message(FATAL_ERROR "System Boost not found: Please set Boost_INCLUDE_DIR")
  endif()
endif()
