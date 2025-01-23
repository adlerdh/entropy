#pragma once

#include "image/ImageUtility.tpp"

#include <glm/glm.hpp>
#include <itkImage.h>
#include <itkVector.h>

#include <algorithm>
#include <array>
#include <iostream>
#include <vector>

void test()
{
  /*
    std::vector<double> A{0.5377,    1.8339,   -2.2588,    0.8622,    0.3188,   -1.3077,   -0.4336};
    std::sort(std::begin(A), std::end(A));

    const auto Q0 = quantileToValue(std::span{A}, 0.3);
    const auto Q1 = quantileToValue(std::span{A}, 0.0);
    const auto Q2 = quantileToValue(std::span{A}, 0.025);
    const auto Q3 = quantileToValue(std::span{A}, 0.25);
    const auto Q4 = quantileToValue(std::span{A}, 0.5);
    const auto Q5 = quantileToValue(std::span{A}, 0.75);
    const auto Q6 = quantileToValue(std::span{A}, 0.975);
    const auto Q7 = quantileToValue(std::span{A}, 1.0);

    if (Q0) { std::cout << "Q0: " << *Q0 << std::endl; }
    if (Q1) { std::cout << "Q1: " << *Q1 << std::endl; }
    if (Q2) { std::cout << "Q2: " << *Q2 << std::endl; }
    if (Q3) { std::cout << "Q3: " << *Q3 << std::endl; }
    if (Q4) { std::cout << "Q4: " << *Q4 << std::endl; }
    if (Q5) { std::cout << "Q5: " << *Q5 << std::endl; }
    if (Q6) { std::cout << "Q6: " << *Q6 << std::endl; }
    if (Q7) { std::cout << "Q7: " << *Q7 << std::endl; }
*/

  const std::vector<int> data{1, 2, 4, 5, 5, 6};

  for (int i = 0; i < 8; ++i)
  {
    // Search for first element x such that i ≤ x
    auto lower = std::lower_bound(data.begin(), data.end(), i);

    std::cout << i << " ≤ ";
    lower != data.end() ? std::cout << *lower << " at index " << std::distance(data.begin(), lower)
                        : std::cout << "not found";
    std::cout << '\n';
  }

  return;

  /*
    uint32_t Ni = 128;
    uint32_t Nj = 128;
    uint32_t Nk = 128;

    glm::vec3 center(Ni/2, Nj/2, Nk/2);

    const std::array< std::array<double, 3>, 3 > dirs
        {
            std::array<double, 3>{1,0,0},
            std::array<double, 3>{0,1,0},
            std::array<double, 3>{0,0,1}
        };

    std::vector<float> im(Ni*Nj*Nk, 0.0f);

    for (uint32_t k = 0; k < Nk; ++k) {
        for (uint32_t j = 0; j < Nj; ++j) {
            for (uint32_t i = 0; i < Ni; ++i)
            {
                glm::vec3 p(i,j,k);
                im[i + j*Ni + k*Ni*Nj] = glm::distance(p, center);
            }
        }
    }

    const itk::Image<float, 3>::Pointer image = makeScalarImage(
        {Ni, Nj, Nk}, {0,0,0}, {1,1,1}, dirs, im.data());

    writeImage<float, 3, false>(image, "/Users/danadler/im.nrrd");


    std::vector<float> def(Ni*Nj*Nk*3, 0.0f);

    for (uint32_t k = 0; k < Nk; ++k) {
        for (uint32_t j = 0; j < Nj; ++j) {
            for (uint32_t i = 0; i < Ni; ++i)
            {
                uint32_t index = 3 * (i + j*Ni + k*Ni*Nj);
                glm::vec3 p(i,j,k);
                glm::vec3 d = glm::normalize(p - center);
                def[index + 0] = d.x;
                def[index + 1] = d.y;
                def[index + 2] = d.z;
            }
        }
    }

    typename itk::Image<itk::Vector<float, 3>, 3>::Pointer field = makeVectorImage<float, 3>(
        {Ni, Nj, Nk}, {0,0,0}, {1,1,1}, dirs, def.data() );

    writeImage<itk::Vector<float, 3>, 3, false>(field, "/Users/danadler/def.nii.gz");
*/
}
