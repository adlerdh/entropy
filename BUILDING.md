# Building Entropy

Build the pinned third-party dependencies first, then configure and build the Entropy application against them in the
 same directory. Run all commands given below from the repository root unless stated otherwise.

This also covers tests, analysis, and coverage. For installer, archive, signing, and GitHub release details, see
[PACKAGING.md](PACKAGING.md).

## Requirements

- CMake 3.28 or newer
- A C++23 compiler and matching native build tool, such as Make, Ninja, Visual Studio, or Xcode
- Git and network access for the dependency build
- About 15 GB of available disk space for one build tree
- An OpenGL 3.3-capable graphics driver and graphical session to run Entropy

A Release tree can use about 5 GB, while a Debug tree can reach 13 GB. Allow more space for multiple build trees or a
compiler cache. Downloads, source trees, and build outputs coexist during the dependency build.

Entropy is developed and tested on:

| Platform | Toolchain |
| --- | --- |
| macOS arm64 and x86_64 | Apple Clang 15.0.0 or newer |
| Windows x86_64 | Visual Studio 2022 17.3.4 or newer |
| Ubuntu 22.04 x86_64 | GCC 13 or newer |
| Ubuntu 24.04 x86_64 | GCC 13 or newer |
| Fedora 43 x86_64 | GCC 15 |

Other systems may work if they have a C++23 compiler and the required OpenGL/windowing development libraries.


## Platform Setup

On macOS, install Xcode and its command-line tools. Install CMake and Git with MacPorts, Homebrew, or their official
installers.

On Windows, install Visual Studio 2022 with the **Desktop development with C++** workload, plus CMake and Git if they
are not already available on `PATH`.

