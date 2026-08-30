# Building Entropy

This guide explains how to configure, build, and test Entropy from source. Entropy uses CMake and a two-stage dependency
build, following the CMake "superbuild" pattern: first build the pinned third-party dependencies, then configure and
build the Entropy app against those dependencies.

For installer, archive, signing, and GitHub release details, see [PACKAGING.md](PACKAGING.md).

Run all commands in this guide from the repository root unless stated otherwise.

## Requirements

- CMake 3.28 or newer
- A C++23 compiler and matching native build tool, such as Make, Ninja, Visual Studio, or Xcode
- Git and network access for the dependency build
- 8-10 GB of available disk space for a clean build
- An OpenGL 3.3-capable graphics driver and graphical session to run Entropy

A completed Release build tree is roughly 2-3 GB. A clean build needs more temporary space because downloaded archives,
source trees, build trees, and install trees coexist during the dependency stage.

Entropy is developed and tested on:

| Platform | Toolchain |
| --- | --- |
| macOS arm64 and x86_64 | Apple Clang 15.0.0 or newer |
| Windows x86_64 | Visual Studio 2022 17.3.4 or newer |
| Ubuntu 22.04 x86_64 | GCC 13 or newer |
| Ubuntu 24.04 x86_64 | GCC 13 or newer |
| Fedora 43 x86_64 | GCC 13 or newer |

Other systems may work if they provide a C++23 compiler and the required OpenGL/windowing development libraries.

## Platform Setup

On macOS, install Xcode and its command-line tools. CMake and Git may be installed with MacPorts, Homebrew, or their
official installers.

On Windows, install Visual Studio 2022 with the **Desktop development with C++** workload, plus CMake and Git if they
are not already available on `PATH`.

