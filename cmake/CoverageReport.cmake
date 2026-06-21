if(NOT DEFINED ENTROPY_COVERAGE_MODE)
  message(FATAL_ERROR "ENTROPY_COVERAGE_MODE is required")
endif()

if(NOT DEFINED ENTROPY_SOURCE_DIR OR NOT DEFINED ENTROPY_BINARY_DIR OR NOT DEFINED ENTROPY_CTEST_COMMAND)
  message(FATAL_ERROR "ENTROPY_SOURCE_DIR, ENTROPY_BINARY_DIR, and ENTROPY_CTEST_COMMAND are required")
endif()

if(NOT DEFINED ENTROPY_COVERAGE_DIR)
  set(ENTROPY_COVERAGE_DIR "${ENTROPY_BINARY_DIR}/coverage")
endif()

if(NOT DEFINED ENTROPY_COVERAGE_HTML)
  set(ENTROPY_COVERAGE_HTML OFF)
endif()

set(_entropy_ctest_command "${ENTROPY_CTEST_COMMAND}" "--output-on-failure" "--test-dir" "${ENTROPY_BINARY_DIR}")
if(DEFINED ENTROPY_COVERAGE_CONFIG AND NOT ENTROPY_COVERAGE_CONFIG STREQUAL "")
  list(APPEND _entropy_ctest_command "-C" "${ENTROPY_COVERAGE_CONFIG}")
endif()

file(REMOVE_RECURSE "${ENTROPY_COVERAGE_DIR}")
file(MAKE_DIRECTORY "${ENTROPY_COVERAGE_DIR}")

if(ENTROPY_COVERAGE_MODE STREQUAL "LLVM")
  if(NOT EXISTS "${ENTROPY_LLVM_COV}" OR NOT EXISTS "${ENTROPY_LLVM_PROFDATA}")
    message(FATAL_ERROR "LLVM coverage requires valid ENTROPY_LLVM_COV and ENTROPY_LLVM_PROFDATA")
  endif()

  set(_entropy_profile_dir "${ENTROPY_COVERAGE_DIR}/profiles")
  file(MAKE_DIRECTORY "${_entropy_profile_dir}")

  execute_process(
    COMMAND "${CMAKE_COMMAND}" -E env
      "LLVM_PROFILE_FILE=${_entropy_profile_dir}/%p-%m.profraw"
      ${_entropy_ctest_command}
    WORKING_DIRECTORY "${ENTROPY_BINARY_DIR}"
    RESULT_VARIABLE _entropy_test_result)
  if(NOT _entropy_test_result EQUAL 0)
    message(FATAL_ERROR "Coverage test run failed with exit code ${_entropy_test_result}")
  endif()

  file(GLOB _entropy_raw_profiles "${_entropy_profile_dir}/*.profraw")
  if(NOT _entropy_raw_profiles)
    message(FATAL_ERROR "No LLVM .profraw files were produced by the test run")
  endif()

  set(_entropy_indexed_profile "${ENTROPY_COVERAGE_DIR}/entropy.profdata")
  execute_process(
    COMMAND "${ENTROPY_LLVM_PROFDATA}" merge -sparse ${_entropy_raw_profiles}
      -o "${_entropy_indexed_profile}"
    RESULT_VARIABLE _entropy_merge_result)
  if(NOT _entropy_merge_result EQUAL 0)
    message(FATAL_ERROR "llvm-profdata merge failed with exit code ${_entropy_merge_result}")
  endif()

  if(NOT DEFINED ENTROPY_COVERAGE_OBJECTS OR ENTROPY_COVERAGE_OBJECTS STREQUAL "")
    message(FATAL_ERROR "ENTROPY_COVERAGE_OBJECTS is required for LLVM coverage")
  endif()

  string(REPLACE "|" ";" _entropy_coverage_objects "${ENTROPY_COVERAGE_OBJECTS}")
  list(POP_FRONT _entropy_coverage_objects _entropy_coverage_executable)
  set(_entropy_object_args)
  foreach(_entropy_object IN LISTS _entropy_coverage_objects)
    list(APPEND _entropy_object_args "-object=${_entropy_object}")
  endforeach()

  if(ENTROPY_COVERAGE_HTML)
    set(_entropy_html_dir "${ENTROPY_COVERAGE_DIR}/html")
    execute_process(
      COMMAND "${ENTROPY_LLVM_COV}" show
        "${_entropy_coverage_executable}"
        ${_entropy_object_args}
        "-instr-profile=${_entropy_indexed_profile}"
        "-ignore-filename-regex=${ENTROPY_COVERAGE_EXCLUDE_REGEX}"
        "-format=html"
        "-output-dir=${_entropy_html_dir}"
      WORKING_DIRECTORY "${ENTROPY_SOURCE_DIR}"
      RESULT_VARIABLE _entropy_show_result)
    if(NOT _entropy_show_result EQUAL 0)
      message(FATAL_ERROR "llvm-cov show failed with exit code ${_entropy_show_result}")
    endif()
    message(STATUS "Entropy HTML coverage report: ${_entropy_html_dir}/index.html")
  endif()

  execute_process(
    COMMAND "${ENTROPY_LLVM_COV}" report
      "${_entropy_coverage_executable}"
      ${_entropy_object_args}
      "-instr-profile=${_entropy_indexed_profile}"
      "-ignore-filename-regex=${ENTROPY_COVERAGE_EXCLUDE_REGEX}"
    WORKING_DIRECTORY "${ENTROPY_SOURCE_DIR}"
    RESULT_VARIABLE _entropy_report_result)
  if(NOT _entropy_report_result EQUAL 0)
    message(FATAL_ERROR "llvm-cov report failed with exit code ${_entropy_report_result}")
  endif()
