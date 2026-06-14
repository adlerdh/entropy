# Building Entropy Medical Image Viewer

This guide explains how to build Entropy from source. Entropy uses CMake and a two-stage superbuild: first CMake downloads and builds pinned third-party dependencies, then the same build directory is reconfigured to build the Entropy application against those dependencies.

For packaging and release artifact commands, see [PACKAGING.md](PACKAGING.md).

## Requirements

Core requirements:

- CMake 3.24 or newer.
- A C++23 compiler and the matching native build tool for your generator, such as Make, Ninja, Visual Studio, or Xcode.
- Git and network access for the superbuild. Dependencies are downloaded as pinned archives with hash checks, and the app build records Git metadata in the build stamp.
- Disk space for the superbuild. A completed release build tree is roughly 2-3 GB on Linux, but allow at least 8-10 GB free for a clean build because source archives, extracted sources, intermediate build trees, and install trees coexist during the build.

Entropy is developed and tested on:

| Platform | Toolchain notes |
| --- | --- |
| Ubuntu 22.04 and 24.04 | GCC 13.3.0 or newer. Other Linux distributions may work if they provide a C++23 compiler and the development libraries listed below. |
| Windows 10 and 11 | Visual Studio 2022 17.3.4 or newer. |
| macOS 14.6.1, 15.3.1, 15.6.1, and 26.0.1 | Apple arm64 with Apple Clang 15.0.0 or newer. *macOS Intel x86_64 is not supported*. |

## Linux Development Packages

On Debian or Ubuntu, install the development packages needed for OpenGL, windowing, and native file dialogs:

```sh
sudo apt-get install xorg-dev libgl1-mesa-dev libxcursor-dev libxkbcommon-dev libxinerama-dev libxi-dev libxrandr-dev libwayland-dev libdbus-1-dev
```

