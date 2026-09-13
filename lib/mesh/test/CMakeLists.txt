add_executable(TestMeshIO
  MeshFormatTests.cpp
  MeshIOTests.cpp
  MeshTransformTests.cpp
)

if(Entropy_ENABLE_IWYU)
  # IWYU 0.21 on Ubuntu 24 uses a Clang 17 frontend that crashes while parsing GCC 13 std::expected:
  set_source_files_properties(
    MeshIOTests.cpp
    MeshTransformTests.cpp
    PROPERTIES SKIP_LINTING ON
  )
endif()

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
