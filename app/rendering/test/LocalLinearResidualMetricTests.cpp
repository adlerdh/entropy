#include "rendering/metrics/LocalLinearResidualMetric.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <array>

namespace local_linear_residual = entropy::rendering::local_linear_residual;

TEST_CASE("local linear residual fits gain and bias", "[rendering][local-linear-residual]")
{
  constexpr std::array fixed{0.0f, 0.25f, 0.5f, 0.75f, 1.0f};
  constexpr std::array moving{0.1f, 0.6f, 1.1f, 1.6f, 2.1f};

  const auto fit = local_linear_residual::fit(fixed, moving, 1.0e-5f);

  REQUIRE(fit.valid);
  CHECK(fit.gain == Catch::Approx(2.0f));
  CHECK(fit.bias == Catch::Approx(0.1f));
  CHECK(fit.residual == Catch::Approx(0.0f).margin(1.0e-6f));
}

TEST_CASE("local linear residual reports nonlinear mismatch", "[rendering][local-linear-residual]")
{
  constexpr std::array fixed{0.0f, 0.25f, 0.5f, 0.75f, 1.0f};
  constexpr std::array moving{0.0f, 0.0625f, 0.25f, 0.5625f, 1.0f};

  const auto fit = local_linear_residual::fit(fixed, moving, 1.0e-5f);

  REQUIRE(fit.valid);
  CHECK(fit.residual > 0.0f);
}

TEST_CASE("local linear residual rejects constant reference patches", "[rendering][local-linear-residual]")
{
  constexpr std::array fixed{0.5f, 0.5f, 0.5f};
  constexpr std::array moving{0.1f, 0.2f, 0.3f};

  CHECK_FALSE(local_linear_residual::fit(fixed, moving, 1.0e-5f).valid);
}
