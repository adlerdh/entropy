#ifndef POISSON_H
#define POISSON_H

#include "common/SegmentationTypes.h"

#include <glm/fwd.hpp>

#include <array>
#include <cstdint>
#include <vector>

class Image;

void initializePotential(
  const uint8_t* seeds, float* potential, const glm::ivec3& dims, LabelType label
);

void computeResultSeg(
  const std::vector<const float*> potentials, uint8_t* resultSeg, const glm::ivec3& dims
);

void computeBinaryResultSeg(
  const std::array<const float*, 2> potentials, uint8_t* resultSeg, const glm::ivec3& dims
);

/**
 * @cite This code is a 3D extension of the algorithm from "Numerical Recipes in C":
 * "Successive over-relaxation solution of equation (19.5.25) with Chebyshev acceleration"
 * 'rjac' is input as the spectral radius of the Jacobi iteration, or an estimate of it.
 */
void sor(
  const uint8_t* seeds,
  const float* image,
  float* potential,
  const glm::ivec3& dims,
  const VoxelDistances& distances,
  float rjac,
  uint32_t maxits,
  float beta
);

// Compute a decent value for the 'beta' parameter used in SOR.
float computeBeta(const float* image, const glm::ivec3& dims);

#endif // POISSON_H
