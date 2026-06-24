#include "logic/app/StackTrace.h"

#include <catch2/catch_test_macros.hpp>

TEST_CASE("Current stack trace returns diagnostic text", "[StackTrace]")
{
  const std::string trace = stack_trace::current();

  CHECK_FALSE(trace.empty());
  CHECK(trace.find("Stack trace") != std::string::npos);
}
