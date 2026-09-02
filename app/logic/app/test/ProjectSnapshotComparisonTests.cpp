#include "logic/app/ProjectSnapshotComparison.h"

#include <catch2/catch_test_macros.hpp>

namespace
{
serialize::EntropyProject makeProject()
{
  serialize::EntropyProject project;
  project.m_referenceImage.m_imageFileName = "reference.nii.gz";
  project.m_referenceImage.m_annotationsFileName = "reference-annotations.json";
  project.m_referenceImage.m_settings = serialize::ImageSettings{};
  project.m_referenceImage.m_settings->m_displayName = "Reference";
  project.m_referenceImage.m_settings->m_showEdges = true;

  serialize::Segmentation segmentation;
  segmentation.m_segFileName = "segmentation.nrrd";
  segmentation.m_settings = serialize::SegSettings{};
  segmentation.m_settings->m_displayName = "Segmentation";
  segmentation.m_settings->m_opacity = 0.5;
  project.m_referenceImage.m_segmentations.push_back(segmentation);

  serialize::LandmarkGroup landmarks;
  landmarks.m_csvFileName = "landmarks.csv";
  landmarks.m_coordinateSpace = serialize::ProjectLandmarkCoordinateSpace::Voxel;
  landmarks.m_name = "Landmarks";
  landmarks.m_points.push_back(
    serialize::LandmarkPoint{.m_index = 0, .m_position = glm::vec3{1.0f, 2.0f, 3.0f}, .m_name = "AC"});
  project.m_referenceImage.m_landmarkGroups.push_back(landmarks);

  serialize::ImageIsosurface isosurface;
  isosurface.m_component = 0;
  isosurface.m_surface.name = "Surface";
  isosurface.m_surface.value = 12.0;
  project.m_referenceImage.m_isosurfaces.push_back(isosurface);

  serialize::Image additionalImage;
  additionalImage.m_imageFileName = "moving.nii.gz";
  additionalImage.m_manualAffineMatrix = glm::mat4{1.0f};
  additionalImage.m_manualAffineMatrix->operator[](3).x = 2.0f;
  project.m_additionalImages.push_back(additionalImage);

  project.m_layoutsFileName = "layouts.json";
  layout::LayoutSpec layout;
  layout.m_displayName = "3-Up";
  layout.m_views.push_back(layout::ViewSpec{});
  layout.m_views.front().m_viewType = 2;
  project.m_layouts.push_back(layout);
  project.m_currentLayoutIndex = 0;

  project.m_view.m_anatomicalLabelType = AnatomicalLabelType::Human;
  project.m_view.m_lockAnatomicalDirectionsToReferenceImage = false;
  project.m_view.m_crosshairsSnapping = CrosshairsSnapping::Disabled;

  return project;
}
} // namespace

TEST_CASE("Project snapshots compare equal when all serialized state matches", "[ProjectSnapshotComparison]")
{
  const serialize::EntropyProject project = makeProject();

  CHECK(project_snapshot::equivalent(project, project));
}

TEST_CASE("Project snapshot comparison detects image state changes", "[ProjectSnapshotComparison]")
{
  const serialize::EntropyProject project = makeProject();

  auto changedImagePath = project;
  changedImagePath.m_referenceImage.m_imageFileName = "other.nii.gz";
  CHECK_FALSE(project_snapshot::equivalent(project, changedImagePath));

  auto changedImageSettings = project;
  changedImageSettings.m_referenceImage.m_settings->m_showEdges = false;
  CHECK_FALSE(project_snapshot::equivalent(project, changedImageSettings));

  changedImageSettings = project;
  changedImageSettings.m_referenceImage.m_settings->m_lockedToReference = false;
  CHECK_FALSE(project_snapshot::equivalent(project, changedImageSettings));

  changedImageSettings = project;
  changedImageSettings.m_referenceImage.m_settings->m_warpStrength = 2.0f;
  CHECK_FALSE(project_snapshot::equivalent(project, changedImageSettings));

  auto changedTransform = project;
  changedTransform.m_additionalImages.front().m_manualAffineMatrix->operator[](3).x = 3.0f;
  CHECK_FALSE(project_snapshot::equivalent(project, changedTransform));

  auto changedForwardWarp = project;
  changedForwardWarp.m_additionalImages.front().m_forwardWarpFieldPath = "forward-warp.nrrd";
  CHECK_FALSE(project_snapshot::equivalent(project, changedForwardWarp));
}

