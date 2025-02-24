include(ExternalProject)

# Compute -G arg for configuring external projects with the same CMake generator:
if(CMAKE_EXTRA_GENERATOR)
  set(gen "${CMAKE_EXTRA_GENERATOR} - ${CMAKE_GENERATOR}")
else()
  set(gen "${CMAKE_GENERATOR}")
endif()

set(GIT_PROTOCOL "https")



message(STATUS "Adding external library argparse in ${argparse_PREFIX}")

ExternalProject_Add(argparse
  URL "https://github.com/p-ranav/argparse/archive/refs/tags/v${argparse_VERSION}.zip"
  URL_HASH SHA256=14c1a0e975d6877dfeaf52a1e79e54f70169a847e29c7e13aa7fe68a3d0ecbf1
  DOWNLOAD_NAME "argparse-v${argparse_VERSION}.zip"
  DOWNLOAD_EXTRACT_TIMESTAMP false

  # Uncomment to instead clone Git repository:
  # GIT_REPOSITORY "${GIT_PROTOCOL}://github.com/p-ranav/argparse.git"
  # GIT_TAG "3eda91b2e1ce7d569f84ba295507c4cd8fd96910" # tag: v${argparse_VERSION}
  # GIT_PROGRESS true

  PREFIX "${argparse_PREFIX}"
  TMP_DIR "${argparse_PREFIX}/tmp"
  STAMP_DIR "${argparse_PREFIX}/stamp"
  DOWNLOAD_DIR "${argparse_PREFIX}/download"
  SOURCE_DIR "${argparse_PREFIX}/src"
  BINARY_DIR "${argparse_PREFIX}/build"
  INSTALL_DIR "${argparse_PREFIX}/install"

  CMAKE_ARGS
    -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
    -DCMAKE_INSTALL_PREFIX=<INSTALL_DIR>
    -DARGPARSE_BUILD_SAMPLES:BOOL=OFF
    -DARGPARSE_BUILD_TESTS:BOOL=OFF
    -DARGPARSE_INSTALL:BOOL=ON

  CMAKE_GENERATOR ${gen}
)


message(STATUS "Adding external library Boost in ${boost_PREFIX}")

set(boost_bootstrap_CMD)
set(boost_b2_CMD)

if(WIN32)
  set(boost_bootstrap_CMD bootstrap.bat)
  set(boost_b2_CMD b2.exe)
elseif(UNIX OR APPLE)
  set(boost_bootstrap_CMD ./bootstrap.sh)
  set(boost_b2_CMD ./b2)
endif()

ExternalProject_Add(Boost
  URL "https://github.com/boostorg/boost/releases/download/boost-${boost_VERSION}/boost-${boost_VERSION}-b2-nodocs.tar.xz"
  URL_HASH SHA256=3abd7a51118a5dd74673b25e0a3f0a4ab1752d8d618f4b8cea84a603aeecc680
  DOWNLOAD_EXTRACT_TIMESTAMP false

  # Uncomment to instead clone Git repositories:
  # GIT_REPOSITORY "${GIT_PROTOCOL}://github.com/boostorg/boost.git"
  # GIT_TAG "c89e6267665516192015a9e40955e154466f4f68" # tag: ${boost_VERSION}
  # GIT_SUBMODULES_RECURSE true
  # GIT_PROGRESS true

  PREFIX "${boost_PREFIX}"
  TMP_DIR "${boost_PREFIX}/tmp"
  STAMP_DIR "${boost_PREFIX}/stamp"
  DOWNLOAD_DIR "${boost_PREFIX}/download"
  SOURCE_DIR "${boost_PREFIX}/src"

  CONFIGURE_COMMAND ${boost_bootstrap_CMD}
  BUILD_IN_SOURCE true
  BUILD_COMMAND ${boost_b2_CMD} headers
  INSTALL_COMMAND "${CMAKE_COMMAND}" -E echo "Skipping Boost install step"
)