`libdbus-1-dev` is required by [Native File Dialog Extended (NFDe)](https://github.com/btzy/nativefiledialog-extended) when Entropy uses the Linux `xdg-desktop-portal` backend. This is a build-time package. Runtime Linux packages should depend on `libdbus-1-3` and a working `xdg-desktop-portal` backend instead.

On macOS and Windows, Native File Dialog Extended uses platform APIs, so no additional NFDe-specific development package is required.

## Quick Build With Presets

For day-to-day development, use the default presets. They build shared libraries with `RelWithDebInfo` in `build-default`.

```sh
cmake --preset superbuild
cmake --build --preset superbuild --parallel

cmake --preset app
cmake --build --preset app --parallel
```

The same build directory is configured twice. First `Entropy_SUPERBUILD=ON` builds dependencies. Then `Entropy_SUPERBUILD=OFF` builds the application.

Run the result with:

```sh
build-default/bin/entropy # Linux
open build-default/bin/Entropy.app # macOS
.\build-default\bin\entropy.exe # Windows
```

## Build Presets

The project defines configure and build presets in [CMakePresets.json](CMakePresets.json). Presets are the recommended interface because they keep build directories and common cache values consistent.

Most presets have the same name for configure and build steps: configure with `cmake --preset NAME`, then build with `cmake --build --preset NAME --parallel`. The `package-release` build preset is the exception, since it reuses the `app-release` configure preset and builds CMake's `package` target.

| Preset | Build directory | Build type | Stage | Commands | Purpose |
| --- | --- | --- | --- | --- | --- |
| `superbuild` | `build-default` | `RelWithDebInfo` | Dependencies | `cmake --preset superbuild`<br>`cmake --build --preset superbuild --parallel` | Default dependency superbuild for day-to-day development. |
| `app` | `build-default` | `RelWithDebInfo` | App | `cmake --preset app`<br>`cmake --build --preset app --parallel` | Default Entropy app and tests after `superbuild` has completed. |
| `default` | `build-default` | `RelWithDebInfo` | App | `cmake --preset default`<br>`cmake --build --preset default --parallel` | Alias-style app preset for the default development build. |
| `superbuild-debug` | `build-debug` | `Debug` | Dependencies | `cmake --preset superbuild-debug`<br>`cmake --build --preset superbuild-debug --parallel` | Debug dependency superbuild. |
| `app-debug` | `build-debug` | `Debug` | App | `cmake --preset app-debug`<br>`cmake --build --preset app-debug --parallel` | Debug Entropy app and tests after `superbuild-debug`. |
| `superbuild-coverage` | `build-coverage` | `Debug` | Dependencies | `cmake --preset superbuild-coverage`<br>`cmake --build --preset superbuild-coverage --parallel` | Debug dependency superbuild for code coverage builds. |
| `app-coverage` | `build-coverage` | `Debug` | App | `cmake --preset app-coverage`<br>`cmake --build --preset app-coverage --parallel` | Entropy app and tests built with coverage instrumentation after `superbuild-coverage`. |
| `coverage` | `build-coverage` | `Debug` | Tests/report | `cmake --preset app-coverage`<br>`cmake --build --preset coverage --parallel` | Builds instrumented unit-test binaries, runs CTest, and prints a coverage summary. |
| `coverage-html` | `build-coverage` | `Debug` | Tests/report | `cmake --preset app-coverage`<br>`cmake --build --preset coverage-html --parallel` | Builds instrumented unit-test binaries, runs CTest, and writes an HTML coverage report. |
| `superbuild-release` | `build-release` | `Release` | Dependencies | `cmake --preset superbuild-release`<br>`cmake --build --preset superbuild-release --parallel` | Release dependency superbuild. Use this before release builds and packaging. |
| `app-release` | `build-release` | `Release` | App | `cmake --preset app-release`<br>`cmake --build --preset app-release --parallel` | Release Entropy app and tests after `superbuild-release`. Use this before packaging. |
| `package-release` | `build-release` | `Release` | Package | `cmake --preset app-release`<br>`cmake --build --preset package-release --parallel` | Builds the release app if needed and runs CPack. See [PACKAGING.md](PACKAGING.md). |

## Example Build Flows

These examples show the complete command sequences for the build trees people are most likely to create. Use the default flow from [Quick Build With Presets](#quick-build-with-presets) for normal development, use a debug build when you need unoptimized binaries for debugging, and use a release build when preparing a distributable app or package.

### Debug Build

Debug builds use `CMAKE_BUILD_TYPE=Debug` in `build-debug`. They are useful when stepping through code in a debugger or when you want assertions and debug information from Entropy and dependencies.

```sh
cmake --preset superbuild-debug
cmake --build --preset superbuild-debug --parallel

cmake --preset app-debug
cmake --build --preset app-debug --parallel
```

Run the debug build from `build-debug/bin`.

### Release Build

Release builds use `CMAKE_BUILD_TYPE=Release` in `build-release`. Use this flow before creating packages, and when you want to test the optimized application users will receive.

```sh
cmake --preset superbuild-release
cmake --build --preset superbuild-release --parallel

cmake --preset app-release
cmake --build --preset app-release --parallel
```

Run the release build from `build-release/bin`. For package commands, see [PACKAGING.md](PACKAGING.md).

### Tests

The app build also builds the test binaries. Run tests against the build tree you just built:

```sh
ctest --test-dir build-default --parallel --output-on-failure
```

For release or debug builds, replace `build-default` with `build-release` or `build-debug`.

### Code Coverage

Coverage builds are opt-in and should use their own build directory. The provided coverage presets use `build-coverage`, `Debug`, `BUILD_TESTING=ON`, and `Entropy_ENABLE_COVERAGE=ON`.

```sh
cmake --preset superbuild-coverage
cmake --build --preset superbuild-coverage --parallel

cmake --preset app-coverage
cmake --build --preset coverage --parallel
```

The `coverage` target builds the instrumented unit-test binaries, runs the full CTest suite, and prints a terminal coverage summary. The report excludes external dependencies, generated build files, and test source files from the coverage totals.

To generate an HTML report instead:

```sh
cmake --build --preset coverage-html --parallel
```

The HTML output is written under:

```text
build-coverage/coverage/html/index.html
```

Coverage backend selection is automatic by default:

| Compiler | Default backend | Required tools |
| --- | --- | --- |
| Clang or AppleClang | LLVM source-based coverage | `llvm-cov` and `llvm-profdata`; on macOS, Xcode's tools are discovered through `xcrun` when needed. |
| GCC | gcov-compatible coverage | `gcovr`, or both `lcov` and `genhtml`. |
| MSVC | OpenCppCoverage | `OpenCppCoverage` on `PATH`. |

You can override the backend at configure time:

```sh
cmake --preset app-coverage -DEntropy_COVERAGE_MODE=LLVM
cmake --preset app-coverage -DEntropy_COVERAGE_MODE=GCOV
cmake --preset app-coverage -DEntropy_COVERAGE_MODE=OPENCPPCOVERAGE
```

If you need to include or exclude different files in local reports, override `Entropy_COVERAGE_EXCLUDE_REGEX` when configuring the app stage. The default excludes external dependencies, test files, build-tree files, system headers, and Xcode SDK/toolchain paths.

## Pre-Commit Checks

Entropy uses [pre-commit](https://pre-commit.com/) for lightweight source checks before commits. The current hook runs `clang-format -i` on matching files under `app` and `lib`, so it may rewrite files in place. After running it, review `git diff` before committing.

Install pre-commit with your system package manager or Python environment. For example:

```sh
python3 -m pip install pre-commit
```

Enable the Git hook in this checkout:

```sh
pre-commit install
```

Run the checks on staged files:

```sh
pre-commit run
```

Run the checks across the whole repository:

```sh
pre-commit run --all-files
```

`clang-format` must be available on `PATH` because the hook uses the system formatter.

## Build Options

These are the project-level CMake cache options and important build variables you are likely to use. Pass them at configure time with `-DNAME=value`, or place local overrides in `CMakeUserPresets.json`.

Entropy-specific user-facing cache options use the `Entropy_` prefix. Lowercase `entropy_` names are internal CMake variables used by the build scripts.

| Option | Type | Default | Applies to | What it does |
| --- | --- | --- | --- | --- |
| `Entropy_SUPERBUILD` | Boolean | `ON` | All platforms | Selects the superbuild stage. `ON` configures external dependencies and returns before configuring the app. `OFF` configures Entropy itself and expects dependencies to already exist in the matching build directory. |
| `CMAKE_BUILD_TYPE` | String | `RelWithDebInfo` for single-config generators | Single-config generators such as Ninja and Unix Makefiles | Selects `Debug`, `Release`, `RelWithDebInfo`, or `MinSizeRel`. Multi-config generators ignore this and use `--config` at build time. |
| `Entropy_SUPERBUILD_CONFIG` | String | `Release` for multi-config superbuilds | Multi-config generators such as Visual Studio, Xcode, and Ninja Multi-Config | Selects the dependency build configuration used by ExternalProject builds when the generator supports multiple configurations. |
| `BUILD_SHARED_LIBS` | Boolean | `OFF` in raw CMake; `ON` in the provided presets | All platforms | Chooses shared or static builds for dependencies that honor the standard CMake option. Package presets may override selected bundled dependencies with package-specific static-link settings. |
| `Entropy_STATIC_BUNDLED_DEPENDENCIES` | Boolean | `ON` on Unix, `OFF` elsewhere | Superbuild | Builds bundled third-party dependencies as static libraries where practical. Qt and system libraries stay dynamic. |
| `Entropy_WINDOWS_PACKAGE_STATIC_SMALL_DEPS` | Boolean | `ON` in release presets, `OFF` otherwise | Windows superbuild/package stage | Builds GLFW, Native File Dialog Extended, and spdlog as static libraries for Windows release packages. ITK and Qt keep the normal Windows dynamic linkage. |
| `Entropy_USE_CCACHE` | Boolean | `ON` | All platforms | Uses `ccache` as the compiler launcher when it is installed. If `ccache` is not found, CMake continues without it. |
| `Entropy_SUPERBUILD_PARALLEL` | String | Empty | Superbuild | Sets the parallel level passed to ExternalProject builds. Empty means the native build tool chooses its own default. |
| `BUILD_TESTING` | Boolean | `ON` when CTest is enabled | App stage | Enables test targets through CTest/Catch2. Disable with `-DBUILD_TESTING=OFF` if you need a smaller local build graph. |
| `Entropy_ENABLE_COVERAGE` | Boolean | `OFF` | App stage | Builds Entropy targets and unit-test executables with coverage instrumentation and adds `coverage`, `coverage-html`, and `coverage-clean` targets. |
| `Entropy_COVERAGE_MODE` | String | `AUTO` | App stage with coverage enabled | Selects the coverage backend. Valid values are `AUTO`, `LLVM`, `GCOV`, and `OPENCPPCOVERAGE`. |
| `Entropy_COVERAGE_EXCLUDE_REGEX` | String | Project-defined regex | App stage with coverage enabled | Regular expression passed to the coverage reporter to exclude external dependencies, test sources, build-tree files, and system/toolchain paths. |
| `Entropy_GLAD_GL_VERSION` | String | `3.3` | App stage | Selects the vendored GLAD OpenGL Core loader. Valid values are `3.3`, `4.1`, and `4.6`; Entropy defaults to `3.3` for broad hardware and platform compatibility. |
| `Entropy_GLAD_GL_DEBUG` | Boolean | `false` | App stage | Uses the debug GLAD loader variant for the selected OpenGL version. Useful when debugging OpenGL calls. |
| `Entropy_PACKAGE_OUTPUT_DIR` | Path | `${CMAKE_BINARY_DIR}/packages` | Packaging | Directory where CPack writes generated artifacts and `_CPack_Packages` scratch files. See [PACKAGING.md](PACKAGING.md). |
| `Entropy_STRIP_PACKAGED_APP` | Boolean | `ON` | macOS packaging | Strips local symbols from the installed `.app` bundle before signing. |
| `Entropy_MACOS_CODESIGN_IDENTITY` | String | `-` | macOS app/package stage | Code-signing identity for installed and packaged macOS apps. `-` creates an ad-hoc signature. An empty value skips signing. Use a Developer ID Application identity for public distribution. |
| `Entropy_MACOSX_BUNDLE_IDENTIFIER` | String | `io.github.adlerdh.entropy` | macOS app/package stage | Bundle identifier written to the macOS app `Info.plist`. |
| `Entropy_MACOSX_BUNDLE_MINIMUM_SYSTEM_VERSION` | String | `CMAKE_OSX_DEPLOYMENT_TARGET` if set, otherwise `13.0` | macOS app/package stage | Minimum macOS version recorded in `Info.plist`. |
| `Entropy_MACOSX_BUNDLE_VERSION` | String | Project version | macOS app/package stage | Build version recorded in `CFBundleVersion`. |

Standard CMake variables such as `CMAKE_INSTALL_PREFIX`, `CMAKE_OSX_DEPLOYMENT_TARGET`, `CMAKE_PREFIX_PATH`, and generator selection also work normally.

## Manual Build Commands

Use the presets unless you need a custom generator, build directory, or cache configuration.

### Single-Config Generators

Single-config generators include Ninja and Unix Makefiles. They use `CMAKE_BUILD_TYPE` at configure time.

```sh
BUILD_TYPE=Release
SHARED_LIBS=ON
BUILD_DIR="build-${BUILD_TYPE}-shared"
PARALLEL=$(python3 -c "import os; print(os.cpu_count() or 1)")

cmake -S . -B "${BUILD_DIR}" \
  -DEntropy_SUPERBUILD=ON \
  -DCMAKE_BUILD_TYPE="${BUILD_TYPE}" \
  -DBUILD_SHARED_LIBS="${SHARED_LIBS}" \
  -DEntropy_SUPERBUILD_PARALLEL="${PARALLEL}"

cmake --build "${BUILD_DIR}" --parallel "${PARALLEL}"

cmake -S . -B "${BUILD_DIR}" \
  -DEntropy_SUPERBUILD=OFF \
  -DCMAKE_BUILD_TYPE="${BUILD_TYPE}" \
  -DBUILD_SHARED_LIBS="${SHARED_LIBS}"

cmake --build "${BUILD_DIR}" --parallel "${PARALLEL}"
```

### Multi-Config Generators

Multi-config generators include Visual Studio, Xcode, and Ninja Multi-Config. They use `--config` at build time.

```sh
BUILD_TYPE=Release
SHARED_LIBS=ON
BUILD_DIR="build-${BUILD_TYPE}-shared"
PARALLEL=$(python3 -c "import os; print(os.cpu_count() or 1)")

cmake -S . -B "${BUILD_DIR}" \
  -DEntropy_SUPERBUILD=ON \
  -DBUILD_SHARED_LIBS="${SHARED_LIBS}" \
  -DEntropy_SUPERBUILD_CONFIG="${BUILD_TYPE}" \
  -DEntropy_SUPERBUILD_PARALLEL="${PARALLEL}"

cmake --build "${BUILD_DIR}" --config "${BUILD_TYPE}" --parallel "${PARALLEL}"

cmake -S . -B "${BUILD_DIR}" \
  -DEntropy_SUPERBUILD=OFF \
  -DBUILD_SHARED_LIBS="${SHARED_LIBS}"

cmake --build "${BUILD_DIR}" --config "${BUILD_TYPE}" --parallel "${PARALLEL}"
```

## Third-Party Dependencies and Resources

Entropy uses external projects through the CMake superbuild and carries some vendored source and resource files in this repository. Versions, source URLs, and license notes are documented in [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).

Original attributions and licenses have been preserved and committed for all external sources and resources.
