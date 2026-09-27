#include "logic/serialization/LandmarkCsv.h"
#include "logic/annotation/PointRecord.h"

#include <catch2/catch_test_macros.hpp>
#include <glm/glm.hpp>

#include <algorithm>
#include <cstddef>
#include <map>
#include <sstream>

namespace
{
class FaultSink : public std::streambuf
{
public:
  explicit FaultSink(std::streamsize limit, bool flushFailure = false) : remaining(limit), failFlush(flushFailure) {}

private:
  std::streamsize xsputn(const char*, std::streamsize count) override
  {
    const auto accepted = std::min(remaining, count);
    remaining -= accepted;
    return accepted;
  }
  int_type overflow(int_type character) override
  {
    if (traits_type::eq_int_type(character, traits_type::eof())) return traits_type::not_eof(character);
    if (remaining == 0) return traits_type::eof();
    --remaining;
    return character;
  }
  int sync() override
  {
    return failFlush ? -1 : 0;
  }
  std::streamsize remaining;
  bool failFlush;
};
} // namespace

TEST_CASE("Landmark CSV stream boundary rejects short writes and failed flushes", "[workflow][landmarks][filesystem]")
{
  std::map<std::size_t, PointRecord<glm::vec3>> points;
  points.emplace(7, PointRecord<glm::vec3>{{1, 2, 3}});
  points.emplace(99, PointRecord<glm::vec3>{{-4, 5, 6}});
  SECTION("mid-header write")
  {
    FaultSink sink{5};
    std::ostream stream{&sink};
    CHECK_FALSE(serialize::writeLandmarkCsv(stream, points));
  }
  SECTION("mid-point write")
  {
    FaultSink sink{20};
    std::ostream stream{&sink};
    CHECK_FALSE(serialize::writeLandmarkCsv(stream, points));
  }
  SECTION("flush")
  {
    FaultSink sink{10000, true};
    std::ostream stream{&sink};
    CHECK_FALSE(serialize::writeLandmarkCsv(stream, points));
  }
  SECTION("throwing stream used by the atomic publisher")
  {
    FaultSink sink{20};
    std::ostream stream{&sink};
    stream.exceptions(std::ios::badbit | std::ios::failbit);
    CHECK_THROWS_AS(serialize::writeLandmarkCsv(stream, points), std::ios_base::failure);
  }
}
