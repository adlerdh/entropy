include(ExternalProject)

# Compute -G arg for configuring external projects with the same CMake generator:
if(CMAKE_EXTRA_GENERATOR)
  set(gen "${CMAKE_EXTRA_GENERATOR} - ${CMAKE_GENERATOR}")
else()
  set(gen "${CMAKE_GENERATOR}")
endif()

set(GIT_PROTOCOL "https")

message(STATUS "Downloading and building ITK in ${ITK_PREFIX}")

ExternalProject_Add(ITK
  URL "https://github.com/InsightSoftwareConsortium/ITK/releases/download/v${ITK_VERSION}/InsightToolkit-${ITK_VERSION}.tar.gz"
  URL_HASH SHA512=440d5962336ae7ba68e1efcabd78db8f10437db27da077a65731024d2fd94c588468678d0af8d8be1bfdb45dc90a88ace85ae9e1fabf77fb4172f3cb7cc27a3c
  DOWNLOAD_EXTRACT_TIMESTAMP false

  # Uncomment to instead clone Git repository:
  # GIT_REPOSITORY "${GIT_PROTOCOL}://github.com/InsightSoftwareConsortium/ITK.git"
  # GIT_TAG "898def645183e6a2d3293058ade451ec416c4514" # tag: v${ITK_VERSION}
  # GIT_PROGRESS true

  PREFIX "${ITK_PREFIX}"
  TMP_DIR "${ITK_PREFIX}/tmp"
  STAMP_DIR "${ITK_PREFIX}/stamp"
  DOWNLOAD_DIR "${ITK_PREFIX}/download"
  SOURCE_DIR "${ITK_PREFIX}/src"
  BINARY_DIR "${ITK_PREFIX}/build"

  CMAKE_ARGS
    -DCMAKE_BUILD_TYPE:STRING=${CMAKE_BUILD_TYPE}
    -DBUILD_SHARED_LIBS:BOOL=${BUILD_SHARED_LIBS}
    -DBUILD_STATIC_LIBS:BOOL=${BUILD_STATIC_LIBS}
    -DBUILD_EXAMPLES:BOOL=OFF
    -DBUILD_TESTING:BOOL=OFF

  CMAKE_GENERATOR ${gen}
  UPDATE_COMMAND ""
  PATCH_COMMAND ""
  INSTALL_COMMAND "${CMAKE_COMMAND}" -E echo "Skipping ITK install step"
)


message(STATUS "Downloading and building Boost in ${Boost_PREFIX}")

set(Boost_Bootstrap_CMD)
set(Boost_b2_CMD)

if(WIN32)
  set(Boost_Bootstrap_CMD bootstrap.bat)
  set(Boost_b2_CMD b2.exe)
elseif(UNIX OR APPLE)
  set(Boost_Bootstrap_CMD ./bootstrap.sh)
  set(Boost_b2_CMD ./b2)
endif()

ExternalProject_Add(Boost
  URL "https://github.com/boostorg/boost/releases/download/boost-${Boost_VERSION}/boost-${Boost_VERSION}-b2-nodocs.tar.xz"
  URL_HASH SHA256=3abd7a51118a5dd74673b25e0a3f0a4ab1752d8d618f4b8cea84a603aeecc680
  DOWNLOAD_EXTRACT_TIMESTAMP false

  # Uncomment to instead clone Git repositories:
  # GIT_REPOSITORY "${GIT_PROTOCOL}://github.com/boostorg/boost.git"
  # GIT_TAG "c89e6267665516192015a9e40955e154466f4f68" # tag: ${Boost_VERSION}
  # GIT_SUBMODULES_RECURSE true
  # GIT_PROGRESS true

  PREFIX "${Boost_PREFIX}"
  TMP_DIR "${Boost_PREFIX}/tmp"
  STAMP_DIR "${Boost_PREFIX}/stamp"
  DOWNLOAD_DIR "${Boost_PREFIX}/download"
  SOURCE_DIR "${Boost_PREFIX}/src"

  CONFIGURE_COMMAND ${Boost_Bootstrap_CMD}
  BUILD_IN_SOURCE true
  UPDATE_COMMAND ""
  PATCH_COMMAND ""
  BUILD_COMMAND ${Boost_b2_CMD} headers
  INSTALL_COMMAND "${CMAKE_COMMAND}" -E echo "Skipping Boost install step"
)


message(STATUS "Downloading and building spdlog in ${spdlog_PREFIX}")

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

  CMAKE_ARGS
    -DCMAKE_BUILD_TYPE:STRING=${CMAKE_BUILD_TYPE}
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
  UPDATE_COMMAND ""
  PATCH_COMMAND ""
  INSTALL_COMMAND "${CMAKE_COMMAND}" -E echo "Skipping spdlog install step"
)


message(STATUS "Downloading and building GLFW in ${glfw_PREFIX}")

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
    -DCMAKE_BUILD_TYPE:STRING=${CMAKE_BUILD_TYPE}
    -DCMAKE_INSTALL_PREFIX="${glfw_PREFIX}/install"
    -DBUILD_SHARED_LIBS:BOOL=${BUILD_SHARED_LIBS}
    -DGLFW_BUILD_DOCS:BOOL=OFF
    -DGLFW_BUILD_EXAMPLES:BOOL=OFF
    -DGLFW_BUILD_TESTS:BOOL=OFF
    -DGLFW_INSTALL:BOOL=ON

  CMAKE_GENERATOR ${gen}
  UPDATE_COMMAND ""
  PATCH_COMMAND ""
  INSTALL_COMMAND ${CMAKE_COMMAND} --install "${glfw_PREFIX}/build" --prefix "${glfw_PREFIX}/install"
)


message(STATUS "Downloading and building nlohmann_json in ${nlohmann_json_PREFIX}")

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

  CMAKE_ARGS
    -DCMAKE_BUILD_TYPE:STRING=${CMAKE_BUILD_TYPE}
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
  UPDATE_COMMAND ""
  PATCH_COMMAND ""
  INSTALL_COMMAND "${CMAKE_COMMAND}" -E echo "Skipping nlohmann_json install step"
)


message(STATUS "Downloading NanoVG in ${nanovg_PREFIX}")

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
  UPDATE_COMMAND ""
  PATCH_COMMAND ""
  CONFIGURE_COMMAND "${CMAKE_COMMAND}" -E echo "Skipping NanoVG configure step"
  BUILD_COMMAND "${CMAKE_COMMAND}" -E echo "Skipping NanoVG build step"
  INSTALL_COMMAND "${CMAKE_COMMAND}" -E echo "Skipping NanoVG install step"
)



message(STATUS "Downloading and building GLM in ${glm_PREFIX}")

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

  CMAKE_ARGS
    -DCMAKE_BUILD_TYPE:STRING=${CMAKE_BUILD_TYPE}
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
  UPDATE_COMMAND ""
  PATCH_COMMAND ""
  INSTALL_COMMAND "${CMAKE_COMMAND}" -E echo "Skipping GLM install step"
)
