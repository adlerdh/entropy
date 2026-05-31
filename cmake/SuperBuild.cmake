include(ExternalProject)

# Compute -G arg for configuring external projects with the same CMake generator:
if(CMAKE_EXTRA_GENERATOR)
  set(gen "${CMAKE_EXTRA_GENERATOR} - ${CMAKE_GENERATOR}")
else()
  set(gen "${CMAKE_GENERATOR}")
endif()

set(GIT_PROTOCOL "https")

# Detect whether the generator is multi-config
get_property(_isMultiConfig GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)

if(_isMultiConfig)
  if(NOT DEFINED SUPERBUILD_CONFIG)
    set(SUPERBUILD_CONFIG "Release" CACHE STRING "Build config for multi-config generators")
    set_property(CACHE SUPERBUILD_CONFIG PROPERTY STRINGS "Debug" "Release" "RelWithDebInfo" "MinSizeRel")
  endif()
  set(_cfg_arg "--config" "${SUPERBUILD_CONFIG}")
else()
  set(_cfg_arg "")
endif()

# For single-config generators, propagate CMAKE_BUILD_TYPE into externals
set(_ext_cmake_build_type_args)
if(NOT _isMultiConfig AND CMAKE_BUILD_TYPE)
  list(APPEND _ext_cmake_build_type_args -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE})
endif()

set(_ext_compiler_launcher_args)
foreach(_launcher_var IN ITEMS CMAKE_C_COMPILER_LAUNCHER CMAKE_CXX_COMPILER_LAUNCHER)
  if(DEFINED ${_launcher_var} AND NOT "${${_launcher_var}}" STREQUAL "")
    list(APPEND _ext_compiler_launcher_args -D${_launcher_var}:FILEPATH=${${_launcher_var}})
  endif()
endforeach()

set(_ext_cxx_std_args
  -DCMAKE_CXX_STANDARD=23
  -DCMAKE_CXX_STANDARD_REQUIRED=ON
  -DCMAKE_CXX_EXTENSIONS=OFF
)

message(STATUS "Adding external library Catch2 in ${catch2_PREFIX}")

ExternalProject_Add(catch2
  URL "https://github.com/catchorg/Catch2/archive/refs/tags/v${catch2_VERSION}.tar.gz"
  URL_HASH SHA256=18b3f70ac80fccc340d8c6ff0f339b2ae64944782f8d2fca2bd705cf47cadb79
  DOWNLOAD_NAME "catch2-v${catch2_VERSION}.tar.gz"
  DOWNLOAD_EXTRACT_TIMESTAMP false

  PREFIX "${catch2_PREFIX}"
  TMP_DIR "${catch2_PREFIX}/tmp"
  STAMP_DIR "${catch2_PREFIX}/stamp"
  DOWNLOAD_DIR "${catch2_PREFIX}/download"
  SOURCE_DIR "${catch2_PREFIX}/src"
  BINARY_DIR "${catch2_PREFIX}/build"
  INSTALL_DIR "${catch2_PREFIX}/install"

  CMAKE_ARGS
    ${_ext_cmake_build_type_args}
    ${_ext_compiler_launcher_args}
    ${_ext_cxx_std_args}
    -DCMAKE_INSTALL_PREFIX=<INSTALL_DIR>
    -DBUILD_TESTING:BOOL=OFF
    -DCATCH_BUILD_TESTING:BOOL=OFF
    -DCATCH_BUILD_EXAMPLES:BOOL=OFF
    -DCATCH_BUILD_EXTRA_TESTS:BOOL=OFF
    -DCATCH_BUILD_FUZZERS:BOOL=OFF
    -DCATCH_BUILD_SURROGATES:BOOL=OFF
    -DCATCH_DEVELOPMENT_BUILD:BOOL=OFF
    -DCATCH_ENABLE_WERROR:BOOL=OFF
    -DCATCH_INSTALL_DOCS:BOOL=OFF
    -DCATCH_INSTALL_EXTRAS:BOOL=ON

  BUILD_COMMAND ${CMAKE_COMMAND} --build <BINARY_DIR> ${_cfg_arg} --parallel ${SUPERBUILD_PARALLEL}
  INSTALL_COMMAND ${CMAKE_COMMAND} --build <BINARY_DIR> ${_cfg_arg} --target install

  CMAKE_GENERATOR ${gen}
)


message(STATUS "Adding external library CLI11 in ${cli11_PREFIX}")

