#include "windowing/ViewCameraDefaults.h"

namespace windowing
{

ViewType initialSliceViewType(ViewType viewType) noexcept
{
  return (ViewType::ThreeD == viewType) ? ViewType::Axial : viewType;
}

ProjectionType initialSliceProjectionType(ViewType viewType) noexcept
{
  switch (initialSliceViewType(viewType)) {
    case ViewType::Axial:
    case ViewType::Coronal:
    case ViewType::Sagittal:
    case ViewType::Oblique:
    case ViewType::ThreeD:
    case ViewType::NumElements:
      return ProjectionType::Orthographic;
  }

  return ProjectionType::Orthographic;
}

bool sliceCameraTracksCrosshairs(ViewType viewType) noexcept
{
  switch (viewType) {
    case ViewType::Axial:
    case ViewType::Coronal:
    case ViewType::Sagittal:
      return true;
    case ViewType::Oblique:
    case ViewType::ThreeD:
    case ViewType::NumElements:
      return false;
  }

  return false;
}

} // namespace windowing
