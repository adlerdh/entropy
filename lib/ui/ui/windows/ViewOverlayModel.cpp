#include "ui/windows/ViewOverlayModel.h"

namespace ui::view_overlay
{

bool usesVisibleImageSelection(const ViewRenderMode renderMode)
{
  return ViewRenderMode::Image == renderMode;
}

bool usesMetricImageSelection(const ViewRenderMode renderMode)
{
  return isComparisonRenderMode(renderMode);
}

bool usesDisabledVisibilityIcon(const ViewRenderMode renderMode)
{
  return ViewRenderMode::Disabled == renderMode;
}

bool usesAnatomicalViewTypeSelector(const ViewRenderMode renderMode)
{
  return ViewRenderMode::JointHistogram != renderMode;
}

std::string imageChoiceLabel(const ImageChoice& choice)
{
  std::string label = choice.displayName;

  if (!choice.visible) {
    label += " (hidden)";
  }

  if (choice.reference && choice.active) {
    label += " (ref + active)";
  }
  else if (choice.reference) {
    label += " (ref)";
  }
  else if (choice.active) {
    label += " (active)";
  }

  return label;
}

} // namespace ui::view_overlay
