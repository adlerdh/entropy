if(BUILD_TESTING)
  find_package(Catch2 ${catch2_VERSION} REQUIRED HINTS "${catch2_PREFIX}/install")
  message(STATUS "Using Catch2 in ${Catch2_DIR}")
  include(Catch)
endif()

find_package(CLI11 ${cli11_VERSION} REQUIRED HINTS "${cli11_PREFIX}/install")
message(STATUS "Using CLI11 in ${CLI11_DIR}")

include("${cmakerc_PREFIX}/src/CMakeRC.cmake")

# macOS uses the SDK libcurl. Windows and Linux use the SuperBuild-pinned package.
if(APPLE)
  execute_process(
    COMMAND xcrun --sdk macosx --show-sdk-path
    OUTPUT_VARIABLE entropy_MACOS_SDK_PATH
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET)
  set(CURL_INCLUDE_DIR "${entropy_MACOS_SDK_PATH}/usr/include")
  find_library(CURL_LIBRARY curl PATHS "${entropy_MACOS_SDK_PATH}/usr/lib" NO_DEFAULT_PATH)
  set(CURL_INCLUDE_DIR "${CURL_INCLUDE_DIR}" CACHE PATH "macOS SDK curl include directory" FORCE)
  set(CURL_LIBRARY "${CURL_LIBRARY}" CACHE FILEPATH "macOS SDK curl library" FORCE)
  set(CURL_LIBRARY_RELEASE "${CURL_LIBRARY}" CACHE FILEPATH "macOS SDK curl release library" FORCE)
  set(CURL_LIBRARY_DEBUG "CURL_LIBRARY_DEBUG-NOTFOUND" CACHE FILEPATH "macOS SDK curl debug library" FORCE)
  find_package(CURL REQUIRED MODULE COMPONENTS HTTPS)
else()
  find_package(CURL ${curl_VERSION} REQUIRED CONFIG COMPONENTS HTTPS
    HINTS "${curl_PREFIX}/install" NO_DEFAULT_PATH)
endif()

if(CURL_DIR AND NOT CURL_DIR MATCHES "-NOTFOUND$")
  message(STATUS "Using CURL in ${CURL_DIR}")
else()
  message(STATUS "Using CURL ${CURL_VERSION_STRING}")
endif()

find_package(glfw3 ${glfw_VERSION} REQUIRED HINTS "${glfw_PREFIX}/install")
message(STATUS "Using GLFW in ${glfw3_DIR}")
set(glfw_INCLUDE_DIR "${glfw_PREFIX}/install/include" CACHE PATH "glfw include directory")

find_package(glm ${glm_VERSION} REQUIRED CONFIG HINTS "${glm_PREFIX}/install")
message(STATUS "Using GLM in ${glm_DIR}")

set(iconfont_INCLUDE_DIR "${iconfont_PREFIX}/src" CACHE PATH "IconFontCppHeaders include directory")

find_package(nfd REQUIRED HINTS "${nativefiledialog_PREFIX}/install")
message(STATUS "Using Native File Dialog Extended in ${nfd_DIR}")

find_package(nlohmann_json ${nlohmann_json_VERSION} REQUIRED HINTS "${nlohmann_json_PREFIX}/install")
message(STATUS "Using nlohmann_json in ${nlohmann_json_DIR}")

find_package(Qt6 ${qtbase_VERSION} REQUIRED COMPONENTS Core
  HINTS "${qtbase_PREFIX}/install/lib/cmake" NO_DEFAULT_PATH)
message(STATUS "Using Qt6Core in ${Qt6Core_DIR}")

find_package(spdlog ${spdlog_VERSION} REQUIRED HINTS "${spdlog_PREFIX}/install")
message(STATUS "Using spdlog in ${spdlog_DIR}")

find_package(stduuid REQUIRED HINTS "${stduuid_PREFIX}/install")
message(STATUS "Using stduuid in ${stduuid_DIR}")

set(tinyfsm_INCLUDE_DIR "${tinyfsm_PREFIX}/src/include" CACHE PATH "TinyFSM include directory")

set(entropy_VTK_COMPONENTS
  CommonCore
  CommonDataModel
  CommonExecutionModel
  FiltersCore
  FiltersGeneral
  FiltersSources
)
find_package(VTK ${vtk_VERSION} REQUIRED COMPONENTS ${entropy_VTK_COMPONENTS}
  HINTS "${vtk_PREFIX}/install" NO_DEFAULT_PATH)
message(STATUS "Using VTK in ${VTK_DIR}")

include(CheckCXXSourceCompiles)
check_cxx_source_compiles([=[
  #include <expected>
  #ifndef __cpp_lib_expected
  #error "std::expected not available"
  #endif
  int main() {
    std::expected<int,int> e(1);
    return *e;
  }
]=] ENTROPY_HAVE_STD_EXPECTED)

if(ENTROPY_HAVE_STD_EXPECTED)
  message(STATUS "Using std::expected")
else()
  message(FATAL_ERROR "std::expected is required. Use a C++23 standard library that provides <expected>.")
endif()

# ITK's legacy use file is still required for automatic IO-factory
# registration. Reclassify the include paths it adds so warnings in ITK do not
# become first-party build failures.
macro(entropy_configure_itk_directory)
  include("${ITK_USE_FILE}")
  get_property(entropy_directory_includes DIRECTORY PROPERTY INCLUDE_DIRECTORIES)
  list(REMOVE_ITEM entropy_directory_includes ${ITK_INCLUDE_DIRS})
  set_property(DIRECTORY PROPERTY INCLUDE_DIRECTORIES "${entropy_directory_includes}")
  include_directories(SYSTEM BEFORE ${ITK_INCLUDE_DIRS})
  unset(entropy_directory_includes)
endmacro()
