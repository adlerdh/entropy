#include "ui/windows/ViewOverlayModel.h"

namespace entropy::ui::view_overlay
{

bool usesVisibleImageSelection(const ViewRenderMode renderMode)
{
  return ViewRenderMode::Image == renderMode || ViewRenderMode::VolumeRender == renderMode;
}

bool usesMetricImageSelection(const ViewRenderMode renderMode)
{
  return ViewRenderMode::Disabled != renderMode && !usesVisibleImageSelection(renderMode);
}

bool usesDisabledVisibilityIcon(const ViewRenderMode renderMode)
{
  return ViewRenderMode::Disabled == renderMode;
}

std::string imageChoiceLabel(const ImageChoice& choice)
{
  std::string label = choice.displayName;

  if (!choice.visible) {
    label += " (hidden)";
  }

  if (choice.active) {
    label += " (active)";
  }

  if (choice.reference) {
    label += " (ref.)";
  }

  return label;
}

std::string selectedVisibleImageNames(const std::vector<ImageChoice>& choices)
{
  std::string label;
  bool first = true;

  for (const ImageChoice& choice : choices) {
    if (!choice.selected || !choice.visible) {
      continue;
    }

    if (!first) {
      label += ", ";
    }

    if (choice.active) {
      label += choice.displayName + " (active)";
    }
    else {
      label += choice.displayName;
    }

    if (choice.reference) {
      label += " (ref.)";
    }

    first = false;
  }

  return label;
}

} // namespace entropy::ui::view_overlay
