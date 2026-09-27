#include "logic/app/WarpInversionRequest.h"
#include "logic/app/Data.h"

bool warp_inversion::canPublish(const AppData& data, const RequestState& state, bool latestRequest)
{
  const Image* source = data.warpField(state.sourceWarpUid);
  const Image* image = data.image(state.imageUid);
  const Image* domain = data.image(state.domainUid);
  const bool inverse = state.direction == ComputedWarpDirection::Inverse;
  const auto activeSource =
    inverse ? data.imageToActiveForwardWarpUid(state.imageUid) : data.imageToActiveInverseWarpUid(state.imageUid);
  const auto activeTarget =
    inverse ? data.imageToActiveInverseWarpUid(state.imageUid) : data.imageToActiveForwardWarpUid(state.imageUid);
  return source && image && domain && latestRequest && (!state.cancel || !state.cancel->load()) &&
         activeSource == state.sourceWarpUid && activeTarget == state.targetWarpUid &&
         source->pixelDataRevision() == state.sourcePixelRevision &&
         source->geometryRevision() == state.sourceGeometryRevision &&
         source->settings().activeTimePoint() == state.sourceTimePoint &&
         source->transformations().worldDef_T_subject() == state.sourceTransform &&
         image->transformations().worldDef_T_subject() == state.imageTransform &&
         image->geometryRevision() == state.imageGeometryRevision &&
         domain->geometryRevision() == state.domainGeometryRevision && data.refImageUid() == state.referenceUid;
}
