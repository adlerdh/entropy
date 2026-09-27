# Testing Entropy

Entropy's tests check individual features, workflows that combine features, rendering, and display scaling. Most run without opening a window. Rendering tests create an OpenGL context, and display-scaling tests move a window between two monitors.

## Run the tests

From the repository root, using an existing Release build:

```sh
cmake --build build-release --parallel 8
ctest --test-dir build-release --parallel 8 --timeout 120 --output-on-failure
```

To run just the tests that combine several features:

```sh
build-release/bin/TestWorkflows --reporter compact
```

CTest finds and runs the tests built by CMake. If a test fails, `--output-on-failure` shows its output.

`TestWorkflows` runs sequences of actions through the application's own code. It creates small images, changes application state, saves files in temporary folders, and reads them back to check the results.

## What they check

Alongside tests for individual features, we test situations like these:

- Save and reopen a project without losing transforms, warps, or the selected segmentation.
- Edit a segmentation, send it to registration, export it, and keep editing without changing the data already sent.
- Change images or frames while background work is running, and reject results that no longer apply.
- Replace a warp, pick a mesh, and export it with the correct shape.
- Turn a tilted annotation with a hole into a segmentation and mesh, then export and reload it.
- Interrupt a mouse drag or change display scaling without leaving interaction or layout in a broken state.
- Handle failed saves, failed registration jobs, and cancellation without losing data or reporting false success.

## Graphics tests

On a Mac, run these from a logged-in desktop session to include OpenGL tests:

```sh
ENTROPY_TEST_GL33=1 ctest --test-dir build-release --timeout 120 --output-on-failure
```

`ENTROPY_TEST_GL33=1` enables the OpenGL tests on macOS. Shader compilation also needs `glslangValidator` installed before configuring the build.

Linux CI uses software OpenGL and a virtual display:

```sh
LIBGL_ALWAYS_SOFTWARE=1 xvfb-run -a python3 test/run_required_graphics.py build-debug
```

The graphics tests compile shaders, draw test scenes, and compare the results with known colors, depths, and histogram counts. They also check that graphics resources are created and cleaned up correctly.

The CI script requires every graphics test to run and pass. It saves the results in `required-graphics.xml`.

## Tests with two monitors

These tests move a window between monitors and check that the UI follows each monitor's display scale. They also check that repeated scale updates don't keep enlarging or shrinking the UI. You need a logged-in desktop and two monitors that report different scales.

```sh
cmake --preset app-release -DBUILD_TESTING=ON -DEntropy_BUILD_DISPLAY_TESTS=ON
cmake --build build-release --parallel 8
ENTROPY_TEST_GL33=1 python3 test/run_required_graphics.py build-release --configuration Release --display-transitions
```

The test checks the monitor setup before moving the window. Linux needs to allow the test to move windows, as X11 does.

The manual `platform-validation.yml` CI workflow runs these checks on separately configured desktop machines for macOS, Windows, and Linux. Each needs the `entropy-display` and matching OS labels, build dependencies, Python, and glslang.

## Adding tests

Reuse the helpers in `support/`:

- `AnalyticImages.h` makes small images with known values, so tests can calculate the expected result.
- `TempDirectory.h` gives each test its own folder and cleans it up afterward.
- `CompletionGate.h` pauses background work at a known point. A test can change application state, resume the work, and check how the result is handled. Use this instead of sleeps, so the test doesn't depend on machine speed.
