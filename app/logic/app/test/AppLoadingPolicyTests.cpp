#include "logic/app/LargeImagePolicy.h"
#include "logic/app/ProjectImageSequence.h"

#include "logic/serialization/ProjectSerialization.h"

#include <catch2/catch_test_macros.hpp>

TEST_CASE("large image confirmation begins at the documented memory threshold", "[app][loading]")
{
  CHECK_FALSE(large_image_policy::requiresConfirmation(large_image_policy::k_confirmationThresholdBytes - 1u));
  CHECK(large_image_policy::requiresConfirmation(large_image_policy::k_confirmationThresholdBytes));
  CHECK(large_image_policy::requiresConfirmation(large_image_policy::k_confirmationThresholdBytes + 1u));
}

TEST_CASE("project image sequence presents reference and additional images in load order", "[app][loading]")
{
  serialize::EntropyProject project;
  project.m_referenceImage.m_imageFileName = "reference.nii.gz";
  project.m_additionalImages.resize(2u);
  project.m_additionalImages[0].m_imageFileName = "first.nii.gz";
  project.m_additionalImages[1].m_imageFileName = "second.nii.gz";

  REQUIRE(project_image_sequence::size(project) == 3u);
  REQUIRE(project_image_sequence::at(project, 0u));
  REQUIRE(project_image_sequence::at(project, 1u));
  REQUIRE(project_image_sequence::at(project, 2u));
  CHECK(project_image_sequence::at(project, 0u)->m_imageFileName == "reference.nii.gz");
  CHECK(project_image_sequence::at(project, 1u)->m_imageFileName == "first.nii.gz");
  CHECK(project_image_sequence::at(project, 2u)->m_imageFileName == "second.nii.gz");
  CHECK(project_image_sequence::at(project, 3u) == nullptr);

  const serialize::EntropyProject& constProject = project;
  CHECK(project_image_sequence::at(constProject, 1u)->m_imageFileName == "first.nii.gz");
}

TEST_CASE("project image sequence erases only additional images", "[app][loading]")
{
  serialize::EntropyProject project;
  project.m_referenceImage.m_imageFileName = "reference.nii.gz";
  project.m_additionalImages.resize(2u);
  project.m_additionalImages[0].m_imageFileName = "first.nii.gz";
  project.m_additionalImages[1].m_imageFileName = "second.nii.gz";

  CHECK_FALSE(project_image_sequence::erase(project, 0u));
  CHECK_FALSE(project_image_sequence::erase(project, 3u));
  CHECK(project_image_sequence::erase(project, 1u));
  REQUIRE(project_image_sequence::size(project) == 2u);
  CHECK(project_image_sequence::at(project, 1u)->m_imageFileName == "second.nii.gz");
}
