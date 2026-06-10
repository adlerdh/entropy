add_executable(EntropyImageTests
  DicomSeriesTests.cpp
  ImageColorMapTests.cpp
)

target_link_libraries(EntropyImageTests PRIVATE
  Catch2::Catch2WithMain
  Entropy::Image
  entropy_warnings
)

set_target_properties(EntropyImageTests PROPERTIES
  CXX_STANDARD 23
  CXX_STANDARD_REQUIRED ON
  CXX_EXTENSIONS OFF
)

entropy_copy_runtime_dlls(EntropyImageTests)

catch_discover_tests(EntropyImageTests)
