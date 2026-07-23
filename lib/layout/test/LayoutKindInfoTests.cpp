#include "layout/LayoutKindInfo.h"

#include <catch2/catch_test_macros.hpp>

TEST_CASE("layout kind display names cover built-in layouts", "[layout][layout_kind]")
{
  CHECK(layout::layoutDisplayName(LayoutKind::FourUp, false) == "4-Up");
  CHECK(layout::layoutDisplayName(LayoutKind::ThreeUp, false) == "3-Up");
  CHECK(layout::layoutDisplayName(LayoutKind::OneUp, false) == "1-Up");
  CHECK(layout::layoutDisplayName(LayoutKind::MultiImageGrid, false) == "Grid");
  CHECK(layout::layoutDisplayName(LayoutKind::AxCorSagByImage, false) == "Grid");
  CHECK(layout::layoutDisplayName(LayoutKind::Lightbox, false) == "Lightbox");
}

TEST_CASE("custom layout display names reflect lightbox state", "[layout][layout_kind]")
{
  CHECK(layout::layoutDisplayName(LayoutKind::Custom, false) == "Custom");
  CHECK(layout::layoutDisplayName(LayoutKind::Custom, true) == "Lightbox");
}

TEST_CASE("unknown layout kind display names fall back to Layout", "[layout][layout_kind]")
{
  CHECK(layout::layoutDisplayName(LayoutKind::NumElements, false) == "Layout");
}

TEST_CASE("layout kind lightbox classification covers managed lightboxes", "[layout][layout_kind]")
{
  CHECK(layout::isLightboxLayoutKind(LayoutKind::Lightbox));

  CHECK_FALSE(layout::isLightboxLayoutKind(LayoutKind::Custom));
  CHECK_FALSE(layout::isLightboxLayoutKind(LayoutKind::FourUp));
  CHECK_FALSE(layout::isLightboxLayoutKind(LayoutKind::ThreeUp));
  CHECK_FALSE(layout::isLightboxLayoutKind(LayoutKind::OneUp));
  CHECK_FALSE(layout::isLightboxLayoutKind(LayoutKind::MultiImageGrid));
  CHECK_FALSE(layout::isLightboxLayoutKind(LayoutKind::AxCorSagByImage));
  CHECK_FALSE(layout::isLightboxLayoutKind(LayoutKind::NumElements));
}

TEST_CASE("layout kind image dependency classification covers managed layouts", "[layout][layout_kind]")
{
  CHECK(layout::isImageDependentManagedLayoutKind(LayoutKind::OneUp));
  CHECK(layout::isImageDependentManagedLayoutKind(LayoutKind::MultiImageGrid));
  CHECK(layout::isImageDependentManagedLayoutKind(LayoutKind::AxCorSagByImage));
  CHECK(layout::isImageDependentManagedLayoutKind(LayoutKind::Lightbox));

  CHECK_FALSE(layout::isImageDependentManagedLayoutKind(LayoutKind::Custom));
  CHECK_FALSE(layout::isImageDependentManagedLayoutKind(LayoutKind::FourUp));
  CHECK_FALSE(layout::isImageDependentManagedLayoutKind(LayoutKind::ThreeUp));
  CHECK_FALSE(layout::isImageDependentManagedLayoutKind(LayoutKind::NumElements));
}

TEST_CASE("layout kind fixed managed classification covers initial layouts", "[layout][layout_kind]")
{
  CHECK(layout::isFixedManagedLayoutKind(LayoutKind::FourUp));
  CHECK(layout::isFixedManagedLayoutKind(LayoutKind::ThreeUp));

  CHECK_FALSE(layout::isFixedManagedLayoutKind(LayoutKind::Custom));
  CHECK_FALSE(layout::isFixedManagedLayoutKind(LayoutKind::OneUp));
  CHECK_FALSE(layout::isFixedManagedLayoutKind(LayoutKind::MultiImageGrid));
  CHECK_FALSE(layout::isFixedManagedLayoutKind(LayoutKind::AxCorSagByImage));
  CHECK_FALSE(layout::isFixedManagedLayoutKind(LayoutKind::Lightbox));
  CHECK_FALSE(layout::isFixedManagedLayoutKind(LayoutKind::NumElements));
}

TEST_CASE("regular grid recipes are only used when they preserve layout view order", "[layout][layout_kind]")
{
  CHECK(layout::canUseRegularGridRecipe(LayoutKind::Custom));
  CHECK(layout::canUseRegularGridRecipe(LayoutKind::OneUp));
  CHECK(layout::canUseRegularGridRecipe(LayoutKind::MultiImageGrid));
  CHECK(layout::canUseRegularGridRecipe(LayoutKind::AxCorSagByImage));
  CHECK(layout::canUseRegularGridRecipe(LayoutKind::Lightbox));

  CHECK_FALSE(layout::canUseRegularGridRecipe(LayoutKind::FourUp));
  CHECK_FALSE(layout::canUseRegularGridRecipe(LayoutKind::ThreeUp));
  CHECK_FALSE(layout::canUseRegularGridRecipe(LayoutKind::NumElements));
}

TEST_CASE("lightbox layout kind follows supported slice view type", "[layout][layout_kind]")
{
  CHECK(layout::lightboxLayoutKindForViewType(ViewType::Axial) == LayoutKind::Lightbox);
  CHECK(layout::lightboxLayoutKindForViewType(ViewType::Coronal) == LayoutKind::Lightbox);
  CHECK(layout::lightboxLayoutKindForViewType(ViewType::Sagittal) == LayoutKind::Lightbox);
  CHECK(layout::lightboxLayoutKindForViewType(ViewType::Oblique) == LayoutKind::Custom);
  CHECK(layout::lightboxLayoutKindForViewType(ViewType::ThreeD) == LayoutKind::Custom);
  CHECK(layout::lightboxLayoutKindForViewType(ViewType::NumElements) == LayoutKind::Custom);
}
