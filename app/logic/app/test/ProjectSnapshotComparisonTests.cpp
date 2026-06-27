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
  landmarks.m_inVoxelSpace = true;
  project.m_referenceImage.m_landmarkGroups.push_back(landmarks);

  serialize::Image additionalImage;
  additionalImage.m_imageFileName = "moving.nii.gz";
  additionalImage.m_worldDefTx = glm::mat4{1.0f};
  additionalImage.m_worldDefTx->operator[](3).x = 2.0f;
  project.m_additionalImages.push_back(additionalImage);

  project.m_layoutsFileName = "layouts.json";
  layout::LayoutSpec layout;
  layout.m_displayName = "3-Up";
  layout.m_views.push_back(layout::ViewSpec{});
  layout.m_views.front().m_viewType = 2;
  project.m_layouts.push_back(layout);
  project.m_currentLayoutIndex = 0;

  project.m_interface.m_showLayoutTabs = true;
  project.m_interface.m_layoutTabPlacement = serialize::ProjectLayoutTabPlacement::Top;
  project.m_interface.m_showGlobalTimeControls = true;
  project.m_interface.m_imageValuePrecision = 3;
  project.m_interface.m_coordsPrecision = 4;
  project.m_interface.m_txPrecision = 5;
  project.m_interface.m_percentilePrecision = 2;

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

  auto changedTransform = project;
  changedTransform.m_additionalImages.front().m_worldDefTx->operator[](3).x = 3.0f;
  CHECK_FALSE(project_snapshot::equivalent(project, changedTransform));
}

TEST_CASE("Project snapshot comparison detects related data changes", "[ProjectSnapshotComparison]")
{
  const serialize::EntropyProject project = makeProject();

  auto changedSegmentation = project;
  changedSegmentation.m_referenceImage.m_segmentations.front().m_settings->m_opacity = 0.75;
  CHECK_FALSE(project_snapshot::equivalent(project, changedSegmentation));

  auto changedLandmarks = project;
  changedLandmarks.m_referenceImage.m_landmarkGroups.front().m_inVoxelSpace = false;
  CHECK_FALSE(project_snapshot::equivalent(project, changedLandmarks));
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

  auto changedInterface = project;
  changedInterface.m_interface.m_layoutTabPlacement = serialize::ProjectLayoutTabPlacement::Bottom;
  CHECK_FALSE(project_snapshot::equivalent(project, changedInterface));

  changedInterface = project;
  changedInterface.m_interface.m_showGlobalTimeControls = false;
  CHECK_FALSE(project_snapshot::equivalent(project, changedInterface));
}
