#include "ui/headers/Headers.h"
#include "ui/GuiData.h"
#include "ui/headers/HeaderCommon.h"
#include "ui/DicomMetadataTable.h"
#include "ui/Helpers.h"
#include "ui/ImageExport.h"
#include "ui/ImGuiCustomControls.h"
#include "ui/NativeFileDialogs.h"
#include "ui/widgets/Widgets.h"
#include "ui/widgets/ImageHistogram.h"

// data::roundPointToNearestImageVoxelCenter
// data::getAnnotationSubjectPlaneName
#include "logic/app/DataHelper.h"

#include "image/DicomSeries.h"
#include "image/Image.h"
#include "image/ImageColorMap.h"
#include "image/ImageDerivedData.h"
#include "image/ImageHeader.h"
#include "image/ImageSettings.h"
#include "image/ImageTransformations.h"
#include "image/ImageUtility.h"

#include "logic/app/Data.h"
#include "logic/states/annotation/AnnotationStateMachine.h"

#include <IconsForkAwesome.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_inverse.hpp>
#include <glm/gtc/type_ptr.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/color_space.hpp>
#include <imgui-knobs.h>
#include <imgui/imgui.h>
#include <imgui/misc/cpp/imgui_stdlib.h>
#include <implot/implot.h>

#include <spdlog/fmt/ostr.h>
#include <spdlog/fmt/std.h>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#undef min
#undef max

namespace fs = std::filesystem;
using namespace ui::headers;

namespace
{
std::string lowerCaseCopy(std::string text)
{
  std::transform(text.begin(), text.end(), text.begin(), [](unsigned char c) {
    return static_cast<char>(std::tolower(c));
  });
  return text;
}

} // namespace

