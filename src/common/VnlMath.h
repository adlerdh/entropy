#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <vnl/vnl_matrix_fixed.h>

namespace math::convert
{

/**
 * @brief Convert a 3x3 GLM matrix to a 3x3 VNL matrix
 */
template<class T>
vnl_matrix_fixed<T, 3, 3> toVnlMatrixFixed(const glm::tmat3x3<T, glm::highp>& glmMatrix)
{
  const vnl_matrix_fixed<T, 3, 3> vnlMatrixTransposed(glm::value_ptr(glmMatrix));
  return vnlMatrixTransposed.transpose();
}

} // namespace math::convert
