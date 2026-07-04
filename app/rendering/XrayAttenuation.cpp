#include "rendering/XrayAttenuation.h"

#include "common/MathFuncs.h"

#include <map>

namespace
{
// Densities in g/cm^3. The air density is the ISA sea-level value used to convert the
// NIST dry-air mass attenuation coefficient to a linear attenuation coefficient.
constexpr float k_airDensity_gPerCm3 = 1.225e-3f;
constexpr float k_waterDensity_gPerCm3 = 1.0f;
constexpr float k_defaultEnergyKeV = 80.0f;

// Total photon mass attenuation coefficients (mu/rho) in cm^2/g, tabulated by photon energy in MeV.
// Source: NIST X-Ray Mass Attenuation Coefficients, Water, Liquid.
// https://physics.nist.gov/PhysRefData/XrayMassCoef/ComTab/water.html
const std::map<float, float> k_waterMassAttenCoeffs_cm2PerG{
  {1.00000E-03f, 4.078E+03f}, {1.50000E-03f, 1.376E+03f}, {2.00000E-03f, 6.173E+02f}, {3.00000E-03f, 1.929E+02f},
  {4.00000E-03f, 8.278E+01f}, {5.00000E-03f, 4.258E+01f}, {6.00000E-03f, 2.464E+01f}, {8.00000E-03f, 1.037E+01f},
  {1.00000E-02f, 5.329E+00f}, {1.50000E-02f, 1.673E+00f}, {2.00000E-02f, 8.096E-01f}, {3.00000E-02f, 3.756E-01f},
  {4.00000E-02f, 2.683E-01f}, {5.00000E-02f, 2.269E-01f}, {6.00000E-02f, 2.059E-01f}, {8.00000E-02f, 1.837E-01f},
  {1.00000E-01f, 1.707E-01f}, {1.50000E-01f, 1.505E-01f}, {2.00000E-01f, 1.370E-01f}, {3.00000E-01f, 1.186E-01f},
  {4.00000E-01f, 1.061E-01f}, {5.00000E-01f, 9.687E-02f}, {6.00000E-01f, 8.956E-02f}, {8.00000E-01f, 7.865E-02f},
  {1.00000E+00f, 7.072E-02f}, {1.25000E+00f, 6.323E-02f}, {1.50000E+00f, 5.754E-02f}, {2.00000E+00f, 4.942E-02f},
  {3.00000E+00f, 3.969E-02f}, {4.00000E+00f, 3.403E-02f}, {5.00000E+00f, 3.031E-02f}, {6.00000E+00f, 2.770E-02f},
  {8.00000E+00f, 2.429E-02f}, {1.00000E+01f, 2.219E-02f}, {1.50000E+01f, 1.941E-02f}, {2.00000E+01f, 1.813E-02f}};

// Total photon mass attenuation coefficients (mu/rho) in cm^2/g, tabulated by photon energy in MeV.
// Source: NIST X-Ray Mass Attenuation Coefficients, Air, Dry (Near Sea Level).
// https://physics.nist.gov/PhysRefData/XrayMassCoef/ComTab/air.html
const std::map<float, float> k_airMassAttenCoeffs_cm2PerG{
  {1.00000E-03f, 3.606E+03f}, {1.50000E-03f, 1.191E+03f}, {2.00000E-03f, 5.279E+02f}, {3.00000E-03f, 1.625E+02f},
  {3.20290E-03f, 1.340E+02f}, {4.00000E-03f, 7.788E+01f}, {5.00000E-03f, 4.027E+01f}, {6.00000E-03f, 2.341E+01f},
  {8.00000E-03f, 9.921E+00f}, {1.00000E-02f, 5.120E+00f}, {1.50000E-02f, 1.614E+00f}, {2.00000E-02f, 7.779E-01f},
  {3.00000E-02f, 3.538E-01f}, {4.00000E-02f, 2.485E-01f}, {5.00000E-02f, 2.080E-01f}, {6.00000E-02f, 1.875E-01f},
  {8.00000E-02f, 1.662E-01f}, {1.00000E-01f, 1.541E-01f}, {1.50000E-01f, 1.356E-01f}, {2.00000E-01f, 1.233E-01f},
  {3.00000E-01f, 1.067E-01f}, {4.00000E-01f, 9.549E-02f}, {5.00000E-01f, 8.712E-02f}, {6.00000E-01f, 8.055E-02f},
  {8.00000E-01f, 7.074E-02f}, {1.00000E+00f, 6.358E-02f}, {1.25000E+00f, 5.687E-02f}, {1.50000E+00f, 5.175E-02f},
  {2.00000E+00f, 4.447E-02f}, {3.00000E+00f, 3.581E-02f}, {4.00000E+00f, 3.079E-02f}, {5.00000E+00f, 2.751E-02f},
  {6.00000E+00f, 2.522E-02f}, {8.00000E+00f, 2.225E-02f}, {1.00000E+01f, 2.045E-02f}, {1.50000E+01f, 1.810E-02f},
  {2.00000E+01f, 1.705E-02f}};
} // namespace

namespace xray
{
float defaultEnergyKeV()
{
  return k_defaultEnergyKeV;
}

LinearAttenuationCoefficients linearAttenuationCoefficients(float energyKeV)
{
  const float energyMeV = energyKeV / 1000.0f;
  if (
    energyMeV < k_airMassAttenCoeffs_cm2PerG.begin()->first || k_airMassAttenCoeffs_cm2PerG.rbegin()->first < energyMeV)
  {
    return {};
  }

  return {
    .water_cmInv = math::interpolate(energyMeV, k_waterMassAttenCoeffs_cm2PerG) * k_waterDensity_gPerCm3,
    .air_cmInv = math::interpolate(energyMeV, k_airMassAttenCoeffs_cm2PerG) * k_airDensity_gPerCm3};
}
} // namespace xray
