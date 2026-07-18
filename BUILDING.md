# Building Entropy

This guide explains how to configure, build, and test Entropy from source. Entropy uses CMake and a two-stage dependency build, following the CMake "superbuild" pattern: first build the pinned third-party dependencies, then configure and build the Entropy app against those dependencies.

For installer, archive, signing, and GitHub release details, see [PACKAGING.md](PACKAGING.md).

## Requirements

- CMake 3.28 or newer
- A C++23 compiler and matching native build tool, such as Make, Ninja, Visual Studio, or Xcode
- Git and network access for the dependency build
- Disk space for dependencies and build products

A completed release build tree is roughly 2-3 GB. A clean build needs more temporary space because source archives, extracted sources, build trees, and install trees coexist during the dependency build. Allow at least 8-10 GB.

Entropy is developed and tested on:

| Platform | Toolchain |
| --- | --- |
| macOS arm64 and x86_64 | Apple Clang 15.0.0 or newer |
| Windows x86_64 | Visual Studio 2022 17.3.4 or newer |
| Ubuntu 22.04 x86_64 | GCC 13 or newer |
| Fedora 43 x86_64 | GCC 13 or newer |

Other systems may work if they provide a C++23 compiler and the required OpenGL/windowing development libraries.

## Linux Packages

On Debian or Ubuntu, install the development packages needed for OpenGL, windowing, native file dialogs, OpenSSL, and the dependency build:

```sh
sudo apt-get update
sudo apt-get install --no-install-recommends -y \
  ccache \
  gcc-13 \
  g++-13 \
  libdbus-1-dev \
  libssl-dev \
  libgl1-mesa-dev \
  libwayland-dev \
  libxcursor-dev \
  libxi-dev \
  libxinerama-dev \
  libxkbcommon-dev \
  libxrandr-dev \
  xorg-dev
```

On Fedora, install the equivalent development packages:

```sh
sudo dnf install -y \
  ccache \
  cmake \
  file \
  gcc \
  gcc-c++ \
  git \
  dbus-devel \
  libglvnd-devel \
  libglvnd-opengl \
  libX11-devel \
  libXcursor-devel \
  libXext-devel \
  libXfixes-devel \
  libXi-devel \
  libXinerama-devel \
  libXrandr-devel \
  libxkbcommon-devel \
  make \
  mesa-libGL-devel \
  openssl-devel \
  wayland-devel
```

