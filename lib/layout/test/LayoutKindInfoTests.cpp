#include "layout/LayoutKindInfo.h"

#include <catch2/catch_test_macros.hpp>

TEST_CASE("layout kind display names cover built-in layouts", "[layout][layout_kind]")
{
  CHECK(layout::layoutDisplayName(LayoutKind::FourUp, false) == "Four-up");
  CHECK(layout::layoutDisplayName(LayoutKind::Tri, false) == "Three-up");
  CHECK(layout::layoutDisplayName(LayoutKind::SingleAxial, false) == "1-up axial");
  CHECK(layout::layoutDisplayName(LayoutKind::SingleCoronal, false) == "1-up coronal");
  CHECK(layout::layoutDisplayName(LayoutKind::SingleSagittal, false) == "1-up sagittal");
  CHECK(layout::layoutDisplayName(LayoutKind::MultiImageAxialGrid, false) == "Multi-image axial grid");
  CHECK(layout::layoutDisplayName(LayoutKind::MultiImageCoronalGrid, false) == "Multi-image coronal grid");
  CHECK(layout::layoutDisplayName(LayoutKind::MultiImageSagittalGrid, false) == "Multi-image sagittal grid");
  CHECK(layout::layoutDisplayName(LayoutKind::AxCorSagByImage, false) == "Axial/coronal/sagittal by image");
  CHECK(layout::layoutDisplayName(LayoutKind::AxialLightbox, false) == "Axial lightbox");
  CHECK(layout::layoutDisplayName(LayoutKind::CoronalLightbox, false) == "Coronal lightbox");
  CHECK(layout::layoutDisplayName(LayoutKind::SagittalLightbox, false) == "Sagittal lightbox");
}

TEST_CASE("custom layout display names reflect lightbox state", "[layout][layout_kind]")
{
  CHECK(layout::layoutDisplayName(LayoutKind::Custom, false) == "Custom");
  CHECK(layout::layoutDisplayName(LayoutKind::Custom, true) == "Custom lightbox");
}

TEST_CASE("unknown layout kind display names fall back to Layout", "[layout][layout_kind]")
{
  CHECK(layout::layoutDisplayName(LayoutKind::NumElements, false) == "Layout");
}

TEST_CASE("layout kind lightbox classification covers managed lightboxes", "[layout][layout_kind]")
{
  CHECK(layout::isLightboxLayoutKind(LayoutKind::AxialLightbox));
  CHECK(layout::isLightboxLayoutKind(LayoutKind::CoronalLightbox));
  CHECK(layout::isLightboxLayoutKind(LayoutKind::SagittalLightbox));

  CHECK_FALSE(layout::isLightboxLayoutKind(LayoutKind::Custom));
  CHECK_FALSE(layout::isLightboxLayoutKind(LayoutKind::FourUp));
  CHECK_FALSE(layout::isLightboxLayoutKind(LayoutKind::Tri));
  CHECK_FALSE(layout::isLightboxLayoutKind(LayoutKind::SingleAxial));
  CHECK_FALSE(layout::isLightboxLayoutKind(LayoutKind::SingleCoronal));
  CHECK_FALSE(layout::isLightboxLayoutKind(LayoutKind::SingleSagittal));
  CHECK_FALSE(layout::isLightboxLayoutKind(LayoutKind::MultiImageAxialGrid));
  CHECK_FALSE(layout::isLightboxLayoutKind(LayoutKind::MultiImageCoronalGrid));
  CHECK_FALSE(layout::isLightboxLayoutKind(LayoutKind::MultiImageSagittalGrid));
  CHECK_FALSE(layout::isLightboxLayoutKind(LayoutKind::AxCorSagByImage));
  CHECK_FALSE(layout::isLightboxLayoutKind(LayoutKind::NumElements));
}