message(STATUS "Adding external library CMakeRC in ${cmakerc_PREFIX}")

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

  CMAKE_ARGS
    -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
    -DBUILD_TESTS:BOOL=OFF

  CMAKE_GENERATOR ${gen}
  INSTALL_COMMAND "${CMAKE_COMMAND}" -E echo "Skipping CMakeRC install step"
)



message(STATUS "Adding external library ghc::filesystem in ${ghc_filesystem_PREFIX}")

ExternalProject_Add(ghc_filesystem
  URL "https://github.com/gulrak/filesystem/archive/refs/tags/v${ghc_filesystem_VERSION}.tar.gz"
  URL_HASH SHA256=e783f672e49de7c5a237a0cea905ed51012da55c04fbacab397161976efc8472
  DOWNLOAD_NAME "ghc_filesystem-v${ghc_filesystem_VERSION}.tar.gz"
  DOWNLOAD_EXTRACT_TIMESTAMP false

  # Uncomment to instead clone Git repository:
  # GIT_REPOSITORY "${GIT_PROTOCOL}://github.com/gulrak/filesystem.git"
  # GIT_TAG "8a2edd6d92ed820521d42c94d179462bf06b5ed3" # tag: v${ghc_filesystem_VERSION}
  # GIT_PROGRESS true

  PREFIX "${ghc_filesystem_PREFIX}"
  TMP_DIR "${ghc_filesystem_PREFIX}/tmp"
  STAMP_DIR "${ghc_filesystem_PREFIX}/stamp"
  DOWNLOAD_DIR "${ghc_filesystem_PREFIX}/download"
  SOURCE_DIR "${ghc_filesystem_PREFIX}/src"
  BINARY_DIR "${ghc_filesystem_PREFIX}/build"
  INSTALL_DIR "${ghc_filesystem_PREFIX}/install"

  CMAKE_ARGS
    -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
    -DCMAKE_INSTALL_PREFIX=<INSTALL_DIR>
    -DGHC_FILESYSTEM_BUILD_EXAMPLES:BOOL=OFF
    -DGHC_FILESYSTEM_BUILD_STD_TESTING:BOOL=OFF
    -DGHC_FILESYSTEM_BUILD_TESTING:BOOL=OFF
    -DGHC_FILESYSTEM_WITH_INSTALL:BOOL=ON

  CMAKE_GENERATOR ${gen}
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
    -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
    -DCMAKE_INSTALL_PREFIX=<INSTALL_DIR>
    -DBUILD_SHARED_LIBS:BOOL=${BUILD_SHARED_LIBS}
    -DGLFW_BUILD_DOCS:BOOL=OFF
    -DGLFW_BUILD_EXAMPLES:BOOL=OFF
    -DGLFW_BUILD_TESTS:BOOL=OFF
    -DGLFW_INSTALL:BOOL=ON

  CMAKE_GENERATOR ${gen}
)


message(STATUS "Adding external library GLM in ${glm_PREFIX}")

ExternalProject_Add(glm
  URL "https://github.com/g-truc/glm/archive/refs/tags/${glm_VERSION}.tar.gz"
  URL_HASH SHA256=9f3174561fd26904b23f0db5e560971cbf9b3cbda0b280f04d5c379d03bf234c
  DOWNLOAD_NAME "glm-${glm_VERSION}.tar.gz"
  DOWNLOAD_EXTRACT_TIMESTAMP false

  # Uncomment to instead clone Git repository:
  # GIT_REPOSITORY "${GIT_PROTOCOL}://github.com/g-truc/glm.git"
  # GIT_TAG "0af55ccecd98d4e5a8d1fad7de25ba429d60e863" # tag: ${glm_VERSION}
  # GIT_PROGRESS true

  PREFIX "${glm_PREFIX}"
  TMP_DIR "${glm_PREFIX}/tmp"
  STAMP_DIR "${glm_PREFIX}/stamp"
  DOWNLOAD_DIR "${glm_PREFIX}/download"
  SOURCE_DIR "${glm_PREFIX}/src"
  BINARY_DIR "${glm_PREFIX}/build"
  INSTALL_DIR "${glm_PREFIX}/install"

  CMAKE_ARGS
    -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
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
    -DGLM_ENABLE_SIMD_AVX:BOOL=ON
    -DGLM_ENABLE_SIMD_AVX2:BOOL=ON
    -DGLM_ENABLE_SIMD_NEON:BOOL=ON
    -DGLM_ENABLE_SIMD_SSE2:BOOL=ON
    -DGLM_ENABLE_SIMD_SSE3:BOOL=ON
    -DGLM_ENABLE_SIMD_SSE4_1:BOOL=ON
    -DGLM_ENABLE_SIMD_SSE4_2:BOOL=ON
    -DGLM_ENABLE_SIMD_SSSE3:BOOL=ON
    -DGLM_FORCE_PURE:BOOL=OFF

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

  CONFIGURE_COMMAND "${CMAKE_COMMAND}" -E echo "Skipping IconFontCppHeaders configure step"
  BUILD_COMMAND "${CMAKE_COMMAND}" -E echo "Skipping IconFontCppHeaders build step"
  INSTALL_COMMAND "${CMAKE_COMMAND}" -E echo "Skipping IconFontCppHeaders install step"
)