On Ubuntu 22.04, install the development packages needed for OpenGL, windowing, native file dialogs, OpenSSL, and the
dependency build. The list includes the optional [ccache](https://ccache.dev/) compiler cache described later in this
guide. GCC 13 is supplied by the Ubuntu toolchain PPA:

```sh
sudo apt-get update
sudo apt-get install --no-install-recommends -y software-properties-common
sudo add-apt-repository --yes ppa:ubuntu-toolchain-r/test
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

Select GCC 13 for the current shell before configuring:

```sh
export CC=gcc-13
export CXX=g++-13
```

On Ubuntu 24.04, GCC 13 is available from the standard repositories. Skip the `software-properties-common` and
`add-apt-repository` commands, then install the same build packages and select `gcc-13` and `g++-13` as shown above.

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

The supported local build path is through [CMakePresets.json](CMakePresets.json). Configure and build dependencies
first, then configure and build the app in the same build directory.

CMake chooses the platform's default generator unless `-G` is specified. Select the compiler and generator before the
first dependency configure, then use the same choices for the application configure. A configured build directory
cannot safely switch to a different generator or compiler.

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

Each configure command prints the selected compiler and build settings. The presets enable compiler caching when
ccache is installed, as described below.

A good starting point for `--parallel` is the number of logical CPU cores minus two, with a minimum of one. This leaves
some processing capacity for the operating system and other applications. Calculate the value with:

On Linux:

```sh
BUILD_JOBS=$(( $(nproc) - 2 ))
[ "$BUILD_JOBS" -lt 1 ] && BUILD_JOBS=1
```

On macOS:

```sh
BUILD_JOBS=$(( $(sysctl -n hw.logicalcpu) - 2 ))
[ "$BUILD_JOBS" -lt 1 ] && BUILD_JOBS=1
```

On Windows PowerShell:

```powershell
$BuildJobs = [Math]::Max(1, [Environment]::ProcessorCount - 2)
```

Pass the resulting number to builds, for example with `--parallel "$BUILD_JOBS"` in a POSIX shell or
`--parallel $BuildJobs` in PowerShell. An unqualified `--parallel` uses the native build tool's default parallelism. Use
a lower value if the dependency build exhausts available memory.

## CMake Presets

| Preset | Build directory | Purpose |
| --- | --- | --- |
| `deps-debug` | `build-debug` | Configure/build Debug dependencies |
| `app-debug` | `build-debug` | Configure/build the Debug app and tests after `deps-debug` |
| `deps-release` | `build-release` | Configure/build Release dependencies |
| `app-release` | `build-release` | Configure/build the Release app and tests after `deps-release` |
| `package-release` | `build-release` | Build the Release package target after `app-release` |

The `deps-*` presets configure with `Entropy_SUPERBUILD=ON`. After the dependencies finish, the corresponding `app-*`
preset reconfigures the same directory with `Entropy_SUPERBUILD=OFF`. Do not configure the application stage in a new
directory unless that directory already contains a completed dependency build.

## Compiler Caching

A compiler cache reuses object files from previous compilations when the source, compiler, and relevant options have
not changed. It can greatly shorten repeated Entropy builds, especially while compiling ITK and VTK dependencies. It
does not change the resulting binaries.

### ccache

[ccache](https://ccache.dev/) is the recommended cache on macOS and Linux. Install it with `sudo port install ccache` or
`brew install ccache` on macOS, `sudo apt-get install ccache` on Ubuntu, or `sudo dnf install ccache` on Fedora. The
project presets set `Entropy_USE_CCACHE=ON`, so CMake uses ccache automatically when it is on `PATH`. Disable it with
`-D Entropy_USE_CCACHE=OFF`.

A 20 GB local cache is a good starting point. Use 40 GB when regularly building both Debug and Release configurations
or switching among several branches. Smaller caches remain useful but discard reusable dependency objects sooner.

```sh
ccache --max-size=20G
ccache --show-stats
```

ccache stores data in its platform-specific default cache directory. Set `CCACHE_DIR` before configuring to choose a
different location. Use `ccache --clear` only when the cache must be discarded.

### sccache

[sccache](https://github.com/mozilla/sccache) is the recommended cache for MSVC builds and is used by Entropy's Windows Debug CI job. Install sccache and
Ninja, and ensure both executables are on `PATH`. Then disable Entropy's ccache integration and select sccache through
CMake's standard launcher variables during both configure stages:

```powershell
$env:SCCACHE_CACHE_SIZE = "20G"
cmake --preset deps-debug -G Ninja `
  -D Entropy_USE_CCACHE=OFF `
  -D CMAKE_C_COMPILER_LAUNCHER=sccache `
  -D CMAKE_CXX_COMPILER_LAUNCHER=sccache
cmake --build --preset deps-debug --parallel

cmake --preset app-debug -G Ninja `
  -D Entropy_USE_CCACHE=OFF `
  -D CMAKE_C_COMPILER_LAUNCHER=sccache `
  -D CMAKE_CXX_COMPILER_LAUNCHER=sccache
cmake --build --preset app-debug --parallel
sccache --show-stats
```

Set `SCCACHE_DIR` to move the local cache. Do not configure ccache and sccache as launchers at the same time. GitHub
Actions uses a 2 GB ccache limit for macOS and Linux jobs and a 5 GB sccache limit for the Windows Debug job because CI
caches have tighter storage constraints than developer machines.

## Reconfiguration and Clean Builds

Rerun the appropriate configure preset after changing CMake files or configuration options. CMake preserves cached
values in the build directory, so inspect unexpected settings with `cmake -LA -N build-debug`.

Use a fresh build directory after changing the compiler, generator, target architecture, or dependency linkage model.
A fresh directory is also the most reliable recovery from a stale or interrupted dependency build. The `build-debug`,
`build-release`, and other `build-*` directories contain generated files only and can be deleted without affecting
source files. Reconfigure and rebuild the dependency stage before rebuilding the application stage.

## Windows Path Length

ITK may stop early on Windows if the source or build directory path is too long. The simplest fix is to keep the
checkout near the root of a drive, e.g.: `C:\entropy`.

Entropy's Windows CI also maps the checkout to a short temporary drive letter before building:

```powershell
subst S: C:\path\to\entropy
S:
```

Build from the mapped drive, then remove the mapping when finished:

```powershell
C:
subst S: /D
```

If Windows long paths are enabled on the system, then ITK's path length check can also be disabled by passing
`ITK_SKIP_PATH_LENGTH_CHECKS:BOOL=ON` to ITK during the dependency build. Prefer the shorter path approach unless the
system is already configured for long paths.

## Run Entropy

Launch the Debug app from the build tree:

```sh
open build-debug/bin/Entropy.app # macOS
build-debug/bin/entropy # Linux
```

```powershell
.\build-debug\bin\Debug\entropy.exe # Windows with the Visual Studio generator
```

For a Release build, replace `build-debug` with `build-release` and, on Windows, replace `Debug` with `Release`.

## Run Tests

The application build also builds the unit tests. Run the Debug tests with CTest:

```sh
ctest --test-dir build-debug --parallel --output-on-failure
```

For release builds, use `build-release`:

```sh
ctest --test-dir build-release --parallel --output-on-failure
```

On multi-config generators such as Visual Studio, specify the configuration:

```sh
ctest --test-dir build-debug -C Debug --parallel --output-on-failure
ctest --test-dir build-release -C Release --parallel --output-on-failure
```

Use the same logical-cores-minus-two value recommended for builds as the CTest `--parallel` level. For example, a
machine with 14 logical cores would use `--parallel 12`. Lower the value if concurrently running tests compete for
memory or graphics resources.

## Static Analysis

Entropy runs [cppcheck](https://www.cppcheck.com/) and
[clang-tidy](https://clang.llvm.org/extra/clang-tidy/) as independent jobs in the
[Static Analysis](.github/workflows/static-analysis.yml) workflow. Both can also be run locally on macOS and Ubuntu.

### cppcheck

cppcheck performs compiler-independent static analysis, including data-flow and whole-program checks that complement
the compiler and clang-tidy. It is useful for finding lifetime, initialization, control-flow, portability, performance,
and concurrency problems.

Install cppcheck on macOS with either MacPorts or Homebrew: `sudo port install cppcheck` or `brew install cppcheck`.
On Ubuntu: `sudo apt-get install cppcheck`.

Use a dedicated build directory to configure the dependencies and application, generate the compilation database, and
run the cppcheck target:

```sh
cmake --preset deps-debug -B build-cppcheck
cmake --build build-cppcheck --parallel
cmake --preset app-debug -B build-cppcheck -D Entropy_ENABLE_CPPCHECK=ON
cmake --build build-cppcheck --target cppcheck
```

The target enables error, warning, style, performance, portability, and inconclusive analysis, inline suppressions,
and the thread-safety addon. Project-wide reviewed false positives are in
[.cppcheck-suppressions](.cppcheck-suppressions), scoped as narrowly as practical. Source-local suppressions use
cppcheck's `cppcheck-suppress` comment syntax. External, generated, and Objective-C++ sources that cppcheck cannot
meaningfully analyze are excluded by the target in [CMakeLists.txt](CMakeLists.txt).

CI runs cppcheck in its own Ubuntu 24.04 x86_64 job and uploads `cppcheck.log` as the `cppcheck-log` artifact. The local
and CI targets fail when cppcheck reports an unsuppressed finding.

### clang-tidy

clang-tidy analyzes C++ using Clang's compiler model. Entropy uses it for compiler diagnostics, Clang Static Analyzer
checks, and targeted bug-prone, security, Core Guidelines, modernization, performance, portability, and readability
checks.

Install clang-tidy on macOS with either MacPorts or Homebrew: `sudo port install clang-19` or `brew install llvm`.
On Ubuntu: `sudo apt-get install clang-tidy`.

Homebrew installs LLVM keg-only, so ensure `$(brew --prefix llvm)/bin` is on `PATH`. Confirm the intended executable is
selected with `clang-tidy --version` before configuring.

Use a dedicated build directory to configure and build the dependencies, then enable clang-tidy while configuring and
building the application:

```sh
cmake --preset deps-debug -B build-clang-tidy
cmake --build build-clang-tidy --parallel
cmake --preset app-debug -B build-clang-tidy -D Entropy_ENABLE_CLANG_TIDY=ON
cmake --build build-clang-tidy --parallel
```

The exact checks and analyzer options are defined in [.clang-tidy](.clang-tidy), and every diagnostic is treated as an
error. External, generated, and system headers are excluded. When a narrowly justified source suppression is necessary,
use `NOLINT`, `NOLINTNEXTLINE`, or a scoped `NOLINTBEGIN`/`NOLINTEND` with the exact check name. Project-wide check
configuration and exclusions are in [.clang-tidy](.clang-tidy).

CI runs clang-tidy during a Debug application build in its own Ubuntu 24.04 x86_64 job. Findings fail that job, and the
full output is uploaded as the `clang-tidy-log` artifact.

## Include Hygiene

[Include What You Use](https://include-what-you-use.org/) reports missing and unnecessary C++ includes. Install it with
`sudo port install include-what-you-use` or `brew install include-what-you-use` on macOS, or
`sudo apt-get install iwyu` on Ubuntu.

Run IWYU through a dedicated Debug build:

```sh
cmake --preset deps-debug -B build-iwyu
cmake --build build-iwyu --parallel
cmake --preset app-debug -B build-iwyu -D Entropy_ENABLE_IWYU=ON
cmake --build build-iwyu --parallel
```

The default options favor direct quoted includes, avoid forward-declaration recommendations, and report suggestions
without making the compiler command fail. External and generated targets are excluded in
[CMakeLists.txt](CMakeLists.txt). The Ubuntu 24.04
[Include What You Use](.github/workflows/iwyu.yml) workflow runs for pull requests and pushes to `main`, weekly, and
when manually dispatched. The job remains advisory because recommendations vary between IWYU releases. It uploads its
full output as the `iwyu-log` artifact.

## Packaging

Create release packages with:

```sh
cmake --preset deps-release
cmake --build --preset deps-release --parallel

cmake --preset app-release
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
cmake --build build-coverage --target coverage --parallel
```

Coverage backend selection is automatic by default:

| Compiler | Default backend |
| --- | --- |
| Clang or AppleClang | [LLVM source-based coverage](https://clang.llvm.org/docs/SourceBasedCodeCoverage.html) |
| GCC | [gcov-compatible coverage](https://gcc.gnu.org/onlinedocs/gcc/Gcov.html) |
| MSVC | [OpenCppCoverage](https://github.com/OpenCppCoverage/OpenCppCoverage) |

Both targets build and run the registered unit tests before producing a report. The `coverage` target writes
machine-readable output under `build-coverage/coverage/`. Use the `coverage-html` target to additionally write an HTML
report under `build-coverage/coverage/html/`.

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

Pass options at configure time with `-DNAME=value`, or put local overrides in `CMakeUserPresets.json`.

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
| `Entropy_SUPERBUILD_PARALLEL` | empty | Sets parallelism inside ExternalProject dependency builds. An empty value lets the native build tool choose |
| `Entropy_STATIC_BUNDLED_DEPENDENCIES` | `ON` on macOS and Linux, `OFF` on Windows | Builds bundled dependencies as static libraries where practical. Qt and system libraries remain dynamic |

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

Pull requests run formatting checks, Debug builds, and unit tests on the primary platforms, plus static analysis,
include hygiene, and text-hygiene checks. Pushes to `main` repeat these checks and the primary platform builds and
tests. Scheduled and manually dispatched workflows provide broader compatibility, coverage, and package validation.
Official tag-driven builds are handled by the release workflow described in [PACKAGING.md](PACKAGING.md).

The main CI build matrix is:

| Platform | Runner | Toolchain | Scope |
| --- | --- | --- | --- |
| macOS arm64 | `macos-14` | Apple Clang through Xcode | Debug build and tests, release packages, optional coverage |
| macOS x86_64 | `macos-15-intel` | Apple Clang through Xcode | Debug build and tests, release packages |
| macOS arm64 compatibility | `macos-26` | Apple Clang through Xcode | Scheduled/manual Debug build and tests on a newer macOS runner |
| Windows x86_64 | `windows-2022` | Visual Studio 2022 / MSVC v143 | Debug build and tests, release packages, optional coverage |
| Windows x86_64 compatibility | `windows-2025` | Visual Studio 2026 / MSVC | Scheduled/manual Debug build and tests on a newer Windows runner |
| Ubuntu 22.04 x86_64 | `ubuntu-22.04` | `gcc-13` / `g++-13` | Debug build and tests, release packages, and primary coverage |
| Ubuntu 24.04 x86_64 | `ubuntu-24.04` | `gcc-13` / `g++-13` | Debug build and tests with clang-tidy, cppcheck analysis, and IWYU analysis |
| Fedora 43 x86_64 | `fedora:43` container on `ubuntu-24.04` | GCC 15 | Manual Debug build and tests, manual release packages, and tag-driven Fedora release packages |

The Ubuntu 22.04 workflow installs `gcc-13` and `g++-13` from the Ubuntu toolchain PPA. Ubuntu 24.04 runs a complete
Debug build and test suite with clang-tidy, runs cppcheck, and hosts the Fedora container jobs. Ubuntu 22.04 remains the
primary Linux packaging target because release packages should be built on the oldest supported distribution.

macOS release artifacts are built separately for `arm64` and `x86_64`. Entropy does not publish a universal macOS
binary.

> The workflow files are the source of truth for exact runner images, package installation commands, cache keys, and
> artifact upload names.

## Third-Party Dependencies

Entropy builds pinned third-party dependencies from source during the dependency stage. Versions, source URLs, and
license notes are documented in [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md). The project does not use Git
submodules. The first dependency build downloads versioned source archives, while subsequent builds reuse archives and
completed ExternalProject outputs from the same build directory.