TEST_CASE("layout kind image dependency classification covers managed layouts", "[layout][layout_kind]")
{
  CHECK(layout::isImageDependentManagedLayoutKind(LayoutKind::SingleCoronal));
  CHECK(layout::isImageDependentManagedLayoutKind(LayoutKind::SingleSagittal));
  CHECK(layout::isImageDependentManagedLayoutKind(LayoutKind::MultiImageAxialGrid));
  CHECK(layout::isImageDependentManagedLayoutKind(LayoutKind::MultiImageCoronalGrid));
  CHECK(layout::isImageDependentManagedLayoutKind(LayoutKind::MultiImageSagittalGrid));
  CHECK(layout::isImageDependentManagedLayoutKind(LayoutKind::AxCorSagByImage));
  CHECK(layout::isImageDependentManagedLayoutKind(LayoutKind::AxialLightbox));
  CHECK(layout::isImageDependentManagedLayoutKind(LayoutKind::CoronalLightbox));
  CHECK(layout::isImageDependentManagedLayoutKind(LayoutKind::SagittalLightbox));

  CHECK_FALSE(layout::isImageDependentManagedLayoutKind(LayoutKind::Custom));
  CHECK_FALSE(layout::isImageDependentManagedLayoutKind(LayoutKind::FourUp));
  CHECK_FALSE(layout::isImageDependentManagedLayoutKind(LayoutKind::Tri));
  CHECK_FALSE(layout::isImageDependentManagedLayoutKind(LayoutKind::SingleAxial));
  CHECK_FALSE(layout::isImageDependentManagedLayoutKind(LayoutKind::NumElements));
}

TEST_CASE("layout kind fixed managed classification covers initial layouts", "[layout][layout_kind]")
{
  CHECK(layout::isFixedManagedLayoutKind(LayoutKind::FourUp));
  CHECK(layout::isFixedManagedLayoutKind(LayoutKind::Tri));
  CHECK(layout::isFixedManagedLayoutKind(LayoutKind::SingleAxial));

  CHECK_FALSE(layout::isFixedManagedLayoutKind(LayoutKind::Custom));
  CHECK_FALSE(layout::isFixedManagedLayoutKind(LayoutKind::SingleCoronal));
  CHECK_FALSE(layout::isFixedManagedLayoutKind(LayoutKind::SingleSagittal));
  CHECK_FALSE(layout::isFixedManagedLayoutKind(LayoutKind::MultiImageAxialGrid));
  CHECK_FALSE(layout::isFixedManagedLayoutKind(LayoutKind::MultiImageCoronalGrid));
  CHECK_FALSE(layout::isFixedManagedLayoutKind(LayoutKind::MultiImageSagittalGrid));
  CHECK_FALSE(layout::isFixedManagedLayoutKind(LayoutKind::AxCorSagByImage));
  CHECK_FALSE(layout::isFixedManagedLayoutKind(LayoutKind::AxialLightbox));
  CHECK_FALSE(layout::isFixedManagedLayoutKind(LayoutKind::CoronalLightbox));
  CHECK_FALSE(layout::isFixedManagedLayoutKind(LayoutKind::SagittalLightbox));
  CHECK_FALSE(layout::isFixedManagedLayoutKind(LayoutKind::NumElements));
}

TEST_CASE("lightbox layout kind follows supported slice view type", "[layout][layout_kind]")
{
  CHECK(layout::lightboxLayoutKindForViewType(ViewType::Axial) == LayoutKind::AxialLightbox);
  CHECK(layout::lightboxLayoutKindForViewType(ViewType::Coronal) == LayoutKind::CoronalLightbox);
  CHECK(layout::lightboxLayoutKindForViewType(ViewType::Sagittal) == LayoutKind::SagittalLightbox);
  CHECK(layout::lightboxLayoutKindForViewType(ViewType::Oblique) == LayoutKind::Custom);
  CHECK(layout::lightboxLayoutKindForViewType(ViewType::ThreeD) == LayoutKind::Custom);
  CHECK(layout::lightboxLayoutKindForViewType(ViewType::NumElements) == LayoutKind::Custom);
}
