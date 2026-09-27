#include "logic/app/ProjectSnapshotSettings.h"
#include "image/Image.h"
#include "logic/app/Data.h"
#include <spdlog/spdlog.h>
#include <spdlog/fmt/std.h>
#include <algorithm>

void project_snapshot::restoreSegmentationState(
  AppData& data,
  const uuids::uuid& imageUid,
  const uuids::uuid& segUid,
  const serialize::Image& record,
  bool applySettings)
{
  Image* seg = data.seg(segUid);
  if (!seg) return;
  const auto saved = std::ranges::find_if(record.m_segmentations, [seg](const auto& candidate) {
    return candidate.m_segFileName == seg->header().fileName();
  });
  if (saved == record.m_segmentations.end()) return;
  if (saved->m_active) data.assignActiveSegUidToImage(imageUid, segUid);
  if (applySettings && saved->m_settings) applySegmentationSettings(data, *seg, *saved->m_settings);
}

void project_snapshot::restoreImageState(
  Image& target,
  const serialize::Image& serializedImage,
  bool isReferenceImage,
  const ImageLoadFailure& reportFailure)
{
  Image* image = &target;
  if (serializedImage.m_settings) {
    project_snapshot::applyImageSettings(*image, *serializedImage.m_settings);
  }

  auto headerOverrides = image->header().getHeaderOverrides();
  headerOverrides.m_useIdentityPixelSpacings = serializedImage.m_useIdentityPixelSpacings;
  headerOverrides.m_useZeroPixelOrigin = serializedImage.m_useZeroPixelOrigin;
  headerOverrides.m_useIdentityPixelDirections = serializedImage.m_useIdentityPixelDirections;
  headerOverrides.m_snapToClosestOrthogonalPixelDirections = serializedImage.m_snapToClosestOrthogonalPixelDirections;
  image->setHeaderOverrides(headerOverrides);

  // Disable the initial affine and manual transformations for the reference image:
  image->transformations().set_enable_worldDef_T_affine(!isReferenceImage);
  image->transformations().set_enable_affine_T_subject(!isReferenceImage);

  // Lock all affine transformations to the reference image, which defines the World space:
  image->transformations().set_worldDef_T_affine_locked(true);

  // Load and set the initial/imported affine transformation for non-reference images.
  if (serializedImage.m_initialAffineMatrix || serializedImage.m_initialAffineFileName) {
    glm::dmat4 affine_T_subject(1.0);

    if (isReferenceImage) {
      if (serializedImage.m_initialAffineFileName) {
        spdlog::warn(
          "An affine transformation file ({}) was provided for the reference image. "
          "It will be ignored, since the reference image defines the World coordinate "
          "space, which cannot be transformed.",
          *serializedImage.m_initialAffineFileName);
      }
      else if (serializedImage.m_initialAffineMatrix) {
        // Retain disabled values so future reference changes do not discard project state.
        image->transformations().set_affine_T_subject(*serializedImage.m_initialAffineMatrix);
      }

      image->transformations().set_affine_T_subject_fileName(std::nullopt);
    }
    else {
      if (serializedImage.m_initialAffineMatrix) {
        affine_T_subject = glm::dmat4{*serializedImage.m_initialAffineMatrix};
        image->transformations().set_affine_T_subject_fileName(std::nullopt);
      }
      else if (serializedImage.m_initialAffineFileName) {
        if (!serialize::openAffineTxFile(affine_T_subject, *serializedImage.m_initialAffineFileName)) {
          spdlog::error(
            "Unable to read affine transformation from {} for image {}",
            serializedImage.m_initialAffineFileName,
            image->header().fileName());
          reportFailure(
            "affine transformation",
            serializedImage.m_initialAffineFileName,
            "The transformation file could not be read or parsed.");

          image->transformations().set_affine_T_subject_fileName(std::nullopt);
        }
        else {
          image->transformations().set_affine_T_subject_fileName(serializedImage.m_initialAffineFileName);
        }
      }

      image->transformations().set_affine_T_subject(glm::mat4{affine_T_subject});
      image->transformations().set_enable_affine_T_subject(serializedImage.m_initialAffineEnabled);
    }
  }
  else {
    // No affine transformation provided:
    image->transformations().set_affine_T_subject_fileName(std::nullopt);
  }
  if (!isReferenceImage) {
    image->transformations().set_enable_affine_T_subject(serializedImage.m_initialAffineEnabled);
  }

  if (serializedImage.m_manualAffineMatrix || serializedImage.m_manualAffineFileName) {
    if (isReferenceImage) {
      if (serializedImage.m_manualAffineMatrix) {
        image->transformations().set_worldDef_T_affine_locked(false);
        image->transformations().set_worldDef_T_affine(*serializedImage.m_manualAffineMatrix);
        image->transformations().set_worldDef_T_affine_locked(true);
      }
    }
    else {
      glm::dmat4 worldDef_T_affine(1.0);
      bool manualAffineAvailable = true;
      if (serializedImage.m_manualAffineMatrix) {
        worldDef_T_affine = glm::dmat4{*serializedImage.m_manualAffineMatrix};
      }
      else if (
        serializedImage.m_manualAffineFileName &&
        !serialize::openAffineTxFile(worldDef_T_affine, *serializedImage.m_manualAffineFileName))
      {
        spdlog::error(
          "Unable to read manual affine transformation from {} for image {}",
          serializedImage.m_manualAffineFileName,
          image->header().fileName());
        reportFailure(
          "affine transformation",
          serializedImage.m_manualAffineFileName,
          "The transformation file could not be read or parsed.");
        manualAffineAvailable = false;
      }

      if (manualAffineAvailable) {
        image->transformations().set_worldDef_T_affine_locked(false);
        image->transformations().set_enable_worldDef_T_affine(serializedImage.m_manualAffineEnabled);
        image->transformations().set_worldDef_T_affine(glm::mat4{worldDef_T_affine});
        image->transformations().set_worldDef_T_affine_locked(true);
      }
    }
  }
  if (!isReferenceImage) {
    image->transformations().set_worldDef_T_affine_locked(false);
    image->transformations().set_enable_worldDef_T_affine(serializedImage.m_manualAffineEnabled);
    image->transformations().set_worldDef_T_affine_locked(true);
  }
}
