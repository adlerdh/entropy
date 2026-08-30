target_sources(entropy PRIVATE
  app/CallbackHandler.cpp
  app/Data.cpp
  app/DeformationWarp.cpp
  app/DataHelper.cpp
  app/ImageScaleInteraction.cpp
  app/ImageSelectionPolicy.cpp
  app/Logging.cpp
  app/LoadingStatusItems.cpp
  app/ParcellationLabelTable.cpp
  app/ProjectLayoutDelta.cpp
  app/ProjectSnapshotComparison.cpp
  app/ProjectSnapshotSettings.cpp
  app/Settings.cpp
  app/StackTrace.cpp
  app/State.cpp
  app/UserPreferences.cpp
  app/UserPreferencesRenderData.cpp
  app/WindowTitleStatus.cpp

  annotation/Annotation.cpp
  annotation/BezierHelper.cpp
  annotation/LandmarkGroup.cpp
  annotation/SerializeAnnot.cpp

  camera/Camera.cpp
  camera/Camera3DControls.cpp
  camera/CameraFrustumSlice.cpp
  camera/Camera3DInteraction.cpp
  camera/CameraHelpers.cpp
  camera/CameraTypes.cpp
  camera/MathUtility.cpp
  camera/OrthogonalProjection.cpp
  camera/PerspectiveProjection.cpp
  camera/Projection.cpp
  camera/RaycastIsoSurfacePicker.cpp

  interaction/ViewHit.cpp
  interaction/events/ButtonState.cpp

  segmentation/AnnotationSegmentation.cpp
  segmentation/Poisson.cpp
  segmentation/SegHelpers.cpp

  serialization/ProjectSerialization.cpp
  serialization/FileIo.cpp
  serialization/ImageRecords.cpp
  serialization/ImageSettings.cpp
  serialization/Isosurface.cpp
  serialization/LayoutSerialization.cpp
  serialization/ProjectIo.cpp

  states/annotation/AnnotationStateHelpers.cpp
  states/annotation/AnnotationStateMachine.cpp
  states/annotation/AnnotationStates.cpp

  DistanceMap.cpp
  SurfaceUtility.cpp
  sync/EntropyInstanceSync.cpp
  sync/EntropyInstanceSyncProtocol.cpp
  sync/ItkSnapSyncProtocol.cpp
  sync/ItkSnapSync.cpp
)

if(APPLE)
  target_sources(entropy PRIVATE app/AppPathsMac.mm)
else()
  target_sources(entropy PRIVATE app/AppPaths.cpp)
endif()

if(BUILD_TESTING)
  add_subdirectory(app/test)
  add_subdirectory(camera/test)
  add_subdirectory(interaction/test)
  add_subdirectory(serialization/test)
  add_subdirectory(sync/test)
endif()