TEST_CASE("Project snapshot comparison detects related data changes", "[ProjectSnapshotComparison]")
{
  const serialize::EntropyProject project = makeProject();

  auto changedSegmentation = project;
  changedSegmentation.m_referenceImage.m_segmentations.front().m_settings->m_opacity = 0.75;
  CHECK_FALSE(project_snapshot::equivalent(project, changedSegmentation));

  auto changedSegmentationLabels = project;
  changedSegmentationLabels.m_referenceImage.m_segmentations.front().m_settings->m_labels =
    serialize::SegmentationLabels{
      .m_count = 256,
      .m_values = {serialize::SegmentationLabel{
        .m_index = 3,
        .m_name = "Edited label",
        .m_color = glm::vec4{0.2f, 0.4f, 0.6f, 1.0f},
        .m_visible = true,
        .m_showMesh = false}}};
  CHECK_FALSE(project_snapshot::equivalent(project, changedSegmentationLabels));

  auto changedLandmarks = project;
  changedLandmarks.m_referenceImage.m_landmarkGroups.front().m_coordinateSpace =
    serialize::ProjectLandmarkCoordinateSpace::Subject;
  CHECK_FALSE(project_snapshot::equivalent(project, changedLandmarks));

  auto changedLandmarkPoint = project;
  changedLandmarkPoint.m_referenceImage.m_landmarkGroups.front().m_points.front().m_position.x = 9.0f;
  CHECK_FALSE(project_snapshot::equivalent(project, changedLandmarkPoint));

  auto changedIsosurface = project;
  changedIsosurface.m_referenceImage.m_isosurfaces.front().m_surface.opacity = 0.4f;
  CHECK_FALSE(project_snapshot::equivalent(project, changedIsosurface));

  auto changedIsosurfaceVisibility2d = project;
  changedIsosurfaceVisibility2d.m_referenceImage.m_isosurfaces.front().m_surface.visibleIn2d = false;
  CHECK_FALSE(project_snapshot::equivalent(project, changedIsosurfaceVisibility2d));

  auto changedIsosurfaceVisibility3d = project;
  changedIsosurfaceVisibility3d.m_referenceImage.m_isosurfaces.front().m_surface.visibleIn3d = false;
  CHECK_FALSE(project_snapshot::equivalent(project, changedIsosurfaceVisibility3d));
}

