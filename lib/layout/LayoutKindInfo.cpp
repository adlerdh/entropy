#include "layout/LayoutKindInfo.h"

namespace layout
{

bool isLightboxLayoutKind(LayoutKind kind)
{
  switch (kind) {
    case LayoutKind::AxialLightbox:
    case LayoutKind::CoronalLightbox:
    case LayoutKind::SagittalLightbox:
      return true;
    case LayoutKind::Custom:
    case LayoutKind::FourUp:
    case LayoutKind::Tri:
    case LayoutKind::SingleAxial:
    case LayoutKind::SingleCoronal:
    case LayoutKind::SingleSagittal:
    case LayoutKind::MultiImageAxialGrid:
    case LayoutKind::MultiImageCoronalGrid:
    case LayoutKind::MultiImageSagittalGrid:
    case LayoutKind::AxCorSagByImage:
    case LayoutKind::NumElements:
      return false;
  }
  return false;
}

bool isImageDependentManagedLayoutKind(LayoutKind kind)
{
  switch (kind) {
    case LayoutKind::SingleCoronal:
    case LayoutKind::SingleSagittal:
    case LayoutKind::MultiImageAxialGrid:
    case LayoutKind::MultiImageCoronalGrid:
    case LayoutKind::MultiImageSagittalGrid:
    case LayoutKind::AxCorSagByImage:
    case LayoutKind::AxialLightbox:
    case LayoutKind::CoronalLightbox:
    case LayoutKind::SagittalLightbox:
      return true;
    case LayoutKind::Custom:
    case LayoutKind::FourUp:
    case LayoutKind::Tri:
    case LayoutKind::SingleAxial:
    case LayoutKind::NumElements:
      return false;
  }
  return false;
}

bool isFixedManagedLayoutKind(LayoutKind kind)
{
  switch (kind) {
    case LayoutKind::FourUp:
    case LayoutKind::Tri:
    case LayoutKind::SingleAxial:
      return true;
    case LayoutKind::Custom:
    case LayoutKind::SingleCoronal:
    case LayoutKind::SingleSagittal:
    case LayoutKind::MultiImageAxialGrid:
    case LayoutKind::MultiImageCoronalGrid:
    case LayoutKind::MultiImageSagittalGrid:
    case LayoutKind::AxCorSagByImage:
    case LayoutKind::AxialLightbox:
    case LayoutKind::CoronalLightbox:
    case LayoutKind::SagittalLightbox:
    case LayoutKind::NumElements:
      return false;
  }
  return false;
}

LayoutKind lightboxLayoutKindForViewType(ViewType viewType)
{
  switch (viewType) {
    case ViewType::Axial:
      return LayoutKind::AxialLightbox;
    case ViewType::Coronal:
      return LayoutKind::CoronalLightbox;
    case ViewType::Sagittal:
      return LayoutKind::SagittalLightbox;
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
      return "Four-up";
    case LayoutKind::Tri:
      return "Three-up";
    case LayoutKind::SingleAxial:
      return "1-up axial";
    case LayoutKind::SingleCoronal:
      return "1-up coronal";
    case LayoutKind::SingleSagittal:
      return "1-up sagittal";
    case LayoutKind::MultiImageAxialGrid:
      return "Multi-image axial grid";
    case LayoutKind::MultiImageCoronalGrid:
      return "Multi-image coronal grid";
    case LayoutKind::MultiImageSagittalGrid:
      return "Multi-image sagittal grid";
    case LayoutKind::AxCorSagByImage:
      return "Axial/coronal/sagittal by image";
    case LayoutKind::AxialLightbox:
      return "Axial lightbox";
    case LayoutKind::CoronalLightbox:
      return "Coronal lightbox";
    case LayoutKind::SagittalLightbox:
      return "Sagittal lightbox";
    case LayoutKind::Custom:
      return isLightbox ? "Custom lightbox" : "Custom";
    case LayoutKind::NumElements:
      break;
  }
  return "Layout";
}

} // namespace layout