On Ubuntu 22.04, install the development packages needed for OpenGL, windowing, native file dialogs, OpenSSL, and the
dependency build. The list includes the optional [ccache](https://ccache.dev/) compiler cache described later. GCC 13
is from by the Ubuntu toolchain PPA:

```sh
sudo apt-get update
sudo apt-get install --no-install-recommends -y software-properties-common
sudo add-apt-repository --yes ppa:ubuntu-toolchain-r/test
sudo apt-get update
sudo apt-get install --no-install-recommends -y \
  ccache \
  gcc-13 \
  g++-13 \
  glslang-tools \
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

Select GCC 13 for the current shell before configuring:

```sh
export CC=gcc-13
export CXX=g++-13
```

On Ubuntu 24.04, GCC 13 is available from the standard repositories. Skip the `software-properties-common` and
`add-apt-repository` commands, then install the same build packages and select `gcc-13` and `g++-13` as shown above.

On Fedora 43, install the equivalent development packages:

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

Native File Dialog Extended needs `libdbus-1-dev` on Ubuntu or `dbus-devel` on Fedora to use the Linux
`xdg-desktop-portal` backend.

### Windows path length

ITK may reject a long checkout path on Windows. Keep the checkout near the root of a drive, such as `C:\entropy`, or
map it to a short drive letter before building. Windows CI uses this approach:

```powershell
subst S: C:\path\to\entropy
S:
```

When finished, switch to another drive and remove the mapping:

```powershell
C:
subst S: /D
```

## Build with Presets

Use the presets in [CMakePresets.json](CMakePresets.json) for local builds. Configure and build the dependencies first,
then configure and build the app in the same directory.

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

The presets use ccache if it is installed. To limit memory use, specify a job count such as `--parallel 8` on both
dependency and app builds. For dependencies, you can also set `Entropy_SUPERBUILD_PARALLEL` at configure time to limit
builds run inside ExternalProject targets.

### Preset reference

| Preset | Build directory | Purpose |
| --- | --- | --- |
| `deps-debug` | `build-debug` | Configure/build Debug dependencies |
| `app-debug` | `build-debug` | Configure/build the Debug app and tests after `deps-debug` |
| `deps-release` | `build-release` | Configure/build Release dependencies |
| `app-release` | `build-release` | Configure/build the Release app and tests after `deps-release` |
| `package-release` | `build-release` | Build the Release package target after `app-release` |

The `deps-*` presets set `Entropy_SUPERBUILD=ON`. The matching `app-*` presets switch it to `OFF`. Both stages must use
the same build directory.


## Compiler Caching

A compiler cache can make repeated builds much faster, especially while compiling ITK and VTK. It reuses object files
only when the source, compiler, and relevant options match.

### ccache

[ccache](https://ccache.dev/) is the recommended cache on macOS and Linux. Install it with `sudo port install ccache` or
`brew install ccache` on macOS, `sudo apt-get install ccache` on Ubuntu, or `sudo dnf install ccache` on Fedora. The
project presets set `Entropy_USE_CCACHE=ON`, so CMake uses ccache automatically when it is on `PATH`. Disable it with
`-D Entropy_USE_CCACHE=OFF`.

A 20 GB local cache is a good starting point for one built type.

```sh
ccache --max-size=20G
ccache --show-stats
```

ccache stores data in its platform-specific default cache directory. Set `CCACHE_DIR` before configuring to choose a
different location. Use `ccache --clear` only when the cache must be discarded.

### sccache

[sccache](https://github.com/mozilla/sccache) is used by the Windows Debug CI job. Install sccache and Ninja, then run
the commands below from an x64 Visual Studio 2022 developer shell so `cl`, `ninja`, and `sccache` are on `PATH`.
Configure both stages with Ninja and an explicit sccache launcher:

```powershell
$env:SCCACHE_CACHE_SIZE = "20G"
cmake --preset deps-debug -G Ninja `
  -D CMAKE_C_COMPILER=cl `
  -D CMAKE_CXX_COMPILER=cl `
  -D Entropy_USE_CCACHE=OFF `
  -D CMAKE_C_COMPILER_LAUNCHER=sccache `
  -D CMAKE_CXX_COMPILER_LAUNCHER=sccache
cmake --build --preset deps-debug --parallel

cmake --preset app-debug -G Ninja `
  -D CMAKE_C_COMPILER=cl `
  -D CMAKE_CXX_COMPILER=cl `
  -D Entropy_USE_CCACHE=OFF `
  -D CMAKE_C_COMPILER_LAUNCHER=sccache `
  -D CMAKE_CXX_COMPILER_LAUNCHER=sccache
cmake --build --preset app-debug --parallel
sccache --show-stats
```

Set `SCCACHE_DIR` to move the local cache. Do not configure ccache and sccache as launchers at the same time. GitHub
Actions uses a 2 GB ccache limit for macOS and Linux jobs and a 5 GB sccache limit for the Windows Debug job because CI
caches have tight storage constraints.


## Run Entropy

Launch the Debug app from the build tree:

```sh
open build-debug/bin/Entropy.app # macOS
build-debug/bin/entropy # Linux
```

```powershell
.\build-debug\bin\Debug\entropy.exe # Windows with the Visual Studio generator
.\build-debug\bin\entropy.exe       # Windows with Ninja
```

For a Release build, replace `build-debug` with `build-release`. With the Visual Studio generator, also replace `Debug`
with `Release` in the executable path.


## Run Tests

The application build also builds the unit tests. Run them with

```sh
ctest --test-dir build-debug -C Debug --parallel --output-on-failure
```

For Release builds, use:

```sh
ctest --test-dir build-release -C Release --parallel --output-on-failure
```

The `-C` argument selects the configuration on multi-config generators, such as Visual Studio.

Rendering tests use [glslangValidator](https://github.com/KhronosGroup/glslang) when it is available at CMake
configuration time. The validator compiles and links every assembled GLSL shader variant without launching Entropy or
requiring a GPU context. Install it with `brew install glslang` on macOS or `sudo apt-get install glslang-tools` on
Ubuntu, then reconfigure the application build to enable the test. Ubuntu CI installs it for the Debug test job.


## Static Analysis

Entropy runs [cppcheck](https://www.cppcheck.com/) and [clang-tidy](https://clang.llvm.org/extra/clang-tidy/) as
separate jobs in the [Static Analysis](.github/workflows/static-analysis.yml) workflow. Both can also be run locally on
macOS and Linux.

### cppcheck

cppcheck complements compiler warnings and clang-tidy with its own data flow and whole program analysis. It catches
problems involving lifetime, initialization, control flow, portability, performance, and concurrency.

Install cppcheck on macOS with either MacPorts or Homebrew: `sudo port install cppcheck` or `brew install cppcheck`.
On Ubuntu: `sudo apt-get install cppcheck`.

Use a separate build directory for cppcheck. The app configure generates the compilation database used by the target:

```sh
cmake --preset deps-debug -B build-cppcheck
cmake --build build-cppcheck --parallel
cmake --preset app-debug -B build-cppcheck -D Entropy_ENABLE_CPPCHECK=ON
cmake --build build-cppcheck --target cppcheck
```

The target enables error, warning, style, performance, portability, inconclusive, and thread safety checks. Known
project-wide false positives are listed in [.cppcheck-suppressions](.cppcheck-suppressions). Local suppressions use
cppcheck's `cppcheck-suppress` comment syntax. The target in [CMakeLists.txt](CMakeLists.txt) skips external,
generated, and Objective-C++ sources.

CI runs cppcheck in its own Ubuntu 24.04 x86_64 job and uploads `cppcheck.log` as the `cppcheck-log` artifact.
The local and CI targets fail when cppcheck reports an unsuppressed finding.

### clang-tidy

clang-tidy analyzes the code with Clang's compiler model. Entropy enables compiler diagnostics, the Clang Static
Analyzer, and a selected set of bug, security, modernization, performance, portability, and readability checks.

Install clang-tidy 19 or newer on macOS with either MacPorts or Homebrew: `sudo port install clang-19` or
`brew install llvm`. On Ubuntu 24.04: `sudo apt-get install clang-tidy-19`.

Homebrew installs LLVM keg-only, so put `$(brew --prefix llvm)/bin` on `PATH`. Run `clang-tidy --version` before
configuring to confirm that the expected version is selected.

Use a dedicated build directory to configure and build the dependencies, then enable clang-tidy while configuring and
building the application:

```sh
cmake --preset deps-debug -B build-clang-tidy
cmake --build build-clang-tidy --parallel
cmake --preset app-debug -B build-clang-tidy -D Entropy_ENABLE_CLANG_TIDY=ON
cmake --build build-clang-tidy --parallel
```

The enabled checks and warning-as-error rules are in [.clang-tidy](.clang-tidy). A few known third-party diagnostics
are exempted there. External and generated targets are excluded, and system headers are not analyzed. To suppress a
warning in code, use `NOLINT`, `NOLINTNEXTLINE`, or a scoped `NOLINTBEGIN`/`NOLINTEND` with the check name.

CI runs clang-tidy during a Debug application build in the Ubuntu 24.04 x86_64 job. Findings fail that job and the
full output is uploaded as the `clang-tidy-log` artifact.


## Include Hygiene

[Include What You Use](https://include-what-you-use.org/) reports missing and unnecessary C++ includes. Install it with
`sudo port install include-what-you-use` or `brew install include-what-you-use` on macOS, or
`sudo apt-get install iwyu` on Ubuntu.

Run IWYU through a dedicated Debug build:

```sh
cmake --preset deps-debug -B build-iwyu
cmake --build build-iwyu --parallel
cmake --preset app-debug -B build-iwyu -D Entropy_ENABLE_IWYU=ON -D Entropy_USE_CCACHE=OFF
cmake --build build-iwyu --parallel
```

The default options favor direct quoted includes and skip forward declaration recommendations. IWYU's include
recommendations are advisory (`--error=0`), but parser or compiler failures fail the build. The app stage disables
ccache so IWYU runs directly. External and generated targets are excluded in [CMakeLists.txt](CMakeLists.txt). The
[IWYU workflow](.github/workflows/iwyu.yml) runs on Ubuntu 24.04 only when started manually and uploads its output as
the `iwyu-log` artifact.


## Packaging

Build and test the Release app before packaging it:

```sh
cmake --preset deps-release
cmake --build --preset deps-release --parallel

cmake --preset app-release
cmake --build --preset app-release --parallel
ctest --test-dir build-release -C Release --parallel --output-on-failure
cmake --build --preset package-release --parallel
```

See [PACKAGING.md](PACKAGING.md) for package formats, release artifact names, signing notes, and GitHub release
behavior.


## Coverage

Coverage builds are optional and should use their own build directory. Install the reporting tool required by the
selected compiler before configuring:

- Clang or Apple Clang requires `llvm-cov` and `llvm-profdata`. Xcode provides both on macOS.
- GCC requires `gcov` plus either gcovr 7 or newer, or both `lcov` and `genhtml`.
- MSVC requires [OpenCppCoverage](https://github.com/OpenCppCoverage/OpenCppCoverage) on `PATH`.

Configure the Debug dependencies and application, then run a coverage target:

```sh
cmake --preset deps-debug -B build-coverage
cmake --build build-coverage --parallel
cmake --preset app-debug -B build-coverage -D Entropy_ENABLE_COVERAGE=ON
cmake --build build-coverage --target coverage --config Debug --parallel
```

Coverage backend selection is automatic by default:

| Compiler | Default backend |
| --- | --- |
| Clang or AppleClang | [LLVM source-based coverage](https://clang.llvm.org/docs/SourceBasedCodeCoverage.html) |
| GCC | [gcov-compatible coverage](https://gcc.gnu.org/onlinedocs/gcc/Gcov.html) |
| MSVC | [OpenCppCoverage](https://github.com/OpenCppCoverage/OpenCppCoverage) |

The `coverage` and `coverage-html` targets build and run the registered tests before producing a report. The first
writes output under `build-coverage/coverage/`. The second writes an HTML report under `build-coverage/coverage/html/`.


## Local Hygiene Checks

Entropy uses [pre-commit](https://pre-commit.com/) for lightweight checks before committing. The default hooks run
[codespell](https://github.com/codespell-project/codespell) and
[clang-format](https://clang.llvm.org/docs/ClangFormat.html). clang-format updates files in place, so review and stage
any formatting changes. The Markdown link check is a manual local hook because it uses the network and can fail when
external sites are temporarily unavailable.

Install and enable it:

```sh
python3 -m pip install pre-commit
pre-commit install
```

The clang-format hook uses the clang-format executable on `PATH`. Install it separately with your platform package
manager. pre-commit installs the configured codespell environment automatically.

Run the default hooks manually:

```sh
pre-commit run --all-files
```

Run the Markdown link check when editing documentation:

```sh
pre-commit run lychee-doc-links --hook-stage manual
```

The manual link hook expects lychee to be installed and available on `PATH`. CI runs codespell and the Markdown link
check on pull requests, weekly, and on manual dispatch.


## CMake Options

Pass options at configure time with `-DNAME=value` or put local overrides in `CMakeUserPresets.json`.

### General Build Options

| Option | Default | Stage | Purpose |
| --- | --- | --- | --- |
| `CMAKE_BUILD_TYPE` | `RelWithDebInfo` outside presets | Configure | Selects `Debug`, `Release`, `RelWithDebInfo`, or `MinSizeRel` for single-config generators |
| `CMAKE_VERBOSE_MAKEFILE` | `OFF` | Both | Prints full native build commands for Makefile generators |
| `BUILD_SHARED_LIBS` | `OFF` outside presets, `ON` in presets | Both | Chooses shared or static libraries for targets that honor the standard CMake option |
| `BUILD_TESTING` | `ON` | Application | Enables unit-test targets |
| `Entropy_USE_CCACHE` | `ON` | Both | Uses ccache as the compiler launcher when available |

### Dependency Options

| Option | Default | Purpose |
| --- | --- | --- |
| `Entropy_SUPERBUILD` | `ON` | Selects the build stage. `ON` builds dependencies. `OFF` builds Entropy against dependencies already in the build tree |
| `Entropy_SUPERBUILD_CONFIG` | `Release` | Selects the dependency configuration for multi-config generators such as Visual Studio, Xcode, and Ninja Multi-Config |
| `Entropy_SUPERBUILD_PARALLEL` | empty | Sets parallelism inside ExternalProject dependency builds. Empty defers to `CMAKE_BUILD_PARALLEL_LEVEL` or the native tool default |
| `Entropy_STATIC_BUNDLED_DEPENDENCIES` | Platform default: `ON` on macOS/Linux, `OFF` on Windows. Release presets: `ON` | Builds bundled dependencies as static libraries where practical. Qt and system libraries remain dynamic |

### Code-Quality Options

| Option | Default | Purpose |
| --- | --- | --- |
| `Entropy_ENABLE_CPPCHECK` | `OFF` | Adds the cppcheck static-analysis target |
| `Entropy_CPPCHECK_OPTIONS` | project default | Options passed to cppcheck |
| `Entropy_CPPCHECK_JOBS` | `4` | Number of parallel cppcheck analysis jobs |
| `Entropy_ENABLE_CLANG_TIDY` | `OFF` | Runs clang-tidy during C++ compilation |
| `Entropy_CLANG_TIDY_OPTIONS` | `--quiet` | Extra options passed to clang-tidy |
| `Entropy_ENABLE_IWYU` | `OFF` | Runs Include What You Use during C++ compilation |
| `Entropy_IWYU_OPTIONS` | project default | Extra options passed to Include What You Use |
| `Entropy_IWYU_COMPILER_OPTIONS` | empty | Compiler compatibility options passed only to Include What You Use |

### Coverage Options

| Option | Default | Purpose |
| --- | --- | --- |
| `Entropy_ENABLE_COVERAGE` | `OFF` | Adds coverage instrumentation and report targets |
| `Entropy_COVERAGE_MODE` | `AUTO` | Selects `AUTO`, `LLVM`, `GCOV`, or `OPENCPPCOVERAGE` |
| `Entropy_COVERAGE_EXCLUDE_REGEX` | project default | Excludes external, generated, test, and system files from reports |

### Application Options

| Option | Default | Purpose |
| --- | --- | --- |
| `Entropy_ENABLE_TRACE_LOGGING` | `OFF` | Compiles trace-level logging calls into Entropy |
| `Entropy_GLAD_GL_VERSION` | `3.3` | Selects the vendored GLAD OpenGL Core loader version: `3.3`, `4.1`, or `4.6` |
| `Entropy_GLAD_GL_DEBUG` | `false` | Uses the debug GLAD loader variant |

Standard CMake variables such as `CMAKE_INSTALL_PREFIX`, `CMAKE_OSX_DEPLOYMENT_TARGET`, `CMAKE_PREFIX_PATH`, and
generator selection also work normally. Packaging options are documented in [PACKAGING.md](PACKAGING.md).


## Continuous Integration

CI workflows are under [.github/workflows](.github/workflows). They build and test Entropy on macOS, Windows, Ubuntu,
and Fedora.

Pull requests and pushes to `main` run formatting, Debug builds, unit tests, static analysis, and documentation checks.
Fedora builds and IWYU are manual. Scheduled or manual jobs cover newer platforms and package validation. Coverage jobs
are manual. Tags trigger the release workflow described in [PACKAGING.md](PACKAGING.md).

The main CI build matrix is:

| Platform | Runner | Toolchain | Scope |
| --- | --- | --- | --- |
| macOS arm64 | `macos-14` | Apple Clang through Xcode | Debug build and tests, release packages, optional coverage |
| macOS x86_64 | `macos-15-intel` | Apple Clang through Xcode | Debug build and tests, release packages |
| macOS arm64 compatibility | `macos-26` | Apple Clang through Xcode | Scheduled/manual Debug build and tests on a newer macOS runner |
| Windows x86_64 | `windows-2022` | Visual Studio 2022 / MSVC v143 | Debug build and tests, release packages, optional coverage |
| Windows x86_64 compatibility | `windows-2025` | Visual Studio 2026 / MSVC | Scheduled/manual Debug build and tests on a newer Windows runner |
| Ubuntu 22.04 x86_64 | `ubuntu-22.04` | `gcc-13` / `g++-13` | Debug build and tests, release packages, and manual coverage |
| Ubuntu 24.04 x86_64 | `ubuntu-24.04` | GCC 13 and Clang 19 | clang-tidy and cppcheck analysis, plus manual IWYU analysis |
| Fedora 43 x86_64 | `fedora:43` container on `ubuntu-24.04` | GCC 15 | Manual Debug build and tests, manual release packages, and tag-driven Fedora release packages |

Ubuntu 22.04 installs GCC 13 from the Ubuntu toolchain PPA. The analysis jobs on Ubuntu 24.04 also build with GCC 13.
IWYU uses its own Clang-based analyzer. Ubuntu 24.04 hosts the Fedora container jobs. Ubuntu 22.04 remains the primary
Linux packaging target because release packages should be built on the oldest supported distribution.

macOS release artifacts are built separately for `arm64` and `x86_64`. Entropy does not publish a universal macOS
binary.

Check the workflow files for the exact runner images, package installation commands, cache keys, and artifact names.


## Third-Party Dependencies

The dependency stage builds pinned third-party libraries from source. Their versions, source URLs, and licenses are in
[THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md). Entropy does not use Git submodules. The first build downloads the
source archives. Later builds reuse those downloads and completed ExternalProject outputs from the same build directory.
