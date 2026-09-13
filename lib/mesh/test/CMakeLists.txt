add_executable(TestMeshIO
  MeshFormatTests.cpp
  MeshIOTests.cpp
  MeshTransformTests.cpp
)

target_link_libraries(TestMeshIO PRIVATE
  Catch2::Catch2WithMain
  Entropy::Mesh
  entropy_warnings
)

set_target_properties(TestMeshIO PROPERTIES
  CXX_STANDARD 23
  CXX_STANDARD_REQUIRED ON
  CXX_EXTENSIONS OFF
)

entropy_enable_coverage_for_target(TestMeshIO)
entropy_register_coverage_test_target(TestMeshIO)
entropy_copy_runtime_dlls(TestMeshIO)
catch_discover_tests(TestMeshIO)