elseif(ENTROPY_COVERAGE_MODE STREQUAL "GCOV")
  execute_process(
    COMMAND ${_entropy_ctest_command}
    WORKING_DIRECTORY "${ENTROPY_BINARY_DIR}"
    RESULT_VARIABLE _entropy_test_result)
  if(NOT _entropy_test_result EQUAL 0)
    message(FATAL_ERROR "Coverage test run failed with exit code ${_entropy_test_result}")
  endif()

  if(DEFINED ENTROPY_GCOVR AND NOT ENTROPY_GCOVR STREQUAL "" AND EXISTS "${ENTROPY_GCOVR}")
    set(_entropy_gcovr_command
      "${ENTROPY_GCOVR}"
      "--root" "${ENTROPY_SOURCE_DIR}"
      "--object-directory" "${ENTROPY_BINARY_DIR}"
      "--exclude" "${ENTROPY_COVERAGE_EXCLUDE_REGEX}"
      "--gcov-executable" "${ENTROPY_GCOV_EXECUTABLE}"
      "--gcov-ignore-parse-errors" "negative_hits.warn_once_per_file"
      "--print-summary")
    if(ENTROPY_COVERAGE_HTML)
      file(MAKE_DIRECTORY "${ENTROPY_COVERAGE_DIR}/html")
      list(APPEND _entropy_gcovr_command
        "--html-details" "${ENTROPY_COVERAGE_DIR}/html/index.html")
    endif()
    execute_process(
      COMMAND ${_entropy_gcovr_command}
      WORKING_DIRECTORY "${ENTROPY_SOURCE_DIR}"
      RESULT_VARIABLE _entropy_gcovr_result)
    if(NOT _entropy_gcovr_result EQUAL 0)
      message(FATAL_ERROR "gcovr failed with exit code ${_entropy_gcovr_result}")
    endif()
    if(ENTROPY_COVERAGE_HTML)
      message(STATUS "Entropy HTML coverage report: ${ENTROPY_COVERAGE_DIR}/html/index.html")
    endif()
  elseif(DEFINED ENTROPY_LCOV AND DEFINED ENTROPY_GENHTML
      AND EXISTS "${ENTROPY_LCOV}" AND EXISTS "${ENTROPY_GENHTML}")
    set(_entropy_raw_info "${ENTROPY_COVERAGE_DIR}/entropy.raw.info")
    set(_entropy_filtered_info "${ENTROPY_COVERAGE_DIR}/entropy.info")
    execute_process(
      COMMAND "${ENTROPY_LCOV}" --capture --directory "${ENTROPY_BINARY_DIR}"
        --gcov-tool "${ENTROPY_GCOV_EXECUTABLE}"
        --output-file "${_entropy_raw_info}"
      RESULT_VARIABLE _entropy_lcov_capture_result)
    if(NOT _entropy_lcov_capture_result EQUAL 0)
      message(FATAL_ERROR "lcov capture failed with exit code ${_entropy_lcov_capture_result}")
    endif()
    execute_process(
      COMMAND "${ENTROPY_LCOV}" --remove "${_entropy_raw_info}"
        "*/external/*" "*/build-*/*" "/usr/*"
        --output-file "${_entropy_filtered_info}"
      RESULT_VARIABLE _entropy_lcov_filter_result)
    if(NOT _entropy_lcov_filter_result EQUAL 0)
      message(FATAL_ERROR "lcov filtering failed with exit code ${_entropy_lcov_filter_result}")
    endif()
    execute_process(
      COMMAND "${ENTROPY_LCOV}" --summary "${_entropy_filtered_info}"
      RESULT_VARIABLE _entropy_lcov_summary_result)
    if(NOT _entropy_lcov_summary_result EQUAL 0)
      message(FATAL_ERROR "lcov summary failed with exit code ${_entropy_lcov_summary_result}")
    endif()
    if(ENTROPY_COVERAGE_HTML)
      execute_process(
        COMMAND "${ENTROPY_GENHTML}" "${_entropy_filtered_info}"
          --output-directory "${ENTROPY_COVERAGE_DIR}/html"
        RESULT_VARIABLE _entropy_genhtml_result)
      if(NOT _entropy_genhtml_result EQUAL 0)
        message(FATAL_ERROR "genhtml failed with exit code ${_entropy_genhtml_result}")
      endif()
      message(STATUS "Entropy HTML coverage report: ${ENTROPY_COVERAGE_DIR}/html/index.html")
    endif()
  else()
    message(FATAL_ERROR "GCOV coverage requires gcovr or lcov/genhtml")
  endif()
