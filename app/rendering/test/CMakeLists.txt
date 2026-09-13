add_executable(TestRendering
  EmbeddedResourceTests.cpp
  MeshPlaneIntersectionTests.cpp
  MeshTests.cpp
  MeshOpenGlResourceTests.cpp
  VectorDrawingHelpersTests.cpp
)

target_sources(TestRendering PRIVATE
  "${entropy_APP_DIR}/rendering/helpers/VectorDrawingHelpers.cpp"
  "${entropy_APP_DIR}/rendering/mesh/MeshExtractionJobs.cpp"
  "${entropy_APP_DIR}/rendering/mesh/MeshImageAdapter.cpp"
)

target_link_libraries(TestRendering PRIVATE
  Catch2::Catch2WithMain
  Entropy::Common
  Entropy::Image
  Entropy::Mesh
  Entropy::Rendering
  Entropy::RenderingMeshProcessing
  Entropy::Viewer
  entropy::colormaps
  entropy::fonts
  glad
  glfw
  entropy_warnings
  ${VTK_LIBRARIES}
)

entropy_enable_coverage_for_target(TestRendering)
entropy_register_coverage_test_target(TestRendering)

target_include_directories(TestRendering PRIVATE
  "${entropy_APP_DIR}"
  "${glad_INCLUDE_DIR}"
)

set_target_properties(TestRendering PROPERTIES
  CXX_STANDARD 23
  CXX_STANDARD_REQUIRED ON
  CXX_EXTENSIONS OFF
)

entropy_copy_runtime_dlls(TestRendering)

catch_discover_tests(TestRendering)
