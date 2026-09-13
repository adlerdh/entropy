#include "ui/dialogs/WarpFieldAssignment.h"

#include "image/Image.h"
#include "ui/dialogs/NativeMessageDialogs.h"

#include <glm/gtc/epsilon.hpp>

#include <cstddef>
#include <limits>

namespace warp_field_assignment
{
namespace
{
struct WorldAabb
{
  glm::vec3 min{std::numeric_limits<float>::max()};
  glm::vec3 max{std::numeric_limits<float>::lowest()};
};

constexpr float k_geometryEpsilon = 1.0e-4f;
constexpr float k_domainToleranceMm = 0.5f;

WorldAabb imageWorldAabb(const Image& image)
{
  WorldAabb box;
  const glm::mat4 world_T_subject = image.transformations().worldDef_T_subject();
  for (const glm::vec3& corner : image.header().subjectBBoxCorners()) {
    const glm::vec3 worldCorner{world_T_subject * glm::vec4{corner, 1.0f}};
    box.min = glm::min(box.min, worldCorner);
    box.max = glm::max(box.max, worldCorner);
  }
  return box;
}

bool worldAabbContains(const WorldAabb& domain, const WorldAabb& target)
{
  return glm::all(glm::lessThanEqual(domain.min, target.min + glm::vec3{k_domainToleranceMm})) &&
         glm::all(glm::greaterThanEqual(domain.max, target.max - glm::vec3{k_domainToleranceMm}));
}

bool vec3NearlyEqual(const glm::vec3& a, const glm::vec3& b)
{
  return glm::all(glm::epsilonEqual(a, b, k_geometryEpsilon));
}

std::vector<std::string> domainWarnings(const Image& field, const Image& target)
{
  std::vector<std::string> warnings;
  if (field.header().numComponentsPerPixel() < 3u) {
    warnings.emplace_back("Fewer than three components per voxel");
    return warnings;
  }
  if (field.header().pixelDimensions() != target.header().pixelDimensions()) {
    warnings.emplace_back("Grid dimensions differ");
  }
  if (!vec3NearlyEqual(field.header().spacing(), target.header().spacing())) {
    warnings.emplace_back("Voxel spacing differs");
  }
  if (!vec3NearlyEqual(field.header().origin(), target.header().origin())) {
    warnings.emplace_back("Origin differs");
  }
  if (
    !vec3NearlyEqual(field.header().directions()[0], target.header().directions()[0]) ||
    !vec3NearlyEqual(field.header().directions()[1], target.header().directions()[1]) ||
    !vec3NearlyEqual(field.header().directions()[2], target.header().directions()[2]))
  {
    warnings.emplace_back("Direction matrix differs");
  }
  if (!worldAabbContains(imageWorldAabb(field), imageWorldAabb(target))) {
    warnings.emplace_back("Physical domain does not fully cover the reference");
  }
  return warnings;
}

std::string joinWarnings(const std::vector<std::string>& warnings)
{
  std::string text;
  for (std::size_t i = 0; i < warnings.size(); ++i) {
    if (!text.empty()) {
      text += '\n';
    }
    text += i == 0u ? warnings[i] : "- " + warnings[i];
  }
  return text;
}
} // namespace

std::vector<std::string> inverseWarnings(const Image& field, const Image& referenceImage)
{
  std::vector<std::string> warnings = domainWarnings(field, referenceImage);
  if (!warnings.empty()) {
    warnings.insert(warnings.begin(), "Warning! The inverse warp field differs from the reference:");
  }
  return warnings;
}

std::vector<std::string> forwardWarnings(const Image& field, const Image& movingImage, const Image* referenceImage)
{
  std::vector<std::string> warnings = domainWarnings(field, movingImage);
  if (warnings.empty() || (referenceImage && domainWarnings(field, *referenceImage).empty())) {
    return {};
  }
  warnings.insert(
    warnings.begin(),
    "The forward warp field does not match either the moving-image space or the reference-image space.");
  return warnings;
}

bool confirm(const char* title, const std::vector<std::string>& warnings)
{
  if (warnings.empty()) {
    return true;
  }
  const auto result = native_dialog::showMessageDialog(
    {title,
     "The selected warp field may not match the expected image space.",
     joinWarnings(warnings),
     "Use warp field",
     "Cancel",
     ""});
  return !result || native_dialog::MessageDialogResult::FirstButton == *result;
}

} // namespace warp_field_assignment