elseif(ENTROPY_COVERAGE_MODE STREQUAL "OPENCPPCOVERAGE")
  if(NOT EXISTS "${ENTROPY_OPENCPPCOVERAGE}")
    message(FATAL_ERROR "OpenCppCoverage executable was not found")
  endif()

  set(_entropy_export "${ENTROPY_COVERAGE_DIR}/opencppcoverage.xml")
  if(ENTROPY_COVERAGE_HTML)
    set(_entropy_export "html:${ENTROPY_COVERAGE_DIR}/html")
  else()
    set(_entropy_export "cobertura:${_entropy_export}")
  endif()

  function(_entropy_append_opencppcoverage_path out_var path)
    file(TO_CMAKE_PATH "${path}" _entropy_coverage_path)
    file(TO_NATIVE_PATH "${_entropy_coverage_path}" _entropy_coverage_native_path)
    list(APPEND ${out_var} "${_entropy_coverage_native_path}")

    if(DEFINED ENV{GITHUB_WORKSPACE} AND NOT "$ENV{GITHUB_WORKSPACE}" STREQUAL "")
      file(TO_CMAKE_PATH "$ENV{GITHUB_WORKSPACE}" _entropy_github_workspace)
      file(TO_CMAKE_PATH "${ENTROPY_SOURCE_DIR}" _entropy_source_dir_cmake)
      foreach(_entropy_base IN ITEMS "${ENTROPY_SOURCE_DIR}" "${ENTROPY_BINARY_DIR}")
        file(TO_CMAKE_PATH "${_entropy_base}" _entropy_base_cmake)
        cmake_path(IS_PREFIX _entropy_base_cmake "${_entropy_coverage_path}" NORMALIZE _entropy_is_prefix)
        if(_entropy_is_prefix)
          file(RELATIVE_PATH _entropy_relative_path "${_entropy_base_cmake}" "${_entropy_coverage_path}")
          if(_entropy_base_cmake STREQUAL _entropy_source_dir_cmake)
            set(_entropy_workspace_path "${_entropy_github_workspace}/${_entropy_relative_path}")
          else()
            get_filename_component(_entropy_binary_name "${ENTROPY_BINARY_DIR}" NAME)
            set(_entropy_workspace_path "${_entropy_github_workspace}/${_entropy_binary_name}/${_entropy_relative_path}")
          endif()
          file(TO_NATIVE_PATH "${_entropy_workspace_path}" _entropy_workspace_native_path)
          list(FIND ${out_var} "${_entropy_workspace_native_path}" _entropy_workspace_path_index)
          if(_entropy_workspace_path_index EQUAL -1)
            list(APPEND ${out_var} "${_entropy_workspace_native_path}")
          endif()
        endif()
      endforeach()
    endif()

    set(${out_var} "${${out_var}}" PARENT_SCOPE)
  endfunction()

  set(_entropy_opencppcoverage_source_args)
  set(_entropy_opencppcoverage_app_sources)
  set(_entropy_opencppcoverage_lib_sources)
  _entropy_append_opencppcoverage_path(_entropy_opencppcoverage_app_sources "${ENTROPY_SOURCE_DIR}/app")
  _entropy_append_opencppcoverage_path(_entropy_opencppcoverage_lib_sources "${ENTROPY_SOURCE_DIR}/lib")
  foreach(_entropy_source IN LISTS _entropy_opencppcoverage_app_sources _entropy_opencppcoverage_lib_sources)
    list(APPEND _entropy_opencppcoverage_source_args "--sources" "${_entropy_source}")
  endforeach()

  set(_entropy_opencppcoverage_excluded_source_args)
  set(_entropy_opencppcoverage_external_sources)
  _entropy_append_opencppcoverage_path(_entropy_opencppcoverage_external_sources "${ENTROPY_SOURCE_DIR}/external")
  foreach(_entropy_source IN LISTS _entropy_opencppcoverage_external_sources)
    list(APPEND _entropy_opencppcoverage_excluded_source_args "--excluded_sources" "${_entropy_source}")
  endforeach()

  file(TO_NATIVE_PATH "${_entropy_export}" _entropy_opencppcoverage_export)

  if(NOT DEFINED ENTROPY_COVERAGE_OBJECTS OR ENTROPY_COVERAGE_OBJECTS STREQUAL "")
    message(FATAL_ERROR "ENTROPY_COVERAGE_OBJECTS is required for OpenCppCoverage coverage")
  endif()
  string(REPLACE "|" ";" _entropy_coverage_objects "${ENTROPY_COVERAGE_OBJECTS}")
  set(_entropy_opencppcoverage_module_args)
  foreach(_entropy_object IN LISTS _entropy_coverage_objects)
    set(_entropy_opencppcoverage_modules)
    _entropy_append_opencppcoverage_path(_entropy_opencppcoverage_modules "${_entropy_object}")
    foreach(_entropy_opencppcoverage_module IN LISTS _entropy_opencppcoverage_modules)
      list(APPEND _entropy_opencppcoverage_module_args "--modules" "${_entropy_opencppcoverage_module}")
    endforeach()
  endforeach()

  execute_process(
    COMMAND "${ENTROPY_OPENCPPCOVERAGE}"
      "--cover_children"
      ${_entropy_opencppcoverage_module_args}
      ${_entropy_opencppcoverage_source_args}
      ${_entropy_opencppcoverage_excluded_source_args}
      "--export_type" "${_entropy_opencppcoverage_export}"
      "--"
      ${_entropy_ctest_command}
    WORKING_DIRECTORY "${ENTROPY_BINARY_DIR}"
    RESULT_VARIABLE _entropy_opencppcoverage_result)
  if(NOT _entropy_opencppcoverage_result EQUAL 0)
    message(FATAL_ERROR "OpenCppCoverage failed with exit code ${_entropy_opencppcoverage_result}")
  endif()
  if(ENTROPY_COVERAGE_HTML)
    message(STATUS "Entropy HTML coverage report: ${ENTROPY_COVERAGE_DIR}/html/index.html")
  else()
    message(STATUS "Entropy Cobertura coverage report: ${ENTROPY_COVERAGE_DIR}/opencppcoverage.xml")
  endif()
else()
  message(FATAL_ERROR "Unknown coverage mode '${ENTROPY_COVERAGE_MODE}'")
endif()