message(STATUS "Adding external library ImGui in ${imgui_PREFIX}")

ExternalProject_Add(imgui
  URL "https://github.com/ocornut/imgui/archive/refs/tags/v${imgui_VERSION}.tar.gz"
  URL_HASH SHA256=db3a2e02bfd6c269adf0968950573053d002f40bdfb9ef2e4a90bce804b0f286
  DOWNLOAD_NAME "imgui-${imgui_VERSION}.tar.gz"
  DOWNLOAD_EXTRACT_TIMESTAMP false

  # GIT_REPOSITORY "${GIT_PROTOCOL}://github.com/ocornut/imgui.git"
  # GIT_TAG "dbb5eeaadffb6a3ba6a60de1290312e5802dba5a" # tag: v${imgui_VERSION}
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
  URL_HASH SHA256=961df327d8a756304d1b0a67316eebdb1111d13d559f0d3415114ec0eb30abd1
  DOWNLOAD_NAME "implot-${implot_VERSION}.tar.gz"
  DOWNLOAD_EXTRACT_TIMESTAMP false

  # GIT_REPOSITORY "${GIT_PROTOCOL}://github.com/epezent/implot.git"
  # GIT_TAG "18c72431f8265e2b0b5378a3a73d8a883b2175ff" # tag: v${implot_VERSION}
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


message(STATUS "Adding external library ITK in ${itk_PREFIX}")

ExternalProject_Add(ITK
  URL "https://github.com/InsightSoftwareConsortium/ITK/releases/download/v${itk_VERSION}/InsightToolkit-${itk_VERSION}.tar.gz"
  URL_HASH SHA512=440d5962336ae7ba68e1efcabd78db8f10437db27da077a65731024d2fd94c588468678d0af8d8be1bfdb45dc90a88ace85ae9e1fabf77fb4172f3cb7cc27a3c
  DOWNLOAD_EXTRACT_TIMESTAMP false

  # Uncomment to instead clone Git repository:
  # GIT_REPOSITORY "${GIT_PROTOCOL}://github.com/InsightSoftwareConsortium/ITK.git"
  # GIT_TAG "898def645183e6a2d3293058ade451ec416c4514" # tag: v${itk_VERSION}
  # GIT_PROGRESS true

  PREFIX "${itk_PREFIX}"
  TMP_DIR "${itk_PREFIX}/tmp"
  STAMP_DIR "${itk_PREFIX}/stamp"
  DOWNLOAD_DIR "${itk_PREFIX}/download"
  SOURCE_DIR "${itk_PREFIX}/src"
  BINARY_DIR "${itk_PREFIX}/build"

  CMAKE_ARGS
    -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
    -DBUILD_SHARED_LIBS:BOOL=${BUILD_SHARED_LIBS}
    -DBUILD_STATIC_LIBS:BOOL=${BUILD_STATIC_LIBS}
    -DBUILD_EXAMPLES:BOOL=OFF
    -DBUILD_TESTING:BOOL=OFF

  CMAKE_GENERATOR ${gen}
  INSTALL_COMMAND "${CMAKE_COMMAND}" -E echo "Skipping ITK install step"
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


message(STATUS "Adding external library nlohmann_json in ${nlohmann_json_PREFIX}")

ExternalProject_Add(nlohmann_json
  URL "https://github.com/nlohmann/json/releases/download/v${nlohmann_json_VERSION}/json.tar.xz"
  URL_HASH SHA256=d6c65aca6b1ed68e7a182f4757257b107ae403032760ed6ef121c9d55e81757d
  DOWNLOAD_NAME "nlohmann_json-v${nlohmann_json_VERSION}.tar.xz"
  DOWNLOAD_EXTRACT_TIMESTAMP false

  # Uncomment to instead clone Git repository:
  # GIT_REPOSITORY "${GIT_PROTOCOL}://github.com/nlohmann/json.git"
  # GIT_TAG "9cca280a4d0ccf0c08f47a99aa71d1b0e52f8d03" # tag: v${nlohmann_json_VERSION}
  # GIT_PROGRESS true

  PREFIX "${nlohmann_json_PREFIX}"
  TMP_DIR "${nlohmann_json_PREFIX}/tmp"
  STAMP_DIR "${nlohmann_json_PREFIX}/stamp"
  DOWNLOAD_DIR "${nlohmann_json_PREFIX}/download"
  SOURCE_DIR "${nlohmann_json_PREFIX}/src"
  BINARY_DIR "${nlohmann_json_PREFIX}/build"
  INSTALL_DIR "${nlohmann_json_PREFIX}/install"

  CMAKE_ARGS
    -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
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

  CMAKE_GENERATOR ${gen}
)



message(STATUS "Adding external library spdlog in ${spdlog_PREFIX}")

ExternalProject_Add(spdlog
  URL "https://github.com/gabime/spdlog/archive/refs/tags/v${spdlog_VERSION}.tar.gz"
  URL_HASH SHA512=d6575b5cd53638345078a1c6a886293892359a07ee6de45e23d0c805bb33f59350f33060bce38824e09ce84525b575acdae7b94fc6e82191f5fd576f6c9252b2
  DOWNLOAD_NAME "spdlog-v${spdlog_VERSION}.tar.gz"
  DOWNLOAD_EXTRACT_TIMESTAMP false

  # Uncomment to instead clone Git repository:
  # GIT_REPOSITORY "${GIT_PROTOCOL}://github.com/gabime/spdlog.git"
  # GIT_TAG "f355b3d58f7067eee1706ff3c801c2361011f3d5" # tag: v${spdlog_VERSION}
  # GIT_PROGRESS true

  PREFIX "${spdlog_PREFIX}"
  TMP_DIR "${spdlog_PREFIX}/tmp"
  STAMP_DIR "${spdlog_PREFIX}/stamp"
  DOWNLOAD_DIR "${spdlog_PREFIX}/download"
  SOURCE_DIR "${spdlog_PREFIX}/src"
  BINARY_DIR "${spdlog_PREFIX}/build"
  INSTALL_DIR "${spdlog_PREFIX}/install"

  CMAKE_ARGS
    -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
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
    -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
    -DCMAKE_INSTALL_PREFIX=<INSTALL_DIR>
    -DUUID_BUILD_TESTS:BOOL=OFF
    -DUUID_ENABLE_INSTALL:BOOL=ON
    -DUUID_SYSTEM_GENERATOR:BOOL=OFF
    -DUUID_TIME_GENERATOR:BOOL=OFF
    -DUUID_USING_CXX20_SPAN:BOOL=OFF

  CMAKE_GENERATOR ${gen}
)


message(STATUS "Adding external library TinyFSM in ${tinyfsm_PREFIX}")

ExternalProject_Add(tinyfsm
  GIT_REPOSITORY "${GIT_PROTOCOL}://github.com/digint/tinyfsm.git"
  GIT_TAG "01908cab0397fcdadb0a14e9a3187c308e2708ca" # tag: v${tinyfsm_PREFIX}
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
