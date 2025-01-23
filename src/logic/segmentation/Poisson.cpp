#include "logic/segmentation/Poisson.h"

#include <glm/glm.hpp>
#include <spdlog/spdlog.h>

#include <cmath>
#include <limits>

namespace
{

// Normalize image to rage [0.0, 1.0].
//void normalize( const float* image, float* imageNorm, std::size_t N )
//{
//    float maxVal = std::numeric_limits<float>::lowest();
//    float minVal = std::numeric_limits<float>::max();

//    for ( std::size_t i = 0; i < N; ++i )
//    {
//        const float val = image[i];

//        if ( val > maxVal )
//        {
//            maxVal = val;
//        }

//        if ( val < minVal )
//        {
//            minVal = val;
//        }
//    }

//    for ( std::size_t i = 0; i < N; ++i )
//    {
//        imageNorm[i] = ( image[i] - minVal ) / ( maxVal - minVal );
//    }
//}

} // namespace

void initializePotential(
  const uint8_t* seeds, float* potential, const glm::ivec3& dims, LabelType label
)
{
  int n = 0;

  const int zDelta = dims.x * dims.y;
  const int yDelta = dims.x;

  for (int k = 0; k < dims.z; ++k)
  {
    for (int j = 0; j < dims.y; ++j)
    {
      for (int i = 0; i < dims.x; ++i)
      {
        n = k * zDelta + j * yDelta + i;

        const uint8_t seed = seeds[n];

        if (0u == label)
        {
          // Set all labels:
          potential[n] = static_cast<float>(seed);
        }
        else
        {
          // Set one label:
          if (label == seed)
          {
            // Turn on potential for the label
            potential[n] = 2.0f;
          }
          else if (0u < seed)
          {
            // Ground potential for all other labels
            potential[n] = 1.0f;
          }
          else if (0u == seed)
          {
            potential[n] = 0.0f;
          }
        }
      }
    }
  }
}

void computeBinaryResultSeg(
  const std::array<const float*, 2> potentials, uint8_t* resultSeg, const glm::ivec3& dims
)
{
  int n = 0;

  const int zDelta = dims.x * dims.y;
  const int yDelta = dims.x;

  for (int k = 0; k < dims.z; ++k)
  {
    for (int j = 0; j < dims.y; ++j)
    {
      for (int i = 0; i < dims.x; ++i)
      {
        n = k * zDelta + j * yDelta + i;

        if (potentials[0][n] > potentials[1][n])
        {
          resultSeg[n] = 1u;
        }
        else
        {
          resultSeg[n] = 0u;
        }
      }
    }
  }
}

void computeResultSeg(
  const std::vector<const float*> potentials, uint8_t* resultSeg, const glm::ivec3& dims
)
{
  const int zDelta = dims.x * dims.y;
  const int yDelta = dims.x;

  for (int k = 0; k < dims.z; ++k)
  {
    for (int j = 0; j < dims.y; ++j)
    {
      for (int i = 0; i < dims.x; ++i)
      {
        const int n = k * zDelta + j * yDelta + i;

        float maxPotential = 0.0f;
        uint32_t maxComp = 0u;

        for (uint32_t c = 0; c < potentials.size(); ++c)
        {
          const float p = potentials[c][n];
          if (p > maxPotential)
          {
            maxPotential = p;
            maxComp = c;
          }
        }

        resultSeg[n] = std::min(maxComp + 1u, 255u);
      }
    }
  }
}