`libdbus-1-dev` on Ubuntu and `dbus-devel` on Fedora are used by [Native File Dialog Extended](https://github.com/btzy/nativefiledialog-extended) for the Linux `xdg-desktop-portal` backend.

## Build with Presets

The supported local build path is through [CMakePresets.json](CMakePresets.json). Configure and build dependencies first, then configure and build the app in the same build directory.

Debug build:

```sh
cmake --preset deps-debug
cmake --build --preset deps-debug --parallel

cmake --preset app-debug
cmake --build --preset app-debug --parallel
```

Release build:

```sh
cmake --preset deps-release
cmake --build --preset deps-release --parallel

cmake --preset app-release
cmake --build --preset app-release --parallel
```

The presets enable `Entropy_USE_CCACHE=ON`. If `ccache` is installed, CMake uses it as the compiler launcher to speed up repeated local builds. If `ccache` is not installed, the build continues normally without caching.

If the dependency build runs out of memory, pass a smaller number to `--parallel` (`-j`), e.g. the number of logical CPUs minus two, with a minimum of one:

```sh
N=$(( $(nproc) - 2 )); [ "$N" -lt 1 ] && N=1 # Linux
N=$(( $(sysctl -n hw.logicalcpu) - 2 )); [ "$N" -lt 1 ] && N=1 # macOS
```

```powershell
$N = [Math]::Max(1, [Environment]::ProcessorCount - 2) # Windows
```

## CMake Presets

| Preset | Build directory | Purpose |
| --- | --- | --- |
| `deps-debug` | `build-debug` | Configure/build Debug dependencies |
| `app-debug` | `build-debug` | Configure/build the Debug app and tests after `deps-debug` |
| `deps-release` | `build-release` | Configure/build Release dependencies |
| `app-release` | `build-release` | Configure/build the Release app and tests after `deps-release` |
| `package-release` | `build-release` | Build the Release package target after `app-release` |

The `deps-*` presets configure with `Entropy_SUPERBUILD=ON`. The `app-*` presets configure the same build directory with `Entropy_SUPERBUILD=OFF`.

## Run Entropy

Run the app from the build tree:

```sh
build-debug/bin/entropy # macOS
.\build-debug\bin\entropy.exe # Windows
build-debug/bin/entropy # Linux
```

For release builds, replace `build-debug` with `build-release`.

## Run Tests

The app build also builds the unit tests. Run them with CTest:

```sh
ctest --test-dir build-debug --parallel --output-on-failure
```

For release builds, use `build-release`:

```sh
ctest --test-dir build-release --parallel --output-on-failure
```

On multi-config generators such as Visual Studio, add the configuration:

```sh
ctest --test-dir build-debug -C Debug --parallel --output-on-failure
ctest --test-dir build-release -C Release --parallel --output-on-failure
```

## Packaging

Create release packages with:

```sh
cmake --preset deps-release
cmake --build --preset deps-release --parallel

cmake --preset app-release
cmake --build --preset package-release --parallel
```

See [PACKAGING.md](PACKAGING.md) for package formats, release artifact names, signing notes, and GitHub release behavior.

## Coverage

Coverage builds are optional and should use their own build directory:

```sh
cmake -S . -B build-coverage \
  -DCMAKE_BUILD_TYPE=Debug \
  -DBUILD_SHARED_LIBS=ON \
  -DEntropy_SUPERBUILD=ON \
  -DEntropy_SUPERBUILD_CONFIG=Debug
cmake --build build-coverage --parallel

cmake -S . -B build-coverage \
  -DCMAKE_BUILD_TYPE=Debug \
  -DBUILD_SHARED_LIBS=ON \
  -DEntropy_SUPERBUILD=OFF \
  -DEntropy_ENABLE_COVERAGE=ON
cmake --build build-coverage --target coverage --parallel
```

Coverage backend selection is automatic by default:

| Compiler | Default backend |
| --- | --- |
| Clang or AppleClang | [LLVM source-based coverage](https://clang.llvm.org/docs/SourceBasedCodeCoverage.html) |
| GCC | [gcov-compatible coverage](https://gcc.gnu.org/onlinedocs/gcc/Gcov.html) |
| MSVC | [OpenCppCoverage](https://github.com/OpenCppCoverage/OpenCppCoverage) |

The `coverage` target writes machine-readable output under `build-coverage/coverage/`. The `coverage-html` target writes an HTML report under `build-coverage/coverage/html/`.

## Local Hygiene Checks

Entropy uses [pre-commit](https://pre-commit.com/) for lightweight local checks before committing.
The default hooks run `codespell` and `clang-format`; the Markdown link check is manual because it uses
the network and can fail when external sites are temporarily unavailable.

Install and enable it:

```sh
python3 -m pip install pre-commit
pre-commit install
```

Run the default hooks manually:

```sh
pre-commit run --all-files
```

Run the Markdown link check when editing documentation:

```sh
pre-commit run lychee-doc-links --hook-stage manual
```

The manual link hook expects `lychee` to be installed and available on `PATH`. (The GitHub Actions text hygiene workflow is manual.)

## CMake Options

Pass options at configure time with `-DNAME=value`, or put local overrides in `CMakeUserPresets.json`.

| Option | Default | Stage | Purpose |
| --- | --- | --- | --- |
| `Entropy_SUPERBUILD` | `ON` | Configure | Selects the stage. `ON` builds dependencies; `OFF` builds Entropy against dependencies already in the build tree |
| `CMAKE_BUILD_TYPE` | `RelWithDebInfo` outside presets | Configure | Selects `Debug`, `Release`, `RelWithDebInfo`, or `MinSizeRel` for single-config generators |
| `Entropy_SUPERBUILD_CONFIG` | `Release` | Dependencies | Selects dependency build type for multi-config generators such as Visual Studio, Xcode, and Ninja Multi-Config |
| `BUILD_SHARED_LIBS` | `OFF` outside presets, `ON` in presets | Dependencies | Chooses shared or static libraries for dependencies that honor the standard CMake option |
| `Entropy_STATIC_BUNDLED_DEPENDENCIES` | `ON` on macOS/Linux; `OFF` on Windows; `ON` in release presets | Dependencies | Builds bundled dependencies as static libraries where practical; Qt and system libraries stay dynamic |
| `Entropy_SUPERBUILD_PARALLEL` | empty | Dependencies | Sets parallelism for ExternalProject dependency builds; empty lets the native build tool choose |
| `Entropy_USE_CCACHE` | `ON` | Configure | Uses `ccache` as the compiler launcher when available |
| `CMAKE_VERBOSE_MAKEFILE` | `OFF` | Application/dependencies | Prints full native build commands for Makefile-style generators |
| `BUILD_TESTING` | CTest default | Application | Enables unit-test targets |
| `Entropy_ENABLE_CLANG_TIDY` | `OFF` | Application | Runs `clang-tidy` during C++ compilation |
| `Entropy_CLANG_TIDY_OPTIONS` | `--quiet` | Application | Extra options passed to `clang-tidy` |
| `Entropy_ENABLE_IWYU` | `OFF` | Application | Runs Include What You Use during C++ compilation |
| `Entropy_IWYU_OPTIONS` | project default | Application | Extra options passed to Include What You Use |
| `Entropy_ENABLE_COVERAGE` | `OFF` | Application | Adds coverage instrumentation and coverage targets |
| `Entropy_COVERAGE_MODE` | `AUTO` | Application | Selects coverage backend: `AUTO`, `LLVM`, `GCOV`, or `OPENCPPCOVERAGE` |
| `Entropy_COVERAGE_EXCLUDE_REGEX` | project default | Application | Regex used to exclude external, generated, test, and system files from coverage reports |
| `Entropy_ENABLE_TRACE_LOGGING` | `OFF` | Application | Compiles trace-level logging calls into Entropy |
| `Entropy_GLAD_GL_VERSION` | `3.3` | Application | Selects the vendored GLAD OpenGL Core loader version: `3.3`, `4.1`, or `4.6` |
| `Entropy_GLAD_GL_DEBUG` | `false` | Application | Uses the debug GLAD loader variant |

Standard CMake variables such as `CMAKE_INSTALL_PREFIX`, `CMAKE_OSX_DEPLOYMENT_TARGET`, `CMAKE_PREFIX_PATH`, and generator selection also work normally. Packaging options are documented in [PACKAGING.md](PACKAGING.md).

## Continuous Integration

CI workflows are under [.github/workflows](.github/workflows). They build and test Entropy on macOS, Windows, Ubuntu, and Fedora.

Pull requests and pushes to `main` run formatting checks, debug builds, and unit tests on the primary CI platforms. Additional coverage, release-package validation, static analysis, text hygiene, and compatibility jobs are available through scheduled or manual workflow runs. Official tag-driven release builds are handled by the release workflow described in [PACKAGING.md](PACKAGING.md).

The main CI build matrix is:

| Platform | Runner | Toolchain | Scope |
| --- | --- | --- | --- |
| macOS arm64 | `macos-14` | Apple Clang through Xcode | Debug build and tests, release packages, optional coverage |
| macOS x86_64 | `macos-15-intel` | Apple Clang through Xcode | Debug build and tests, release packages |
| macOS arm64 compatibility | `macos-26` | Apple Clang through Xcode | Scheduled/manual Debug build and tests on a newer macOS runner |
| Windows x86_64 | `windows-2022` | Visual Studio 2022 / MSVC v143 | Debug build and tests, release packages, optional coverage |
| Windows x86_64 compatibility | `windows-2025` | Visual Studio 2026 / MSVC | Scheduled/manual Debug build and tests on a newer Windows runner |
| Ubuntu 22.04 x86_64 | `ubuntu-22.04` | `gcc-13` / `g++-13` | Debug build and tests, release packages, and primary coverage |
| Fedora 43 x86_64 | `fedora:43` container on `ubuntu-24.04` | GCC 15.2.1 | Manual Debug build and tests, manual release packages, and tag-driven Fedora release packages |

The Ubuntu 22.04 workflow installs `gcc-13` and `g++-13` from the Ubuntu toolchain PPA, which currently resolves to GCC 13.3.0. Ubuntu 24.04 is used as the host runner for Fedora container jobs, not as a separate Ubuntu application build.

macOS release artifacts are built separately for `arm64` and `x86_64`. Entropy does not publish a universal macOS binary.

> The workflow files are the source of truth for exact runner images, package installation commands, cache keys, and artifact upload names.

## Third-Party Dependencies

Entropy builds pinned third-party dependencies from source during the dependency stage. Versions, source URLs, and license notes are documented in [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).
