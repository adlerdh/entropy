if(NOT TARGET Catch2::Catch2WithMain)
  find_package(Catch2 3 CONFIG REQUIRED)
endif()
include(Catch)

add_executable(TestDeformation
  FieldDomainTests.cpp
  MapContractTests.cpp
)
target_link_libraries(TestDeformation PRIVATE Catch2::Catch2WithMain Entropy::Deformation)
set_target_properties(TestDeformation PROPERTIES CXX_EXTENSIONS OFF)

# Compile every public header in isolation, without a precompiled or umbrella header.
foreach(header ContractError FieldDomain MapPairDescriptor Types)
  set(source "${CMAKE_CURRENT_BINARY_DIR}/include_${header}.cpp")
  file(GENERATE OUTPUT "${source}" CONTENT "#include \"deformation/${header}.h\"\n")
  target_sources(TestDeformation PRIVATE "${source}")
endforeach()

if(TARGET entropy_warnings)
  target_link_libraries(TestDeformation PRIVATE entropy_warnings)
else()
  target_compile_options(TestDeformation PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang,AppleClang>:-Wall -Wextra -Wpedantic -Werror>
    $<$<CXX_COMPILER_ID:MSVC>:/W4 /WX /permissive->
  )
endif()

if(COMMAND entropy_enable_coverage_for_target)
  entropy_enable_coverage_for_target(TestDeformation)
  entropy_register_coverage_test_target(TestDeformation)
endif()

if(COMMAND entropy_copy_runtime_dlls)
  entropy_copy_runtime_dlls(TestDeformation)
elseif(WIN32)
  # A standalone build can use a shared Catch2 package too.
  file(GENERATE
    OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/copy-runtime-$<CONFIG>.cmake"
    CONTENT "set(runtime_dlls \"$<TARGET_RUNTIME_DLLS:TestDeformation>\")\nforeach(dll IN LISTS runtime_dlls)\n  file(COPY \"\${dll}\" DESTINATION \"$<TARGET_FILE_DIR:TestDeformation>\")\nendforeach()\n"
  )
  add_custom_command(TARGET TestDeformation POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -P "${CMAKE_CURRENT_BINARY_DIR}/copy-runtime-$<CONFIG>.cmake"
    VERBATIM
  )
endif()

catch_discover_tests(TestDeformation PROPERTIES LABELS deformation)