ExternalProject_Add(cli11
  URL "https://github.com/CLIUtils/CLI11/archive/refs/tags/v${cli11_VERSION}.tar.gz"
  URL_HASH SHA256=c6ea6b2e5608b3ea8617999bd5f47420c71b2ebdb8dc4767c1034d1da5785711
  DOWNLOAD_NAME "cli11-v${cli11_VERSION}.tar.gz"
  DOWNLOAD_EXTRACT_TIMESTAMP false

  # Uncomment to instead clone Git repository:
  # GIT_REPOSITORY "${GIT_PROTOCOL}://github.com/CLIUtils/CLI11.git"
  # GIT_TAG "v${cli11_VERSION}"
  # GIT_PROGRESS true

  PREFIX "${cli11_PREFIX}"
  TMP_DIR "${cli11_PREFIX}/tmp"
  STAMP_DIR "${cli11_PREFIX}/stamp"
  DOWNLOAD_DIR "${cli11_PREFIX}/download"
  SOURCE_DIR "${cli11_PREFIX}/src"
  BINARY_DIR "${cli11_PREFIX}/build"
  INSTALL_DIR "${cli11_PREFIX}/install"

  CMAKE_ARGS
    ${_ext_cmake_build_type_args}
    ${_ext_compiler_launcher_args}
    -DCMAKE_INSTALL_PREFIX=<INSTALL_DIR>
    -DCLI11_BUILD_DOCS:BOOL=OFF
    -DCLI11_BUILD_EXAMPLES:BOOL=OFF
    -DCLI11_BUILD_TESTS:BOOL=OFF
    -DCLI11_INSTALL:BOOL=ON

  BUILD_COMMAND ${CMAKE_COMMAND} --build <BINARY_DIR> ${_cfg_arg} --parallel ${SUPERBUILD_PARALLEL}
  INSTALL_COMMAND ${CMAKE_COMMAND} --build <BINARY_DIR> ${_cfg_arg} --target install

  CMAKE_GENERATOR ${gen}
)


message(STATUS "Adding external library CMakeRC in ${cmakerc_PREFIX}")

set(PATCH_FILE "${cmakerc_PREFIX}/patch_cmakerc.cmake")
file(MAKE_DIRECTORY "${cmakerc_PREFIX}")

