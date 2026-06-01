add_executable(EntropyCommonTests
  MathFuncsTests.cpp
)

target_link_libraries(EntropyCommonTests PRIVATE
  Catch2::Catch2WithMain
  Entropy::Common
  entropy_warnings
)

set_target_properties(EntropyCommonTests PROPERTIES
  CXX_STANDARD 23
  CXX_STANDARD_REQUIRED ON
  CXX_EXTENSIONS OFF
)

entropy_copy_runtime_dlls(EntropyCommonTests)

catch_discover_tests(EntropyCommonTests)
