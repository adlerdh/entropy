add_executable(TestWorkflows CrossFeatureTests.cpp)
target_link_libraries(TestWorkflows PRIVATE EntropyAppRuntime Catch2::Catch2WithMain)
target_include_directories(TestWorkflows PRIVATE "${entropy_EXT_DIR}" "${imgui_SRC_DIR}" "${imgui_SRC_DIR}/imgui")
set_target_properties(TestWorkflows PROPERTIES CXX_STANDARD 23 CXX_STANDARD_REQUIRED ON CXX_EXTENSIONS OFF)
entropy_enable_coverage_for_target(TestWorkflows)
entropy_register_coverage_test_target(TestWorkflows)
entropy_copy_runtime_dlls(TestWorkflows)
catch_discover_tests(TestWorkflows PROPERTIES LABELS "workflow" TIMEOUT 60)

# The Stage 4 transaction tests run without a display or AppData instance.
add_executable(TestDeformationSession
  DeformationEditSessionTests.cpp
  "${CMAKE_SOURCE_DIR}/app/logic/app/DeformationEditController.cpp"
  "${CMAKE_SOURCE_DIR}/app/logic/app/DeformationEditSession.cpp")
target_link_libraries(TestDeformationSession PRIVATE Catch2::Catch2WithMain Entropy::Deformation entropy_warnings)
target_include_directories(TestDeformationSession PRIVATE "${CMAKE_SOURCE_DIR}/app")
set_target_properties(TestDeformationSession PROPERTIES CXX_STANDARD 23 CXX_STANDARD_REQUIRED ON CXX_EXTENSIONS OFF)
entropy_enable_coverage_for_target(TestDeformationSession)
entropy_register_coverage_test_target(TestDeformationSession)
entropy_copy_runtime_dlls(TestDeformationSession)
catch_discover_tests(TestDeformationSession PROPERTIES LABELS "deformation-session")

add_executable(TestDeformationArchive
  DeformationArchiveTests.cpp
  "${CMAKE_SOURCE_DIR}/app/logic/app/DeformationArchive.cpp")
target_link_libraries(TestDeformationArchive PRIVATE
  Catch2::Catch2WithMain Entropy::Common Entropy::Deformation nlohmann_json::nlohmann_json entropy_warnings)
target_include_directories(TestDeformationArchive PRIVATE "${CMAKE_SOURCE_DIR}/app")
set_target_properties(TestDeformationArchive PROPERTIES CXX_STANDARD 23 CXX_STANDARD_REQUIRED ON CXX_EXTENSIONS OFF)
entropy_enable_coverage_for_target(TestDeformationArchive)
entropy_register_coverage_test_target(TestDeformationArchive)
entropy_copy_runtime_dlls(TestDeformationArchive)
catch_discover_tests(TestDeformationArchive PROPERTIES LABELS "deformation-archive")

if(Entropy_ENABLE_DEFORMATION_GPU_TESTS)
  add_executable(TestDeformationEditGpu
    DeformationEditGpuTests.cpp
    "${CMAKE_SOURCE_DIR}/app/logic/app/DeformationEditBackend.cpp"
    "${CMAKE_SOURCE_DIR}/app/logic/app/DeformationEditController.cpp"
    "${CMAKE_SOURCE_DIR}/app/logic/app/DeformationEditSession.cpp")
  target_link_libraries(TestDeformationEditGpu PRIVATE
    Catch2::Catch2WithMain Entropy::Rendering Entropy::Deformation glfw entropy_warnings)
  target_include_directories(TestDeformationEditGpu PRIVATE "${CMAKE_SOURCE_DIR}/app")
  set_target_properties(TestDeformationEditGpu PROPERTIES CXX_STANDARD 23 CXX_STANDARD_REQUIRED ON CXX_EXTENSIONS OFF)
  entropy_enable_coverage_for_target(TestDeformationEditGpu)
  entropy_register_coverage_test_target(TestDeformationEditGpu)
  entropy_copy_runtime_dlls(TestDeformationEditGpu)
  catch_discover_tests(TestDeformationEditGpu PROPERTIES LABELS "deformation-edit-gpu")
endif()

find_package(Python3 QUIET COMPONENTS Interpreter)
if(Python3_Interpreter_FOUND)
  add_test(NAME required-graphics-gate-contract
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/test/test_required_graphics.py")
endif()

option(Entropy_BUILD_DISPLAY_TESTS "Build interactive tests requiring physical mixed-DPI monitors" OFF)

if(Entropy_BUILD_DISPLAY_TESTS)
  add_executable(TestDisplayTransitions
    "${CMAKE_SOURCE_DIR}/test/platform/DisplayTransitions.cpp"
    "${entropy_UI_DIR}/UiScaleManager.cpp")
  
  target_link_libraries(TestDisplayTransitions PRIVATE Catch2::Catch2WithMain glfw imgui Entropy::Common)
  target_include_directories(TestDisplayTransitions PRIVATE "${entropy_UI_DIR}/.." "${imgui_SRC_DIR}" "${imgui_SRC_DIR}/imgui")
  set_target_properties(TestDisplayTransitions PROPERTIES CXX_STANDARD 23 CXX_STANDARD_REQUIRED ON CXX_EXTENSIONS OFF)

  entropy_copy_runtime_dlls(TestDisplayTransitions)
  catch_discover_tests(TestDisplayTransitions PROPERTIES LABELS "platform;hardware" TIMEOUT 120)
endif()
