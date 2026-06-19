#include "layout/LayoutKindInfo.h"

namespace layout
{

bool isLightboxLayoutKind(LayoutKind kind)
{
  switch (kind) {
    case LayoutKind::Lightbox:
      return true;
    case LayoutKind::Custom:
    case LayoutKind::FourUp:
    case LayoutKind::ThreeUp:
    case LayoutKind::OneUp:
    case LayoutKind::MultiImageGrid:
    case LayoutKind::AxCorSagByImage:
    case LayoutKind::NumElements:
      return false;
  }
  return false;
}

bool isImageDependentManagedLayoutKind(LayoutKind kind)
{
  switch (kind) {
    case LayoutKind::OneUp:
    case LayoutKind::MultiImageGrid:
    case LayoutKind::AxCorSagByImage:
    case LayoutKind::Lightbox:
      return true;
    case LayoutKind::Custom:
    case LayoutKind::FourUp:
    case LayoutKind::ThreeUp:
    case LayoutKind::NumElements:
      return false;
  }
  return false;
}

bool isFixedManagedLayoutKind(LayoutKind kind)
{
  switch (kind) {
    case LayoutKind::FourUp:
    case LayoutKind::ThreeUp:
      return true;
    case LayoutKind::Custom:
    case LayoutKind::OneUp:
    case LayoutKind::MultiImageGrid:
    case LayoutKind::AxCorSagByImage:
    case LayoutKind::Lightbox:
    case LayoutKind::NumElements:
      return false;
  }
  return false;
}

LayoutKind lightboxLayoutKindForViewType(ViewType viewType)
{
  switch (viewType) {
    case ViewType::Axial:
    case ViewType::Coronal:
    case ViewType::Sagittal:
      return LayoutKind::Lightbox;
    case ViewType::Oblique:
    case ViewType::ThreeD:
    case ViewType::NumElements:
      return LayoutKind::Custom;
  }
  return LayoutKind::Custom;
}

std::string_view layoutDisplayName(LayoutKind kind, bool isLightbox)
{
  switch (kind) {
    case LayoutKind::FourUp:
      return "4-Up";
    case LayoutKind::ThreeUp:
      return "3-Up";
    case LayoutKind::OneUp:
      return "1-Up";
    case LayoutKind::MultiImageGrid:
    case LayoutKind::AxCorSagByImage:
      return "Grid";
    case LayoutKind::Lightbox:
      return "Lightbox";
    case LayoutKind::Custom:
      return isLightbox ? "Lightbox" : "Custom";
    case LayoutKind::NumElements:
      break;
  }
  return "Layout";
}

} // namespace layout
