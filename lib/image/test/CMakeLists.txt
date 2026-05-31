add_executable(EntropyImageTests
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

catch_discover_tests(EntropyImageTests)