void renderImageHeaderInformation(
  AppData& appData,
  const uuids::uuid& imageUid,
  Image& image,
  const std::function<void(void)>& updateImageUniforms,
  const AllViewsRecenterType& recenterAllViews)
{
  const char* txFormat = appData.guiData().m_txPrecisionFormat.c_str();
  const char* coordFormat = appData.guiData().m_coordsPrecisionFormat.c_str();

  const ImageHeader& imgHeader = image.header();
  ImageTransformations& imgTx = image.transformations();

  // File name:
  ImGui::Spacing();
  std::string fileName = imgHeader.fileName().string();
  ImGui::InputText("File name", &fileName, ImGuiInputTextFlags_ReadOnly);
  ImGui::SameLine();
  helpMarker("Image file name");

  ImGui::Spacing();
  ImGui::Separator();
  ImGui::Spacing();

  // Pixel type:
  std::string pixelType = imgHeader.pixelTypeAsString();
  ImGui::InputText("Pixel type", &pixelType, ImGuiInputTextFlags_ReadOnly);

  // Number of components:
  uint32_t numComponentsPerPixel = imgHeader.numComponentsPerPixel();
  ImGui::InputScalar(
    "Components",
    ImGuiDataType_U32,
    &numComponentsPerPixel,
    nullptr,
    nullptr,
    nullptr,
    ImGuiInputTextFlags_ReadOnly);

  // Component type:
  std::string componentType = lowerCaseCopy(componentTypeString(imgHeader.fileComponentType()));
  ImGui::InputText("Component type", &componentType, ImGuiInputTextFlags_ReadOnly);

  if (isComplexValuedImage(image)) {
    std::string componentRoles = "0: real, 1: imaginary";
    ImGui::InputText("Component roles", &componentRoles, ImGuiInputTextFlags_ReadOnly);
    ImGui::SameLine();
    helpMarker("Complex image component interpretation");
  }

  // Image size (bytes):
  uint64_t fileSizeBytes = imgHeader.fileImageSizeInBytes();
  ImGui::InputScalar(
    "Size (bytes)",
    ImGuiDataType_U64,
    &fileSizeBytes,
    nullptr,
    nullptr,
    nullptr,
    ImGuiInputTextFlags_ReadOnly);

  // Image size (MiB):
  double fileSizeMiB = static_cast<double>(imgHeader.fileImageSizeInBytes()) / (1024.0 * 1024.0);
  ImGui::InputScalar(
    "Size (MiB)",
    ImGuiDataType_Double,
    &fileSizeMiB,
    nullptr,
    nullptr,
    nullptr,
    ImGuiInputTextFlags_ReadOnly);

  ImGui::Spacing();
  ImGui::Separator();
  ImGui::Spacing();

  if (image.isTimeSeries()) {
    uint32_t numTimePoints = image.timeAxis().numTimePoints();
    ImGui::InputScalar(
      "Time frames",
      ImGuiDataType_U32,
      &numTimePoints,
      nullptr,
      nullptr,
      nullptr,
      ImGuiInputTextFlags_ReadOnly);
    ImGui::SameLine();
    helpMarker("Number of frames along the image time axis");

    const std::string units = image.timeAxis().units();
    std::string timeUnits = units.empty() ? "frame" : units;
    ImGui::InputText("Time units", &timeUnits, ImGuiInputTextFlags_ReadOnly);
    ImGui::SameLine();
    helpMarker("Units used for time values");

    if (const auto spacingValue = image.timeAxis().spacing()) {
      double timeSpacing = *spacingValue;
      ImGui::InputScalar(
        "Time spacing",
        ImGuiDataType_Double,
        &timeSpacing,
        nullptr,
        nullptr,
        appData.guiData().m_timeValuePrecisionFormat.c_str(),
        ImGuiInputTextFlags_ReadOnly);
    }
    else {
      std::string timeSpacing = "variable";
      ImGui::InputText("Time spacing", &timeSpacing, ImGuiInputTextFlags_ReadOnly);
    }
    ImGui::SameLine();
    helpMarker("Time spacing between adjacent frames, or variable for irregularly sampled time axes");

    double timeRange[2]{
      image.timeAxis().value(0).value_or(0.0),
      image.timeAxis().value(numTimePoints - 1u).value_or(0.0)};
    ImGui::InputScalarN(
      "Time range",
      ImGuiDataType_Double,
      timeRange,
      2,
      nullptr,
      nullptr,
      appData.guiData().m_timeValuePrecisionFormat.c_str(),
      ImGuiInputTextFlags_ReadOnly);
    ImGui::SameLine();
    helpMarker("Time value range from the first frame to the final frame");

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
  }

  // Dimensions:
  glm::uvec3 dimensions = imgHeader.pixelDimensions();
  ImGui::InputScalarN(
    "Dimensions",
    ImGuiDataType_U32,
    glm::value_ptr(dimensions),
    3,
    nullptr,
    nullptr,
    nullptr,
    ImGuiInputTextFlags_ReadOnly);
  ImGui::SameLine();
  helpMarker("Matrix dimensions in voxels");

  // Spacing:
  glm::vec3 spacing = imgHeader.spacing();
  ImGui::InputScalarN(
    "Spacing (mm)",
    ImGuiDataType_Float,
    glm::value_ptr(spacing),
    3,
    nullptr,
    nullptr,
    coordFormat,
    ImGuiInputTextFlags_ReadOnly);
  ImGui::SameLine();
  helpMarker("Voxel spacing (mm)");

  // Origin:
  glm::vec3 origin = imgHeader.origin();
  ImGui::InputScalarN(
    "Origin (mm)",
    ImGuiDataType_Float,
    glm::value_ptr(origin),
    3,
    nullptr,
    nullptr,
    coordFormat,
    ImGuiInputTextFlags_ReadOnly);
  ImGui::SameLine();
  helpMarker("Image origin (mm): physical coordinates of voxel (0, 0, 0)");
  ImGui::Spacing();

  // Directions:
  glm::mat3 directions = imgHeader.directions();
  ImGui::Text("Voxel coordinate directions:");
  ImGui::SameLine();
  helpMarker(
    "Direction vectors in physical Subject space of the X, Y, Z image voxel axes. "
    "Also known as the voxel direction cosines matrix");

  ImGui::InputFloat3("x", glm::value_ptr(directions[0]), coordFormat, ImGuiInputTextFlags_ReadOnly);
  ImGui::InputFloat3("y", glm::value_ptr(directions[1]), coordFormat, ImGuiInputTextFlags_ReadOnly);
  ImGui::InputFloat3("z", glm::value_ptr(directions[2]), coordFormat, ImGuiInputTextFlags_ReadOnly);

  // Closest orientation code:
  std::string orientation = imgHeader.spiralCode();

  if (imgHeader.isOblique()) {
    orientation = "Closest to " + orientation + " (oblique)";
  }

  ImGui::InputText("Orientation", &orientation, ImGuiInputTextFlags_ReadOnly);
  ImGui::SameLine();
  helpMarker("Closest orientation 'SPIRAL' code (-x to +x: R to L; -y to +y: A to P; -z to +z: I to S");

  ImGui::Spacing();
  ImGui::Separator();
  ImGui::Spacing();

  if (const serialize::DicomSource* dicomSource = image_export::dicomSourceForImage(appData, imageUid)) {
    ImGui::Text("DICOM series source:");

    std::string dicomFolder = dicomSource->m_rootPath.string();
    ImGui::InputText("DICOM folder", &dicomFolder, ImGuiInputTextFlags_ReadOnly);

    std::string seriesUid = dicomSource->m_seriesInstanceUid;
    ImGui::InputText("Series UID", &seriesUid, ImGuiInputTextFlags_ReadOnly);

    std::string studyUid = dicomSource->m_studyInstanceUid;
    ImGui::InputText("Study UID", &studyUid, ImGuiInputTextFlags_ReadOnly);

    std::uint64_t sliceCount = static_cast<std::uint64_t>(dicomSource->m_files.size());
    ImGui::InputScalar(
      "Slices",
      ImGuiDataType_U64,
      &sliceCount,
      nullptr,
      nullptr,
      nullptr,
      ImGuiInputTextFlags_ReadOnly);

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
  }

  // Bounding box:
  ImGui::Text("Bounding box (in Subject space):");

  // Note: we used to display the min and max bounding box corners in Subject space.
  // However, this does not make sense if the Voxel-to-Subject transformation has a rotation.

  glm::vec3 boxCenter = imgHeader.subjectBBoxCenter();
  ImGui::InputScalarN(
    "Center (mm)",
    ImGuiDataType_Float,
    glm::value_ptr(boxCenter),
    3,
    nullptr,
    nullptr,
    coordFormat,
    ImGuiInputTextFlags_ReadOnly);
  ImGui::SameLine();
  helpMarker("Bounding box center in Subject space (mm)");

  glm::vec3 boxSize = imgHeader.subjectBBoxSize();
  ImGui::InputScalarN(
    "Size (mm)",
    ImGuiDataType_Float,
    glm::value_ptr(boxSize),
    3,
    nullptr,
    nullptr,
    coordFormat,
    ImGuiInputTextFlags_ReadOnly);
  ImGui::SameLine();
  helpMarker("Bounding box size (mm)");

  ImGui::Spacing();
  ImGui::Separator();
  ImGui::Spacing();

  // subject_T_voxels:
  ImGui::Text("Voxel-to-Subject transformation:");
  ImGui::SameLine();
  helpMarker("Transformation from Voxel indices to Subject (LPS) space");

  glm::mat4 s_T_p = glm::transpose(imgTx.subject_T_pixel());

  // ImGui::PushItemWidth(-1);
  ImGui::InputFloat4("##v2s_col0", glm::value_ptr(s_T_p[0]), txFormat, ImGuiInputTextFlags_ReadOnly);
  ImGui::InputFloat4("##v2s_col1", glm::value_ptr(s_T_p[1]), txFormat, ImGuiInputTextFlags_ReadOnly);
  ImGui::InputFloat4("##v2s_col2", glm::value_ptr(s_T_p[2]), txFormat, ImGuiInputTextFlags_ReadOnly);
  ImGui::InputFloat4("##v2s_col3", glm::value_ptr(s_T_p[3]), txFormat, ImGuiInputTextFlags_ReadOnly);
  // ImGui::PopItemWidth();

  ImGui::Spacing();

  auto applyOverrideToAllSegsOfImage = [&imageUid, &appData](const ImageHeaderOverrides& overrides) {
    for (const auto& segUid : appData.imageToSegUids(imageUid)) {
      if (Image* seg = appData.seg(segUid)) {
        seg->setHeaderOverrides(overrides);
      }
    }
  };

  if (Image::ImageRepresentation::Image == image.imageRep()) {
    static constexpr bool recenterCrosshairs = true;
    static constexpr bool realignCrosshairs = true;
    static constexpr bool recenterOnCurrentCrosshairsPosition = true;
    static constexpr bool doNotResetObliqueOrientation = false;
    static constexpr bool doNotResetZoom = false;

    ImGui::Separator();
    ImGui::Spacing();

    ImGui::Text("Forced override of image header parameters:");
    ImGui::SameLine();
    helpMarker(
      "These options override image header parameters that affect the Voxel-to-Subject "
      "transformation");

    ImageHeaderOverrides overrides = image.getHeaderOverrides();

    // Ignore spacing checkbox:
    if (ImGui::Checkbox("Set 1mm isotropic voxel spacing", &(overrides.m_useIdentityPixelSpacings))) {
      image.setHeaderOverrides(overrides);
      applyOverrideToAllSegsOfImage(overrides);
      updateImageUniforms();

      recenterAllViews(
        recenterCrosshairs,
        realignCrosshairs,
        recenterOnCurrentCrosshairsPosition,
        doNotResetObliqueOrientation,
        doNotResetZoom);
    }
    ImGui::SameLine();
    helpMarker("Ignore voxel spacing from header: force spacing to (1, 1, 1)mm");

    // Ignore origin checkbox:
    if (ImGui::Checkbox("Set zero voxel origin", &(overrides.m_useZeroPixelOrigin))) {
      image.setHeaderOverrides(overrides);
      applyOverrideToAllSegsOfImage(overrides);
      updateImageUniforms();

      recenterAllViews(
        recenterCrosshairs,
        realignCrosshairs,
        recenterOnCurrentCrosshairsPosition,
        doNotResetObliqueOrientation,
        doNotResetZoom);
    }
    ImGui::SameLine();
    helpMarker("Ignore image voxel origin from header: force origin to (0, 0, 0)mm");

    if (ImGui::Checkbox("Set identity cosine direction matrix", &(overrides.m_useIdentityPixelDirections))) {
      image.setHeaderOverrides(overrides);
      applyOverrideToAllSegsOfImage(overrides);
      updateImageUniforms();

      recenterAllViews(
        recenterCrosshairs,
        realignCrosshairs,
        recenterOnCurrentCrosshairsPosition,
        doNotResetObliqueOrientation,
        doNotResetZoom);
    }
    ImGui::SameLine();
    helpMarker("Ignore voxel directions from header: force direction cosines matrix to identity");

    // Snap to closest orthogonal directions checkbox:
    if (overrides.m_originalIsOblique && !overrides.m_useIdentityPixelDirections) {
      const std::string snapToText = "Set closest orthogonal direction matrix (" + imgHeader.spiralCode() + ")";

      if (ImGui::Checkbox(snapToText.c_str(), &(overrides.m_snapToClosestOrthogonalPixelDirections))) {
        image.setHeaderOverrides(overrides);
        applyOverrideToAllSegsOfImage(overrides);
        updateImageUniforms();
      }
      ImGui::SameLine();
      helpMarker("Snap to the closest orthogonal voxel direction cosines matrix");
    }

    ImGui::Spacing();
  }

  ImGui::Spacing();
}