void sor(
  const uint8_t* seeds,
  const float* /*image*/,
  float* potential,
  const glm::ivec3& dims,
  const VoxelDistances& distances,
  float rjac,
  uint32_t maxits,
  float /*beta*/
)
{
  const std::size_t N = dims.x * dims.y * dims.z;

  std::vector<float> imageNorm(N, 0.0f);

  //    normalize( image, imageNorm.data(), N );

  const float BETA = 1.0f; //computeBeta( imageNorm.data(), dims );

  int n = 0;
  int isw, jsw, ksw;

  float omega = 1.0f;
  float val, pot, grad, weight;

  float resid = 0.0f;
  float total = 0.0f;

  const int zDelta = dims.x * dims.y;
  const int yDelta = dims.x;
  const int xDelta = 1;

  for (uint32_t iter = 0; iter < maxits; ++iter)
  {
    float absResid = 0.0f;

    // Split updates into even and odd stencil passes:
    ksw = 0;

    for (int pass = 0; pass < 2; ++pass)
    {
      jsw = ksw;

      for (int k = 0; k < dims.z; ++k)
      {
        isw = jsw;

        for (int j = 0; j < dims.y; ++j)
        {
          for (int i = isw; i < dims.x; i += 2)
          {
            n = k * zDelta + j * yDelta + i;

            if (0 != seeds[n])
            {
              // Do not update nodes on boundary
              continue;
            }

            val = imageNorm[n];
            pot = potential[n];

            resid = 0.0f;
            total = 0.0f;

            if (k < dims.z - 1)
            {
              grad = 0.0f * (val - imageNorm[n + zDelta]) / BETA;
              weight = std::exp(-0.5f * grad * grad) / distances.distZ;
              resid += weight * potential[n + zDelta];
              total -= weight;
            }

            if (k > 0)
            {
              grad = 0.0f * (val - imageNorm[n - zDelta]) / BETA;
              weight = std::exp(-0.5f * grad * grad) / distances.distZ;
              resid += weight * potential[n - zDelta];
              total -= weight;
            }

            if (j < dims.y - 1)
            {
              grad = 0.0f * (val - imageNorm[n + yDelta]) / BETA;
              weight = std::exp(-0.5f * grad * grad) / distances.distY;
              resid += weight * potential[n + yDelta];
              total -= weight;
            }

            if (j > 0)
            {
              grad = 0.0f * (val - imageNorm[n - yDelta]) / BETA;
              weight = std::exp(-0.5f * grad * grad) / distances.distY;
              resid += weight * potential[n - yDelta];
              total -= weight;
            }

            if (i < dims.x - 1)
            {
              grad = 0.0f * (val - imageNorm[n + xDelta]) / BETA;
              weight = std::exp(-0.5f * grad * grad) / distances.distX;
              resid += weight * potential[n + xDelta];
              total -= weight;
            }

            if (i > 0)
            {
              grad = 0.0f * (val - imageNorm[n - xDelta]) / BETA;
              weight = std::exp(-0.5f * grad * grad) / distances.distX;
              resid += weight * potential[n - xDelta];
              total -= weight;
            }

            resid += total * pot;
            potential[n] -= omega * resid / total;
            absResid += std::fabs(resid);
          }

          isw = 1 - isw;
        }

        jsw = 1 - jsw;
      }

      ksw = 1 - ksw;

      omega = (0 == iter && 0 == pass) ? 1.0f / (1.0f - 0.5f * rjac * rjac)
                                       : 1.0f / (1.0f - 0.25f * rjac * rjac * omega);
    }

    if (0 == iter % 100)
    {
      spdlog::trace("Iteration {}, residual = {}", iter, absResid);
    }
  }
}

float computeBeta(const float* image, const glm::ivec3& dims)
{
  int n = 0;
  float grad = 0.0f;

  const int zDelta = dims.x * dims.y;
  const int yDelta = dims.x;
  const int xDelta = 1;

  for (int k = 0; k < dims.z; ++k)
  {
    for (int j = 0; j < dims.y; ++j)
    {
      for (int i = 0; i < dims.x; ++i)
      {
        n = k * zDelta + j * yDelta + i;
        const float val = image[n];

        if (k < dims[2] - 1)
        {
          grad += std::fabs(val - image[n + zDelta]);
        }

        if (k > 0)
        {
          grad += std::fabs(val - image[n - zDelta]);
        }

        if (j < dims.y - 1)
        {
          grad += std::fabs(val - image[n + yDelta]);
        }

        if (j > 0)
        {
          grad += std::fabs(val - image[n - yDelta]);
        }

        if (i < dims.x - 1)
        {
          grad += std::fabs(val - image[n + xDelta]);
        }

        if (i > 0)
        {
          grad += std::fabs(val - image[n - xDelta]);
        }
      }
    }
  }

  return grad / static_cast<float>(dims.x * dims.y * dims.z);
}

//input initial approximation to  potential (double format)
//output potential (double format)
//potential boundary values in (int16_t format)
//maximum number of SOR iterations
//(optional): input conductivity image in (double format)
