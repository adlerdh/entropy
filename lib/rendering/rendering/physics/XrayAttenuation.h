#pragma once

namespace xray
{
struct LinearAttenuationCoefficients
{
  float water_cmInv = 0.0f;
  float air_cmInv = 0.0f;
};

float defaultEnergyKeV();

/**
 * @brief Compute linear attenuation coefficients for the x-ray projection shader.
 * @param energyKeV Monoenergetic photon energy in keV.
 * @return Linear attenuation coefficients in cm^-1 for liquid water and dry air near sea level.
 *
 * The tabulated mass attenuation coefficients are from NIST X-Ray Mass Attenuation Coefficients:
 * - Water, Liquid: https://physics.nist.gov/PhysRefData/XrayMassCoef/ComTab/water.html
 * - Air, Dry (Near Sea Level): https://physics.nist.gov/PhysRefData/XrayMassCoef/ComTab/air.html
 */
LinearAttenuationCoefficients linearAttenuationCoefficients(float energyKeV);
} // namespace xray
