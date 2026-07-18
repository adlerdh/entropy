#include "image/ImageSpatialMetadata.h"

#include <glm/geometric.hpp>

#include <cmath>

namespace
{
bool isFiniteVec3(const glm::vec3& value)
{
  return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
}

std::optional<glm::vec3> normalizedDirection(const glm::vec3& value, std::string* errorMessage)
{
  if (!isFiniteVec3(value)) {
    if (errorMessage) {
      *errorMessage = "Direction values must be finite";
    }
    return std::nullopt;
  }

  const float length = glm::length(value);
  if (length <= 0.0f) {
    if (errorMessage) {
      *errorMessage = "Direction vectors must be nonzero";
    }
    return std::nullopt;
  }

  return value / length;
}

glm::vec3 fallbackPerpendicular(const glm::vec3& axis)
{
  const glm::vec3 absAxis = glm::abs(axis);
  const glm::vec3 seed = (absAxis.x <= absAxis.y && absAxis.x <= absAxis.z) ? glm::vec3{1.0f, 0.0f, 0.0f}
                         : (absAxis.y <= absAxis.z)                         ? glm::vec3{0.0f, 1.0f, 0.0f}
                                                                            : glm::vec3{0.0f, 0.0f, 1.0f};
  return glm::normalize(seed - axis * glm::dot(seed, axis));
}

glm::vec3 projectOrFallback(const glm::vec3& candidate, const glm::vec3& normal)
{
  constexpr float k_minProjectedLength = 1.0e-6f;
  const glm::vec3 projected = candidate - normal * glm::dot(candidate, normal);
  const float length = glm::length(projected);
  if (length > k_minProjectedLength) {
    return projected / length;
  }
  return fallbackPerpendicular(normal);
}
} // namespace

bool validateImageSpatialMetadata(const ImageSpatialMetadata& metadata, std::string* errorMessage)
{
  if (
    !isFiniteVec3(metadata.spacingMm) || metadata.spacingMm.x <= 0.0f || metadata.spacingMm.y <= 0.0f ||
    metadata.spacingMm.z <= 0.0f)
  {
    if (errorMessage) {
      *errorMessage = "Spacing values must be positive";
    }
    return false;
  }

  if (!isFiniteVec3(metadata.originMm)) {
    if (errorMessage) {
      *errorMessage = "Origin values must be finite";
    }
    return false;
  }

  const glm::vec3 x = metadata.directions[0];
  const glm::vec3 y = metadata.directions[1];
  const glm::vec3 z = metadata.directions[2];
  if (!isFiniteVec3(x) || !isFiniteVec3(y) || !isFiniteVec3(z)) {
    if (errorMessage) {
      *errorMessage = "Direction values must be finite";
    }
    return false;
  }

  if (glm::length(x) <= 0.0f || glm::length(y) <= 0.0f || glm::length(z) <= 0.0f) {
    if (errorMessage) {
      *errorMessage = "Direction vectors must be nonzero";
    }
    return false;
  }

  if (glm::dot(glm::cross(x, y), z) <= 0.0f) {
    if (errorMessage) {
      *errorMessage = "Direction vectors must form a right-handed basis";
    }
    return false;
  }

  return true;
}

std::optional<glm::mat3>
normalizedRasterDirectionMatrix(const glm::vec3& xDirection, const glm::vec3& yDirection, std::string* errorMessage)
{
  if (!isFiniteVec3(xDirection) || !isFiniteVec3(yDirection)) {
    if (errorMessage) {
      *errorMessage = "Direction values must be finite";
    }
    return std::nullopt;
  }

  const float xLength = glm::length(xDirection);
  const float yLength = glm::length(yDirection);
  if (xLength <= 0.0f || yLength <= 0.0f) {
    if (errorMessage) {
      *errorMessage = "Direction vectors must be nonzero";
    }
    return std::nullopt;
  }

  const glm::vec3 x = xDirection / xLength;
  glm::vec3 y = yDirection - x * glm::dot(yDirection, x);
  const float orthogonalYLength = glm::length(y);
  if (orthogonalYLength <= 1.0e-6f) {
    if (errorMessage) {
      *errorMessage = "Direction vectors must not be parallel";
    }
    return std::nullopt;
  }
  y /= orthogonalYLength;

  const glm::vec3 z = glm::normalize(glm::cross(x, y));
  return glm::mat3{x, y, z};
}

std::optional<glm::mat3> normalizedRasterDirectionMatrixAfterEdit(
  const glm::mat3& directions,
  ImageDirectionAxis editedAxis,
  std::string* errorMessage)
{
  const glm::vec3 iCandidate = directions[0];
  const glm::vec3 jCandidate = directions[1];
  const glm::vec3 kCandidate = directions[2];

  glm::vec3 i{1.0f, 0.0f, 0.0f};
  glm::vec3 j{0.0f, 1.0f, 0.0f};
  glm::vec3 k{0.0f, 0.0f, 1.0f};

  switch (editedAxis) {
    case ImageDirectionAxis::I: {
      const auto editedI = normalizedDirection(iCandidate, errorMessage);
      if (!editedI) {
        return std::nullopt;
      }
      i = *editedI;
      j = projectOrFallback(jCandidate, i);
      k = glm::normalize(glm::cross(i, j));
      j = glm::normalize(glm::cross(k, i));
      break;
    }
    case ImageDirectionAxis::J: {
      const auto editedJ = normalizedDirection(jCandidate, errorMessage);
      if (!editedJ) {
        return std::nullopt;
      }
      j = *editedJ;
      i = projectOrFallback(iCandidate, j);
      k = glm::normalize(glm::cross(i, j));
      i = glm::normalize(glm::cross(j, k));
      break;
    }
    case ImageDirectionAxis::K: {
      const auto editedK = normalizedDirection(kCandidate, errorMessage);
      if (!editedK) {
        return std::nullopt;
      }
      k = *editedK;
      i = projectOrFallback(iCandidate, k);
      j = glm::normalize(glm::cross(k, i));
      i = glm::normalize(glm::cross(j, k));
      break;
    }
  }

  return glm::mat3{i, j, k};
}
