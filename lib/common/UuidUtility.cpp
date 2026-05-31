#include "common/UuidUtility.h"

#include <array>
#include <random>

uuids::uuid generateRandomUuid()
{
  std::random_device rd;
  auto seed_data = std::array<int, std::mt19937::state_size>{};
  std::generate(std::begin(seed_data), std::end(seed_data), std::ref(rd));
  std::seed_seq seq(std::begin(seed_data), std::end(seed_data));
  std::mt19937 generator(seq);
  return uuids::uuid_random_generator{generator}();
}
