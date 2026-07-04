#include "rendering/XrayAttenuation.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

TEST_CASE("X-ray attenuation defaults use NIST 80 keV coefficients", "[rendering][xray]")
{
  using Catch::Matchers::WithinRel;

  const auto coefficients = xray::linearAttenuationCoefficients(xray::defaultEnergyKeV());

  CHECK(xray::defaultEnergyKeV() == 80.0f);
  CHECK_THAT(coefficients.water_cmInv, WithinRel(1.837E-01f));
  CHECK_THAT(coefficients.air_cmInv, WithinRel(1.662E-01f * 1.225e-3f));
}

TEST_CASE("X-ray attenuation interpolates in energy", "[rendering][xray]")
{
  using Catch::Matchers::WithinRel;

  const auto coefficients = xray::linearAttenuationCoefficients(90.0f);

  CHECK_THAT(coefficients.water_cmInv, WithinRel(0.5f * (1.837E-01f + 1.707E-01f)));
  CHECK_THAT(coefficients.air_cmInv, WithinRel(0.5f * (1.662E-01f + 1.541E-01f) * 1.225e-3f));
}

TEST_CASE("X-ray attenuation rejects energies outside the NIST table range", "[rendering][xray]")
{
  CHECK(xray::linearAttenuationCoefficients(0.5f).water_cmInv == 0.0f);
  CHECK(xray::linearAttenuationCoefficients(21000.0f).water_cmInv == 0.0f);
}
