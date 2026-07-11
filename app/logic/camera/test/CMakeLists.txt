add_executable(TestCamera
  Camera3DInteractionTests.cpp
  Camera3DControlsTests.cpp
  CameraFrustumSliceTests.cpp
  CameraRecenterTests.cpp
  RaycastIsoSurfacePickerTests.cpp
)

target_sources(TestCamera PRIVATE
  "${entropy_APP_DIR}/logic/camera/Camera.cpp"
  "${entropy_APP_DIR}/logic/camera/Camera3DControls.cpp"
  "${entropy_APP_DIR}/logic/camera/CameraFrustumSlice.cpp"
  "${entropy_APP_DIR}/logic/camera/Camera3DInteraction.cpp"
  "${entropy_APP_DIR}/logic/camera/CameraHelpers.cpp"
  "${entropy_APP_DIR}/logic/camera/CameraTypes.cpp"
  "${entropy_APP_DIR}/logic/camera/MathUtility.cpp"
  "${entropy_APP_DIR}/logic/camera/OrthogonalProjection.cpp"
  "${entropy_APP_DIR}/logic/camera/PerspectiveProjection.cpp"
  "${entropy_APP_DIR}/logic/camera/Projection.cpp"
  "${entropy_APP_DIR}/logic/camera/RaycastIsoSurfacePicker.cpp"
)

target_link_libraries(TestCamera PRIVATE
  Catch2::Catch2WithMain
  Entropy::Common
  spdlog::spdlog
  entropy_warnings
)

entropy_enable_coverage_for_target(TestCamera)
entropy_register_coverage_test_target(TestCamera)

target_include_directories(TestCamera PRIVATE
  "${entropy_APP_DIR}"
)

set_target_properties(TestCamera PROPERTIES
  CXX_STANDARD 23
  CXX_STANDARD_REQUIRED ON
  CXX_EXTENSIONS OFF
)

entropy_copy_runtime_dlls(TestCamera)

catch_discover_tests(TestCamera)