file(WRITE "${PATCH_FILE}" "
file(READ \"${cmakerc_PREFIX}/src/CMakeRC.cmake\" contents)
string(REPLACE \"cmake_minimum_required(VERSION 3.3)\" \"cmake_minimum_required(VERSION 3.5...4.2)\\ncmake_policy(VERSION 3.5...4.2)\" contents \"\${contents}\")
file(WRITE \"${cmakerc_PREFIX}/src/CMakeRC.cmake\" \"\${contents}\")
")

ExternalProject_Add(cmakerc
  URL "https://github.com/vector-of-bool/cmrc/archive/refs/tags/${cmakerc_VERSION}.tar.gz"
  URL_HASH SHA256=edad5faaa0bea1df124b5e8cb00bf0adbd2faeccecd3b5c146796cbcb8b5b71b
  DOWNLOAD_NAME "cmakerc-${cmakerc_VERSION}.tar.gz"
  DOWNLOAD_EXTRACT_TIMESTAMP false

  # Uncomment to instead clone Git repository:
  # GIT_REPOSITORY "${GIT_PROTOCOL}://github.com/vector-of-bool/cmrc.git"
  # GIT_TAG "9a80e17c75fca503a55a38ea011dcfaf2a830cc9" # tag: ${cmakerc_VERSION}
  # GIT_PROGRESS true

  PREFIX "${cmakerc_PREFIX}"
  TMP_DIR "${cmakerc_PREFIX}/tmp"
  STAMP_DIR "${cmakerc_PREFIX}/stamp"
  DOWNLOAD_DIR "${cmakerc_PREFIX}/download"
  SOURCE_DIR "${cmakerc_PREFIX}/src"
  BINARY_DIR "${cmakerc_PREFIX}/build"

  # Fix to make cmrc work with up to CMake 4.2
  PATCH_COMMAND ${CMAKE_COMMAND} -P "${PATCH_FILE}"

  CMAKE_ARGS
    ${_ext_cmake_build_type_args}
    ${_ext_compiler_launcher_args}
    -DBUILD_TESTS:BOOL=OFF

  CMAKE_GENERATOR ${gen}
  INSTALL_COMMAND "${CMAKE_COMMAND}" -E echo "Skipping CMakeRC install step"
)


message(STATUS "Adding external library GLFW in ${glfw_PREFIX}")

ExternalProject_Add(glfw
  URL "https://github.com/glfw/glfw/releases/download/3.4/glfw-${glfw_VERSION}.zip"
  URL_HASH SHA512=03de56a0599275ff57759ca19e8f69176058252b5e9976193cc3d9bb7b7b78b6a8dac6ed91de483d03c1b4807d21e1302e5e47c2f0c21e63becb4aba9d5affdc
  DOWNLOAD_EXTRACT_TIMESTAMP false

  # Uncomment to instead clone Git repository:
  # GIT_REPOSITORY "${GIT_PROTOCOL}://github.com/glfw/glfw.git"
  # GIT_TAG "7b6aead9fb88b3623e3b3725ebb42670cbe4c579" # tag: ${glfw_VERSION}
  # GIT_PROGRESS true

  PREFIX "${glfw_PREFIX}"
  TMP_DIR "${glfw_PREFIX}/tmp"
  STAMP_DIR "${glfw_PREFIX}/stamp"
  DOWNLOAD_DIR "${glfw_PREFIX}/download"
  SOURCE_DIR "${glfw_PREFIX}/src"
  BINARY_DIR "${glfw_PREFIX}/build"
  INSTALL_DIR "${glfw_PREFIX}/install"

  CMAKE_ARGS
    ${_ext_cmake_build_type_args}
    ${_ext_compiler_launcher_args}
    ${_ext_cxx_std_args}
    -DCMAKE_INSTALL_PREFIX=<INSTALL_DIR>
    -DBUILD_SHARED_LIBS:BOOL=${BUILD_SHARED_LIBS}
    -DGLFW_BUILD_DOCS:BOOL=OFF
    -DGLFW_BUILD_EXAMPLES:BOOL=OFF
    -DGLFW_BUILD_TESTS:BOOL=OFF
    -DGLFW_INSTALL:BOOL=ON

  BUILD_COMMAND ${CMAKE_COMMAND} --build <BINARY_DIR> ${_cfg_arg} --parallel ${SUPERBUILD_PARALLEL}
  INSTALL_COMMAND ${CMAKE_COMMAND} --build <BINARY_DIR> ${_cfg_arg} --target install

  CMAKE_GENERATOR ${gen}
)


message(STATUS "Adding external library GLM in ${glm_PREFIX}")

ExternalProject_Add(glm
  URL "https://github.com/g-truc/glm/archive/refs/tags/${glm_VERSION}.tar.gz"
  URL_HASH SHA256=6775e47231a446fd086d660ecc18bcd076531cfedd912fbd66e576b118607001
  DOWNLOAD_NAME "glm-${glm_VERSION}.tar.gz"
  DOWNLOAD_EXTRACT_TIMESTAMP false

  # Uncomment to instead clone Git repository:
  # GIT_REPOSITORY "${GIT_PROTOCOL}://github.com/g-truc/glm.git"
  # GIT_TAG "8d1fd52e5ab5590e2c81768ace50c72bae28f2ed" # tag: ${glm_VERSION}
  # GIT_PROGRESS true

  PREFIX "${glm_PREFIX}"
  TMP_DIR "${glm_PREFIX}/tmp"
  STAMP_DIR "${glm_PREFIX}/stamp"
  DOWNLOAD_DIR "${glm_PREFIX}/download"
  SOURCE_DIR "${glm_PREFIX}/src"
  BINARY_DIR "${glm_PREFIX}/build"
  INSTALL_DIR "${glm_PREFIX}/install"

  CMAKE_ARGS
    ${_ext_cmake_build_type_args}
    ${_ext_compiler_launcher_args}
    ${_ext_cxx_std_args}
    -DCMAKE_INSTALL_PREFIX=<INSTALL_DIR>
    -DBUILD_SHARED_LIBS:BOOL=${BUILD_SHARED_LIBS}
    -DGLM_BUILD_INSTALL:BOOL=ON
    -DGLM_BUILD_LIBRARY:BOOL=ON
    -DGLM_BUILD_TESTS:BOOL=OFF
    -DGLM_DISABLE_AUTO_DETECTION:BOOL=OFF
    -DGLM_ENABLE_CXX_11:BOOL=OFF
    -DGLM_ENABLE_CXX_14:BOOL=OFF
    -DGLM_ENABLE_CXX_17:BOOL=OFF
    -DGLM_ENABLE_CXX_20:BOOL=ON
    -DGLM_ENABLE_CXX_98:BOOL=OFF
    -DGLM_ENABLE_FAST_MATH:BOOL=ON
    -DGLM_ENABLE_LANG_EXTENSIONS:BOOL=OFF
    -DGLM_ENABLE_SIMD_AVX:BOOL=OFF
    -DGLM_ENABLE_SIMD_AVX2:BOOL=OFF
    -DGLM_ENABLE_SIMD_NEON:BOOL=OFF
    -DGLM_ENABLE_SIMD_SSE2:BOOL=OFF
    -DGLM_ENABLE_SIMD_SSE3:BOOL=OFF
    -DGLM_ENABLE_SIMD_SSE4_1:BOOL=OFF
    -DGLM_ENABLE_SIMD_SSE4_2:BOOL=OFF
    -DGLM_ENABLE_SIMD_SSSE3:BOOL=OFF
    -DGLM_FORCE_PURE:BOOL=OFF

  BUILD_COMMAND ${CMAKE_COMMAND} --build <BINARY_DIR> ${_cfg_arg} --parallel ${SUPERBUILD_PARALLEL}
  INSTALL_COMMAND ${CMAKE_COMMAND} --build <BINARY_DIR> ${_cfg_arg} --target install

  CMAKE_GENERATOR ${gen}
)


message(STATUS "Adding external library IconFontCppHeaders in ${iconfont_PREFIX}")

ExternalProject_Add(iconfont
  GIT_REPOSITORY "${GIT_PROTOCOL}://github.com/juliettef/IconFontCppHeaders.git"
  GIT_TAG "ef464d2fe5a568d30d7c88138e78d7fac7cfebc5" # branch: master
  GIT_PROGRESS true

  PREFIX "${iconfont_PREFIX}"
  TMP_DIR "${iconfont_PREFIX}/tmp"
  STAMP_DIR "${iconfont_PREFIX}/stamp"
  SOURCE_DIR "${iconfont_PREFIX}/src"
  BINARY_DIR "${iconfont_PREFIX}/build"

  CONFIGURE_COMMAND "${CMAKE_COMMAND}" -E echo "Skipping IconFontCppHeaders configure step"
  BUILD_COMMAND "${CMAKE_COMMAND}" -E echo "Skipping IconFontCppHeaders build step"
  INSTALL_COMMAND "${CMAKE_COMMAND}" -E echo "Skipping IconFontCppHeaders install step"
)


message(STATUS "Adding external library ImGui in ${imgui_PREFIX}")

ExternalProject_Add(imgui
  URL "https://github.com/ocornut/imgui/archive/refs/tags/v${imgui_VERSION}.tar.gz"
  URL_HASH SHA256=fecb33d33930e12ff53a34064e9d3a06c8f7c3e04408f14cd36c80e3faac863b
  DOWNLOAD_NAME "imgui-${imgui_VERSION}.tar.gz"
  DOWNLOAD_EXTRACT_TIMESTAMP false

  # GIT_REPOSITORY "${GIT_PROTOCOL}://github.com/ocornut/imgui.git"
  # GIT_TAG "8936b58fe26e8c3da834b8f60b06511d537b4c63" # tag: v${imgui_VERSION}
  # GIT_PROGRESS true

  PREFIX "${imgui_PREFIX}"
  TMP_DIR "${imgui_PREFIX}/tmp"
  STAMP_DIR "${imgui_PREFIX}/stamp"
  DOWNLOAD_DIR "${imgui_PREFIX}/download"
  SOURCE_DIR "${imgui_PREFIX}/src/imgui"
  BINARY_DIR "${imgui_PREFIX}/build"

  CONFIGURE_COMMAND "${CMAKE_COMMAND}" -E echo "Skipping ImGui configure step"
  BUILD_COMMAND "${CMAKE_COMMAND}" -E echo "Skipping ImGui build step"
  INSTALL_COMMAND "${CMAKE_COMMAND}" -E echo "Skipping ImGui install step"
)


message(STATUS "Adding external library ImPlot in ${implot_PREFIX}")

ExternalProject_Add(implot
  URL "https://github.com/epezent/implot/archive/refs/tags/v${implot_VERSION}.tar.gz"
  URL_HASH SHA256=0aa3ff4fb97e553608e6758e77980eedf01745628fe6c025e647f941ae674127
  DOWNLOAD_NAME "implot-${implot_VERSION}.tar.gz"
  DOWNLOAD_EXTRACT_TIMESTAMP false

  # GIT_REPOSITORY "${GIT_PROTOCOL}://github.com/epezent/implot.git"
  # GIT_TAG "4707b245fbcd69075b1a8a74fa8d2435561b3134" # tag: v${implot_VERSION}
  # GIT_PROGRESS true

  PREFIX "${implot_PREFIX}"
  TMP_DIR "${implot_PREFIX}/tmp"
  STAMP_DIR "${implot_PREFIX}/stamp"
  DOWNLOAD_DIR "${implot_PREFIX}/download"
  SOURCE_DIR "${implot_PREFIX}/src/implot"
  BINARY_DIR "${implot_PREFIX}/build"

  CONFIGURE_COMMAND "${CMAKE_COMMAND}" -E echo "Skipping ImPlot configure step"
  BUILD_COMMAND "${CMAKE_COMMAND}" -E echo "Skipping ImPlot build step"
  INSTALL_COMMAND "${CMAKE_COMMAND}" -E echo "Skipping ImPlot install step"
)

if(APPLE)
  set(ICONV_LINK_FLAG "-liconv")
else()
  set(ICONV_LINK_FLAG "")
endif()

message(STATUS "Adding external library ITK in ${itk_PREFIX}")

ExternalProject_Add(ITK
  URL "https://github.com/InsightSoftwareConsortium/ITK/releases/download/v${itk_VERSION}/InsightToolkit-${itk_VERSION}.tar.gz"
  URL_HASH SHA512=39e9003cc76a08f486c28e47df4b66944d1ba1e7917ad986ace84422acf2abc6956956929bebb08f37b04a9905f251eb941443a3d873c40852130aa1c189cf4b
  DOWNLOAD_EXTRACT_TIMESTAMP false

  # Uncomment to instead clone Git repository:
  # GIT_REPOSITORY "${GIT_PROTOCOL}://github.com/InsightSoftwareConsortium/ITK.git"
  # GIT_TAG "1fc47c7bec4ee133318c1892b7b745763a17d411" # tag: v${itk_VERSION}
  # GIT_PROGRESS true

  PREFIX "${itk_PREFIX}"
  TMP_DIR "${itk_PREFIX}/tmp"
  STAMP_DIR "${itk_PREFIX}/stamp"
  DOWNLOAD_DIR "${itk_PREFIX}/download"
  SOURCE_DIR "${itk_PREFIX}/src"
  BINARY_DIR "${itk_PREFIX}/build"

  CMAKE_ARGS
    ${_ext_cmake_build_type_args}
    ${_ext_compiler_launcher_args}
    -DCMAKE_EXE_LINKER_FLAGS=${ICONV_LINK_FLAG}
    -DCMAKE_SHARED_LINKER_FLAGS=${ICONV_LINK_FLAG}
    -DCMAKE_MODULE_LINKER_FLAGS=${ICONV_LINK_FLAG}
    -DBUILD_SHARED_LIBS:BOOL=${BUILD_SHARED_LIBS}
    -DBUILD_STATIC_LIBS:BOOL=${BUILD_STATIC_LIBS}
    -DBUILD_EXAMPLES:BOOL=OFF
    -DBUILD_TESTING:BOOL=OFF
    -DModule_ITKIOGDCM:BOOL=OFF

  BUILD_COMMAND ${CMAKE_COMMAND} --build <BINARY_DIR> ${_cfg_arg} --parallel ${SUPERBUILD_PARALLEL}
  INSTALL_COMMAND "${CMAKE_COMMAND}" -E echo "Skipping ITK install step"

  CMAKE_GENERATOR ${gen}
)


message(STATUS "Adding external library NanoVG in ${nanovg_PREFIX}")

ExternalProject_Add(NanoVG
  GIT_REPOSITORY "${GIT_PROTOCOL}://github.com/memononen/nanovg.git"
  GIT_TAG "f93799c078fa11ed61c078c65a53914c8782c00b" # branch: master
  GIT_PROGRESS true

  PREFIX "${nanovg_PREFIX}"
  TMP_DIR "${nanovg_PREFIX}/tmp"
  STAMP_DIR "${nanovg_PREFIX}/stamp"
  SOURCE_DIR "${nanovg_PREFIX}/src"
  BINARY_DIR "${nanovg_PREFIX}/build"

  CMAKE_ARGS ""
  CONFIGURE_COMMAND "${CMAKE_COMMAND}" -E echo "Skipping NanoVG configure step"
  BUILD_COMMAND "${CMAKE_COMMAND}" -E echo "Skipping NanoVG build step"
  INSTALL_COMMAND "${CMAKE_COMMAND}" -E echo "Skipping NanoVG install step"
)


message(STATUS "Adding external library Native File Dialog Extended in ${nativefiledialog_PREFIX}")

set(_nfd_portal OFF)
if(UNIX AND NOT APPLE)
  set(_nfd_portal ON)
endif()

ExternalProject_Add(nativefiledialog
  URL "https://github.com/btzy/nativefiledialog-extended/archive/refs/tags/v${nativefiledialog_VERSION}.tar.gz"
  URL_HASH SHA256=2fea19102cf4d5283a80fb87a784792166988e85bb92baa962d34f72b22dcc1a
  DOWNLOAD_NAME "nativefiledialog-v${nativefiledialog_VERSION}.tar.gz"
  DOWNLOAD_EXTRACT_TIMESTAMP false

  PREFIX "${nativefiledialog_PREFIX}"
  TMP_DIR "${nativefiledialog_PREFIX}/tmp"
  STAMP_DIR "${nativefiledialog_PREFIX}/stamp"
  DOWNLOAD_DIR "${nativefiledialog_PREFIX}/download"
  SOURCE_DIR "${nativefiledialog_PREFIX}/src"
  BINARY_DIR "${nativefiledialog_PREFIX}/build"
  INSTALL_DIR "${nativefiledialog_PREFIX}/install"

  CMAKE_ARGS
    ${_ext_cmake_build_type_args}
    ${_ext_compiler_launcher_args}
    -DCMAKE_INSTALL_PREFIX=<INSTALL_DIR>
    -DBUILD_SHARED_LIBS:BOOL=${BUILD_SHARED_LIBS}
    -DNFD_BUILD_TESTS:BOOL=OFF
    -DNFD_INSTALL:BOOL=ON
    -DNFD_PORTAL:BOOL=${_nfd_portal}
    -DNFD_APPEND_EXTENSION:BOOL=ON

  BUILD_COMMAND ${CMAKE_COMMAND} --build <BINARY_DIR> ${_cfg_arg} --parallel ${SUPERBUILD_PARALLEL}
  INSTALL_COMMAND ${CMAKE_COMMAND} --build <BINARY_DIR> ${_cfg_arg} --target install

  CMAKE_GENERATOR ${gen}
)


message(STATUS "Adding external library nlohmann_json in ${nlohmann_json_PREFIX}")

ExternalProject_Add(nlohmann_json
  URL "https://github.com/nlohmann/json/releases/download/v${nlohmann_json_VERSION}/json.tar.xz"
  URL_HASH SHA256=42f6e95cad6ec532fd372391373363b62a14af6d771056dbfc86160e6dfff7aa
  DOWNLOAD_NAME "nlohmann_json-v${nlohmann_json_VERSION}.tar.xz"
  DOWNLOAD_EXTRACT_TIMESTAMP false

  # Uncomment to instead clone Git repository:
  # GIT_REPOSITORY "${GIT_PROTOCOL}://github.com/nlohmann/json.git"
  # GIT_TAG "55f93686c01528224f448c19128836e7df245f72" # tag: v${nlohmann_json_VERSION}
  # GIT_PROGRESS true

  PREFIX "${nlohmann_json_PREFIX}"
  TMP_DIR "${nlohmann_json_PREFIX}/tmp"
  STAMP_DIR "${nlohmann_json_PREFIX}/stamp"
  DOWNLOAD_DIR "${nlohmann_json_PREFIX}/download"
  SOURCE_DIR "${nlohmann_json_PREFIX}/src"
  BINARY_DIR "${nlohmann_json_PREFIX}/build"
  INSTALL_DIR "${nlohmann_json_PREFIX}/install"

  CMAKE_ARGS
    ${_ext_cmake_build_type_args}
    ${_ext_compiler_launcher_args}
    -DCMAKE_INSTALL_PREFIX=<INSTALL_DIR>
    -DBUILD_TESTING:BOOL=OFF
    -DJSON_BuildTests:BOOL=OFF
    -DJSON_CI:BOOL=OFF
    -DJSON_Diagnostic_Positions:BOOL=OFF
    -DJSON_Diagnostics:BOOL=OFF
    -DJSON_DisableEnumSerialization:BOOL=OFF
    -DJSON_FastTests:BOOL=OFF
    -DJSON_GlobalUDLs:BOOL=ON
    -DJSON_ImplicitConversions:BOOL=ON
    -DJSON_Install:BOOL=ON
    -DJSON_LegacyDiscardedValueComparison:BOOL=OFF
    -DJSON_MultipleHeaders:BOOL=ON
    -DJSON_SystemInclude:BOOL=OFF
    -DJSON_Valgrind:BOOL=OFF

  BUILD_COMMAND ${CMAKE_COMMAND} --build <BINARY_DIR> ${_cfg_arg} --parallel ${SUPERBUILD_PARALLEL}
  INSTALL_COMMAND ${CMAKE_COMMAND} --build <BINARY_DIR> ${_cfg_arg} --target install

  CMAKE_GENERATOR ${gen}
)



message(STATUS "Adding external library spdlog in ${spdlog_PREFIX}")

ExternalProject_Add(spdlog
  URL "https://github.com/gabime/spdlog/archive/refs/tags/v${spdlog_VERSION}.tar.gz"
  URL_HASH SHA512=8df117055d19ff21c9c9951881c7bdf27cc0866ea3a4aa0614b2c3939cedceab94ac9abaa63dc4312b51562b27d708cb2f014c68c603fd1c1051d3ed5c1c3087
  DOWNLOAD_NAME "spdlog-v${spdlog_VERSION}.tar.gz"
  DOWNLOAD_EXTRACT_TIMESTAMP false

  # Uncomment to instead clone Git repository:
  # GIT_REPOSITORY "${GIT_PROTOCOL}://github.com/gabime/spdlog.git"
  # GIT_TAG "79524ddd08a4ec981b7fea76afd08ee05f83755d" # tag: v${spdlog_VERSION}
  # GIT_PROGRESS true

  PREFIX "${spdlog_PREFIX}"
  TMP_DIR "${spdlog_PREFIX}/tmp"
  STAMP_DIR "${spdlog_PREFIX}/stamp"
  DOWNLOAD_DIR "${spdlog_PREFIX}/download"
  SOURCE_DIR "${spdlog_PREFIX}/src"
  BINARY_DIR "${spdlog_PREFIX}/build"
  INSTALL_DIR "${spdlog_PREFIX}/install"

  CMAKE_ARGS
    ${_ext_cmake_build_type_args}
    ${_ext_compiler_launcher_args}
    ${_ext_cxx_std_args}
    -DCMAKE_INSTALL_PREFIX=<INSTALL_DIR>
    -DSPDLOG_BUILD_ALL:BOOL=OFF
    -DSPDLOG_BUILD_BENCH:BOOL=OFF
    -DSPDLOG_BUILD_EXAMPLE:BOOL=OFF
    -DSPDLOG_BUILD_EXAMPLE_HO:BOOL=OFF
    -DSPDLOG_BUILD_PIC:BOOL=${BUILD_SHARED_LIBS}
    -DSPDLOG_BUILD_SHARED:BOOL=${BUILD_SHARED_LIBS}
    -DSPDLOG_BUILD_TESTS:BOOL=OFF
    -DSPDLOG_BUILD_TESTS_HO:BOOL=OFF
    -DSPDLOG_BUILD_WARNINGS:BOOL=OFF
    -DSPDLOG_CLOCK_COARSE:BOOL=OFF
    -DSPDLOG_DISABLE_DEFAULT_LOGGER:BOOL=OFF
    -DSPDLOG_ENABLE_PCH:BOOL=OFF
    -DSPDLOG_FMT_EXTERNAL:BOOL=OFF
    -DSPDLOG_FMT_EXTERNAL_HO:BOOL=OFF
    -DSPDLOG_FWRITE_UNLOCKED:BOOL=ON
    -DSPDLOG_INSTALL:BOOL=ON
    -DSPDLOG_NO_ATOMIC_LEVELS:BOOL=OFF
    -DSPDLOG_NO_EXCEPTIONS:BOOL=OFF
    -DSPDLOG_NO_THREAD_ID:BOOL=OFF
    -DSPDLOG_NO_TLS:BOOL=OFF
    -DSPDLOG_PREVENT_CHILD_FD:BOOL=OFF
    -DSPDLOG_SANITIZE_ADDRESS:BOOL=OFF
    -DSPDLOG_SANITIZE_THREAD:BOOL=OFF
    -DSPDLOG_SYSTEM_INCLUDES:BOOL=OFF
    -DSPDLOG_TIDY:BOOL=OFF
    -DSPDLOG_USE_STD_FORMAT:BOOL=OFF
    -DSPDLOG_WCHAR_CONSOLE:BOOL=OFF
    -DSPDLOG_WCHAR_FILENAMES:BOOL=OFF
    -DSPDLOG_WCHAR_SUPPORT:BOOL=OFF

  BUILD_COMMAND ${CMAKE_COMMAND} --build <BINARY_DIR> ${_cfg_arg} --parallel ${SUPERBUILD_PARALLEL}
  INSTALL_COMMAND ${CMAKE_COMMAND} --build <BINARY_DIR> ${_cfg_arg} --target install

  CMAKE_GENERATOR ${gen}
)


message(STATUS "Adding external library stduuid in ${stduuid_PREFIX}")

ExternalProject_Add(stduuid
  URL "https://github.com/mariusbancila/stduuid/archive/refs/tags/v${stduuid_VERSION}.tar.gz"
  URL_HASH SHA256=b1176597e789531c38481acbbed2a6894ad419aab0979c10410d59eb0ebf40d3
  DOWNLOAD_NAME "stduuid-v${stduuid_VERSION}.tar.gz"
  DOWNLOAD_EXTRACT_TIMESTAMP false

  # Uncomment to instead clone Git repository:
  # GIT_REPOSITORY "${GIT_PROTOCOL}://github.com/mariusbancila/stduuid.git"
  # GIT_TAG "3afe7193facd5d674de709fccc44d5055e144d7a" # tag: v${stduuid_VERSION}
  # GIT_PROGRESS true

  PREFIX "${stduuid_PREFIX}"
  TMP_DIR "${stduuid_PREFIX}/tmp"
  STAMP_DIR "${stduuid_PREFIX}/stamp"
  DOWNLOAD_DIR "${stduuid_PREFIX}/download"
  SOURCE_DIR "${stduuid_PREFIX}/src"
  BINARY_DIR "${stduuid_PREFIX}/build"
  INSTALL_DIR "${stduuid_PREFIX}/install"

  CMAKE_ARGS
    ${_ext_cmake_build_type_args}
    ${_ext_compiler_launcher_args}
    -DCMAKE_INSTALL_PREFIX=<INSTALL_DIR>
    -DUUID_BUILD_TESTS:BOOL=OFF
    -DUUID_ENABLE_INSTALL:BOOL=ON
    -DUUID_SYSTEM_GENERATOR:BOOL=OFF
    -DUUID_TIME_GENERATOR:BOOL=OFF
    -DUUID_USING_CXX20_SPAN:BOOL=OFF

  BUILD_COMMAND ${CMAKE_COMMAND} --build <BINARY_DIR> ${_cfg_arg} --parallel ${SUPERBUILD_PARALLEL}
  INSTALL_COMMAND ${CMAKE_COMMAND} --build <BINARY_DIR> ${_cfg_arg} --target install

  CMAKE_GENERATOR ${gen}
)


message(STATUS "Adding external library TinyFSM in ${tinyfsm_PREFIX}")

ExternalProject_Add(tinyfsm
  GIT_REPOSITORY "${GIT_PROTOCOL}://github.com/digint/tinyfsm.git"
  GIT_TAG "01908cab0397fcdadb0a14e9a3187c308e2708ca" # tag: v${tinyfsm_VERSION}
  GIT_PROGRESS true

  PREFIX "${tinyfsm_PREFIX}"
  TMP_DIR "${tinyfsm_PREFIX}/tmp"
  STAMP_DIR "${tinyfsm_PREFIX}/stamp"
  SOURCE_DIR "${tinyfsm_PREFIX}/src"
  BINARY_DIR "${tinyfsm_PREFIX}/build"

  CONFIGURE_COMMAND "${CMAKE_COMMAND}" -E echo "Skipping TinyFSM configure step"
  BUILD_COMMAND "${CMAKE_COMMAND}" -E echo "Skipping TinyFSM build step"
  INSTALL_COMMAND "${CMAKE_COMMAND}" -E echo "Skipping TinyFSM install step"
)