TEST_CASE("Project snapshot comparison detects layout and interface changes", "[ProjectSnapshotComparison]")
{
  const serialize::EntropyProject project = makeProject();

  auto changedExternalLayout = project;
  changedExternalLayout.m_layoutsFileName = "other-layouts.json";
  CHECK_FALSE(project_snapshot::equivalent(project, changedExternalLayout));

  auto changedLayout = project;
  changedLayout.m_layouts.front().m_views.front().m_viewType = 1;
  CHECK_FALSE(project_snapshot::equivalent(project, changedLayout));

  auto changedSynchronization = project;
  changedSynchronization.m_synchronization.m_synchronizeTimeSeries = false;
  CHECK_FALSE(project_snapshot::equivalent(project, changedSynchronization));

  auto changedView = project;
  changedView.m_view.m_anatomicalLabelType = AnatomicalLabelType::Rodent;
  CHECK_FALSE(project_snapshot::equivalent(project, changedView));

  changedView = project;
  changedView.m_view.m_showAnatomicalLabels = false;
  CHECK_FALSE(project_snapshot::equivalent(project, changedView));

  changedView = project;
  changedView.m_view.m_showAnatomicalLabelsInLightboxViews = false;
  CHECK_FALSE(project_snapshot::equivalent(project, changedView));

  changedView = project;
  changedView.m_view.m_crosshairsSnapping = CrosshairsSnapping::ActiveImage;
  CHECK_FALSE(project_snapshot::equivalent(project, changedView));

  changedView = project;
  changedView.m_view.m_showImageBorders = false;
  CHECK_FALSE(project_snapshot::equivalent(project, changedView));

  changedView = project;
  changedView.m_view.m_showImageBordersInLightboxViews = true;
  CHECK_FALSE(project_snapshot::equivalent(project, changedView));

  changedView = project;
  changedView.m_view.m_showCrosshairs = false;
  CHECK_FALSE(project_snapshot::equivalent(project, changedView));

  changedView = project;
  changedView.m_view.m_showCrosshairsInLightboxViews = false;
  CHECK_FALSE(project_snapshot::equivalent(project, changedView));

  changedView = project;
  changedView.m_view.m_showScaleBars = false;
  CHECK_FALSE(project_snapshot::equivalent(project, changedView));

  changedView = project;
  changedView.m_view.m_showScaleBars = true;
  changedView.m_view.m_showScaleBarsInLightboxViews = true;
  CHECK_FALSE(project_snapshot::equivalent(project, changedView));

  changedView = project;
  changedView.m_view.m_annotationsOnTop = true;
  CHECK_FALSE(project_snapshot::equivalent(project, changedView));

  changedView = project;
  changedView.m_view.m_landmarksOnTop = true;
  CHECK_FALSE(project_snapshot::equivalent(project, changedView));

  changedView = project;
  changedView.m_view.m_hideAnnotationVertices = true;
  CHECK_FALSE(project_snapshot::equivalent(project, changedView));

  auto changedComparison = project;
  changedComparison.m_comparison.m_checkerboardSquares = 17;
  CHECK_FALSE(project_snapshot::equivalent(project, changedComparison));

  changedComparison = project;
  changedComparison.m_comparison.m_localNcc.m_patchRadius = 5;
  CHECK_FALSE(project_snapshot::equivalent(project, changedComparison));

  auto changedRaycasting = project;
  changedRaycasting.m_raycasting.m_samplingFactor = 1.25f;
  CHECK_FALSE(project_snapshot::equivalent(project, changedRaycasting));

  changedRaycasting = project;
  changedRaycasting.m_raycasting.m_useDistanceMap = false;
  CHECK_FALSE(project_snapshot::equivalent(project, changedRaycasting));

  auto changedThreeDRendering = project;
  changedThreeDRendering.m_threeDRendering.m_transparentBackground = false;
  CHECK_FALSE(project_snapshot::equivalent(project, changedThreeDRendering));

  changedThreeDRendering = project;
  changedThreeDRendering.m_threeDRendering.m_showThreeDCameraFrustumIn2DViews = true;
  CHECK_FALSE(project_snapshot::equivalent(project, changedThreeDRendering));

  changedThreeDRendering = project;
  changedThreeDRendering.m_threeDRendering.m_imagePlaneSegmentationsVisible = false;
  CHECK_FALSE(project_snapshot::equivalent(project, changedThreeDRendering));

  auto changedMeshRendering = project;
  changedMeshRendering.m_meshRendering.m_pbrShadingEnabled = true;
  CHECK_FALSE(project_snapshot::equivalent(project, changedMeshRendering));

  changedMeshRendering = project;
  changedMeshRendering.m_meshRendering.m_smoothIsosurfaceMeshes = true;
  CHECK_FALSE(project_snapshot::equivalent(project, changedMeshRendering));

  changedMeshRendering = project;
  changedMeshRendering.m_meshRendering.m_shadowsEnabled = true;
  CHECK_FALSE(project_snapshot::equivalent(project, changedMeshRendering));

  changedMeshRendering = project;
  changedMeshRendering.m_meshRendering.m_ambientOcclusionEnabled = true;
  CHECK_FALSE(project_snapshot::equivalent(project, changedMeshRendering));

  changedMeshRendering = project;
  changedMeshRendering.m_meshRendering.m_ambientOcclusionPower = 2.0f;
  CHECK_FALSE(project_snapshot::equivalent(project, changedMeshRendering));

  changedMeshRendering = project;
  changedMeshRendering.m_meshRendering.m_ambientOcclusionContrast = 2.0f;
  CHECK_FALSE(project_snapshot::equivalent(project, changedMeshRendering));

  changedMeshRendering = project;
  changedMeshRendering.m_meshRendering.m_rimLightingEnabled = true;
  CHECK_FALSE(project_snapshot::equivalent(project, changedMeshRendering));

  changedMeshRendering = project;
  changedMeshRendering.m_meshRendering.m_ddpMaxPeelPasses = 12;
  CHECK_FALSE(project_snapshot::equivalent(project, changedMeshRendering));

  changedMeshRendering = project;
  changedMeshRendering.m_meshRendering.m_pickingEnabled = false;
  CHECK_FALSE(project_snapshot::equivalent(project, changedMeshRendering));

  changedMeshRendering = project;
  changedMeshRendering.m_meshRendering.m_clipPlaneEnabled = true;
  CHECK_FALSE(project_snapshot::equivalent(project, changedMeshRendering));

  changedMeshRendering = project;
  changedMeshRendering.m_meshRendering.m_clipPlaneWorld = {0.0f, 1.0f, 0.0f, -1.0f};
  CHECK_FALSE(project_snapshot::equivalent(project, changedMeshRendering));

  auto changedImageIsosurfaces = project;
  changedImageIsosurfaces.m_referenceImage.m_settings->m_modulateIsosurfaceOpacityWithImageOpacity = true;
  CHECK_FALSE(project_snapshot::equivalent(project, changedImageIsosurfaces));

  auto changedIntensityProjection = project;
  changedIntensityProjection.m_intensityProjection.m_slabThicknessMm = 12.0f;
  CHECK_FALSE(project_snapshot::equivalent(project, changedIntensityProjection));

  auto changedSegmentationDisplay = project;
  changedSegmentationDisplay.m_segmentationDisplay.m_outlineStyle = SegmentationOutlineStyle::ViewPixel;
  CHECK_FALSE(project_snapshot::equivalent(project, changedSegmentationDisplay));

  changedSegmentationDisplay = project;
  changedSegmentationDisplay.m_segmentationDisplay.m_modulateOpacityWithImageOpacity2d = false;
  CHECK_FALSE(project_snapshot::equivalent(project, changedSegmentationDisplay));

  changedSegmentationDisplay = project;
  changedSegmentationDisplay.m_segmentationDisplay.m_modulateOpacityWithImageOpacity3d = false;
  CHECK_FALSE(project_snapshot::equivalent(project, changedSegmentationDisplay));
}

TEST_CASE("Project snapshot comparison detects registration result changes", "[ProjectSnapshotComparison]")
{
  serialize::EntropyProject project = makeProject();
  project.m_registrationResults.push_back(serialize::RegistrationResult{
    .m_backend = "Greedy",
    .m_fixedImage = "fixed.nii.gz",
    .m_movingImage = "moving.nii.gz",
    .m_manifestFileName = "registration/result.json",
    .m_warpedImage = "registration/warped.nii.gz",
    .m_inverseWarpField = "registration/inverse.nrrd",
    .m_forwardWarpField = "registration/forward.nrrd"});

  CHECK(project_snapshot::equivalent(project, project));

  serialize::EntropyProject changedRegistration = project;
  changedRegistration.m_registrationResults.front().m_inverseWarpField = "registration/other-inverse.nrrd";
  CHECK_FALSE(project_snapshot::equivalent(project, changedRegistration));

  serialize::EntropyProject missingRegistration = project;
  missingRegistration.m_registrationResults.clear();
  CHECK_FALSE(project_snapshot::equivalent(project, missingRegistration));
}
