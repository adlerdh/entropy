option(Entropy_ENABLE_COVERAGE "Build Entropy targets with code coverage instrumentation" OFF)

set(Entropy_COVERAGE_MODE "AUTO" CACHE STRING
  "Coverage backend to use: AUTO, LLVM, GCOV, or OPENCPPCOVERAGE")
set_property(CACHE Entropy_COVERAGE_MODE PROPERTY STRINGS AUTO LLVM GCOV OPENCPPCOVERAGE)

set(Entropy_COVERAGE_EXCLUDE_REGEX
  "(/external/|/test/|Tests\\.cpp$|/build-[^/]+/|/CMakeFiles/|/usr/|/Applications/Xcode.app/)"
  CACHE STRING "Regular expression for files excluded from coverage reports")

function(entropy_configure_coverage)
  if(NOT Entropy_ENABLE_COVERAGE)
    return()
  endif()

  string(TOUPPER "${Entropy_COVERAGE_MODE}" _entropy_coverage_mode)
  if(_entropy_coverage_mode STREQUAL "AUTO")
    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
      set(_entropy_coverage_mode "LLVM")
    elseif(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
      set(_entropy_coverage_mode "GCOV")
    elseif(MSVC)
      set(_entropy_coverage_mode "OPENCPPCOVERAGE")
    else()
      message(FATAL_ERROR
        "Entropy_ENABLE_COVERAGE is ON, but no coverage backend is known for "
        "${CMAKE_CXX_COMPILER_ID}. Set Entropy_COVERAGE_MODE explicitly.")
    endif()
  endif()

  set_property(GLOBAL PROPERTY ENTROPY_COVERAGE_MODE "${_entropy_coverage_mode}")

  add_library(entropy_coverage_flags INTERFACE)

  if(_entropy_coverage_mode STREQUAL "LLVM")
    find_program(ENTROPY_LLVM_COV llvm-cov)
    find_program(ENTROPY_LLVM_PROFDATA llvm-profdata)

    if((NOT ENTROPY_LLVM_COV OR NOT ENTROPY_LLVM_PROFDATA) AND APPLE)
      execute_process(
        COMMAND xcrun --find llvm-cov
        OUTPUT_VARIABLE _entropy_xcrun_llvm_cov
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET)
      execute_process(
        COMMAND xcrun --find llvm-profdata
        OUTPUT_VARIABLE _entropy_xcrun_llvm_profdata
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET)
      if(_entropy_xcrun_llvm_cov)
        set(ENTROPY_LLVM_COV "${_entropy_xcrun_llvm_cov}" CACHE FILEPATH "llvm-cov executable" FORCE)
      endif()
      if(_entropy_xcrun_llvm_profdata)
        set(ENTROPY_LLVM_PROFDATA "${_entropy_xcrun_llvm_profdata}" CACHE FILEPATH "llvm-profdata executable" FORCE)
      endif()
    endif()

    if(NOT ENTROPY_LLVM_COV OR NOT ENTROPY_LLVM_PROFDATA)
      message(FATAL_ERROR
        "LLVM coverage requires llvm-cov and llvm-profdata. Install LLVM/Xcode "
        "tools or select another Entropy_COVERAGE_MODE.")
    endif()

    target_compile_options(entropy_coverage_flags INTERFACE
      $<$<COMPILE_LANGUAGE:C,CXX,OBJCXX>:-O0 -g -fprofile-instr-generate -fcoverage-mapping>)
    target_link_options(entropy_coverage_flags INTERFACE
      -fprofile-instr-generate -fcoverage-mapping)

    set_property(GLOBAL PROPERTY ENTROPY_COVERAGE_LLVM_COV "${ENTROPY_LLVM_COV}")
    set_property(GLOBAL PROPERTY ENTROPY_COVERAGE_LLVM_PROFDATA "${ENTROPY_LLVM_PROFDATA}")
  elseif(_entropy_coverage_mode STREQUAL "GCOV")
    if(NOT CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
      message(FATAL_ERROR "GCOV coverage mode requires GCC or Clang.")
    endif()

    find_program(ENTROPY_GCOVR gcovr)
    find_program(ENTROPY_LCOV lcov)
    find_program(ENTROPY_GENHTML genhtml)
    set(_entropy_gcov_names gcov)
    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
      string(REGEX MATCH "^[0-9]+" _entropy_gcc_major "${CMAKE_CXX_COMPILER_VERSION}")
      if(_entropy_gcc_major)
        list(PREPEND _entropy_gcov_names "gcov-${_entropy_gcc_major}")
      endif()
    endif()
    find_program(ENTROPY_GCOV_EXECUTABLE NAMES ${_entropy_gcov_names})

    if(NOT ENTROPY_GCOVR AND (NOT ENTROPY_LCOV OR NOT ENTROPY_GENHTML))
      message(FATAL_ERROR
        "GCOV coverage requires gcovr or both lcov and genhtml.")
    endif()
    if(NOT ENTROPY_GCOV_EXECUTABLE)
      message(FATAL_ERROR "GCOV coverage requires a gcov executable.")
    endif()

    target_compile_options(entropy_coverage_flags INTERFACE
      $<$<COMPILE_LANGUAGE:C,CXX>:-O0 -g --coverage>)
    target_link_options(entropy_coverage_flags INTERFACE --coverage)

    set_property(GLOBAL PROPERTY ENTROPY_COVERAGE_GCOVR "${ENTROPY_GCOVR}")
    set_property(GLOBAL PROPERTY ENTROPY_COVERAGE_GCOV_EXECUTABLE "${ENTROPY_GCOV_EXECUTABLE}")
    set_property(GLOBAL PROPERTY ENTROPY_COVERAGE_LCOV "${ENTROPY_LCOV}")
    set_property(GLOBAL PROPERTY ENTROPY_COVERAGE_GENHTML "${ENTROPY_GENHTML}")
  elseif(_entropy_coverage_mode STREQUAL "OPENCPPCOVERAGE")
    if(NOT MSVC)
      message(FATAL_ERROR "OPENCPPCOVERAGE mode is intended for MSVC builds.")
    endif()

    find_program(ENTROPY_OPENCPPCOVERAGE OpenCppCoverage)
    if(NOT ENTROPY_OPENCPPCOVERAGE)
      message(FATAL_ERROR
        "MSVC coverage requires OpenCppCoverage in PATH. Install it or disable "
        "Entropy_ENABLE_COVERAGE.")
    endif()

    target_compile_options(entropy_coverage_flags INTERFACE
      $<$<COMPILE_LANGUAGE:C,CXX>:/Od /Zi>)
    target_link_options(entropy_coverage_flags INTERFACE /DEBUG)

    set_property(GLOBAL PROPERTY ENTROPY_COVERAGE_OPENCPPCOVERAGE "${ENTROPY_OPENCPPCOVERAGE}")
  else()
    message(FATAL_ERROR
      "Unsupported Entropy_COVERAGE_MODE='${Entropy_COVERAGE_MODE}'.")
  endif()

  message(STATUS "Entropy coverage enabled with ${_entropy_coverage_mode} backend")
endfunction()

function(entropy_enable_coverage_for_target target)
  if(NOT Entropy_ENABLE_COVERAGE)
    return()
  endif()

  if(NOT TARGET "${target}")
    message(FATAL_ERROR "Cannot enable coverage for missing target '${target}'")
  endif()

  target_link_libraries("${target}" PRIVATE entropy_coverage_flags)
endfunction()

function(entropy_register_coverage_test_target target)
  if(NOT Entropy_ENABLE_COVERAGE)
    return()
  endif()

  if(NOT TARGET "${target}")
    message(FATAL_ERROR "Cannot register missing coverage test target '${target}'")
  endif()

  set_property(GLOBAL APPEND PROPERTY ENTROPY_COVERAGE_TEST_TARGETS "${target}")
endfunction()

function(entropy_add_coverage_targets)
  if(NOT Entropy_ENABLE_COVERAGE)
    return()
  endif()

  get_property(_entropy_coverage_test_targets GLOBAL PROPERTY ENTROPY_COVERAGE_TEST_TARGETS)
  if(NOT _entropy_coverage_test_targets)
    message(WARNING "Entropy_ENABLE_COVERAGE is ON, but no coverage test targets were registered.")
    return()
  endif()

  get_property(_entropy_coverage_mode GLOBAL PROPERTY ENTROPY_COVERAGE_MODE)
  get_property(_entropy_llvm_cov GLOBAL PROPERTY ENTROPY_COVERAGE_LLVM_COV)
  get_property(_entropy_llvm_profdata GLOBAL PROPERTY ENTROPY_COVERAGE_LLVM_PROFDATA)
  get_property(_entropy_gcovr GLOBAL PROPERTY ENTROPY_COVERAGE_GCOVR)
  get_property(_entropy_gcov_executable GLOBAL PROPERTY ENTROPY_COVERAGE_GCOV_EXECUTABLE)
  get_property(_entropy_lcov GLOBAL PROPERTY ENTROPY_COVERAGE_LCOV)
  get_property(_entropy_genhtml GLOBAL PROPERTY ENTROPY_COVERAGE_GENHTML)
  get_property(_entropy_opencppcoverage GLOBAL PROPERTY ENTROPY_COVERAGE_OPENCPPCOVERAGE)

  set(_entropy_coverage_objects)
  foreach(_entropy_target IN LISTS _entropy_coverage_test_targets)
    list(APPEND _entropy_coverage_objects "$<TARGET_FILE:${_entropy_target}>")
  endforeach()

  add_custom_target(coverage
    COMMAND "${CMAKE_COMMAND}"
      "-DENTROPY_COVERAGE_MODE=${_entropy_coverage_mode}"
      "-DENTROPY_SOURCE_DIR=${CMAKE_SOURCE_DIR}"
      "-DENTROPY_BINARY_DIR=${CMAKE_BINARY_DIR}"
      "-DENTROPY_CTEST_COMMAND=${CMAKE_CTEST_COMMAND}"
      "-DENTROPY_COVERAGE_DIR=${CMAKE_BINARY_DIR}/coverage"
      "-DENTROPY_COVERAGE_EXCLUDE_REGEX=${Entropy_COVERAGE_EXCLUDE_REGEX}"
      "-DENTROPY_COVERAGE_OBJECTS=$<JOIN:${_entropy_coverage_objects},|>"
      "-DENTROPY_COVERAGE_HTML=OFF"
      "-DENTROPY_COVERAGE_CONFIG=$<CONFIG>"
      "-DENTROPY_LLVM_COV=${_entropy_llvm_cov}"
      "-DENTROPY_LLVM_PROFDATA=${_entropy_llvm_profdata}"
      "-DENTROPY_GCOVR=${_entropy_gcovr}"
      "-DENTROPY_GCOV_EXECUTABLE=${_entropy_gcov_executable}"
      "-DENTROPY_LCOV=${_entropy_lcov}"
      "-DENTROPY_GENHTML=${_entropy_genhtml}"
      "-DENTROPY_OPENCPPCOVERAGE=${_entropy_opencppcoverage}"
      -P "${CMAKE_SOURCE_DIR}/cmake/CoverageReport.cmake"
    DEPENDS ${_entropy_coverage_test_targets}
    COMMAND_EXPAND_LISTS
    VERBATIM
    USES_TERMINAL
    COMMENT "Running unit tests and generating Entropy code coverage summary")

  add_custom_target(coverage-html
    COMMAND "${CMAKE_COMMAND}"
      "-DENTROPY_COVERAGE_MODE=${_entropy_coverage_mode}"
      "-DENTROPY_SOURCE_DIR=${CMAKE_SOURCE_DIR}"
      "-DENTROPY_BINARY_DIR=${CMAKE_BINARY_DIR}"
      "-DENTROPY_CTEST_COMMAND=${CMAKE_CTEST_COMMAND}"
      "-DENTROPY_COVERAGE_DIR=${CMAKE_BINARY_DIR}/coverage"
      "-DENTROPY_COVERAGE_EXCLUDE_REGEX=${Entropy_COVERAGE_EXCLUDE_REGEX}"
      "-DENTROPY_COVERAGE_OBJECTS=$<JOIN:${_entropy_coverage_objects},|>"
      "-DENTROPY_COVERAGE_HTML=ON"
      "-DENTROPY_COVERAGE_CONFIG=$<CONFIG>"
      "-DENTROPY_LLVM_COV=${_entropy_llvm_cov}"
      "-DENTROPY_LLVM_PROFDATA=${_entropy_llvm_profdata}"
      "-DENTROPY_GCOVR=${_entropy_gcovr}"
      "-DENTROPY_GCOV_EXECUTABLE=${_entropy_gcov_executable}"
      "-DENTROPY_LCOV=${_entropy_lcov}"
      "-DENTROPY_GENHTML=${_entropy_genhtml}"
      "-DENTROPY_OPENCPPCOVERAGE=${_entropy_opencppcoverage}"
      -P "${CMAKE_SOURCE_DIR}/cmake/CoverageReport.cmake"
    DEPENDS ${_entropy_coverage_test_targets}
    COMMAND_EXPAND_LISTS
    VERBATIM
    USES_TERMINAL
    COMMENT "Running unit tests and generating Entropy HTML code coverage report")

  add_custom_target(coverage-clean
    COMMAND "${CMAKE_COMMAND}" -E rm -rf "${CMAKE_BINARY_DIR}/coverage"
    COMMENT "Removing Entropy coverage output")
endfunction()
