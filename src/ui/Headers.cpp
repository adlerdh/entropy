#include "ui/Headers.h"
#include "ui/GuiData.h"
#include "ui/Helpers.h"
#include "ui/ImGuiCustomControls.h"
#include "ui/Widgets.h"
#include "ui/Widgets.tpp"

// data::roundPointToNearestImageVoxelCenter
// data::getAnnotationSubjectPlaneName
#include "common/DataHelper.h"

#include "image/Image.h"
#include "image/ImageColorMap.h"
#include "image/ImageHeader.h"
#include "image/ImageSettings.h"
#include "image/ImageTransformations.h"
#include "image/ImageUtility.h"

#include "logic/app/Data.h"
#include "logic/states/AnnotationStateMachine.h"

#include <IconFontCppHeaders/IconsForkAwesome.h>

#include <imgui/imgui.h>
#include <imgui/misc/cpp/imgui_stdlib.h>

#include <implot.h>

#include <ui/imgui/imgui-knobs/imgui-knobs.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_inverse.hpp>
#include <glm/gtc/type_ptr.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/color_space.hpp>

#include <spdlog/fmt/ostr.h>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <string>

#undef min
#undef max

namespace
{

static const ImVec4 sk_whiteText(1, 1, 1, 1);
static const ImVec4 sk_blackText(0, 0, 0, 1);

/// Size of small toolbar buttons (pixels)
ImVec2 scaledToolbarButtonSize(const glm::vec2& contentScale)
{
  static const ImVec2 sk_smallToolbarButtonSize(24, 24);
  return ImVec2{
    contentScale.x * sk_smallToolbarButtonSize.x, contentScale.y * sk_smallToolbarButtonSize.y
  };
}

const char* sk_referenceAndActiveImageMessage = "This is the reference and active image";
const char* sk_referenceImageMessage = "This is the reference image";
const char* sk_activeImageMessage = "This is the active image";
const char* sk_nonActiveImageMessage = "This is not the active image";

static const ImGuiColorEditFlags sk_colorEditFlags = ImGuiColorEditFlags_NoInputs
                                                     | ImGuiColorEditFlags_PickerHueBar
                                                     | ImGuiColorEditFlags_DisplayRGB
                                                     | ImGuiColorEditFlags_DisplayHSV
                                                     | ImGuiColorEditFlags_DisplayHex
                                                     | ImGuiColorEditFlags_Uint8
                                                     | ImGuiColorEditFlags_InputRGB;

std::pair<ImVec4, ImVec4> computeHeaderBgAndTextColors(const glm::vec3& color)
{
  glm::vec3 darkerBorderColorHsv = glm::hsvColor(color);
  darkerBorderColorHsv[2] = std::max(0.5f * darkerBorderColorHsv[2], 0.0f);
  const glm::vec3 darkerBorderColorRgb = glm::rgbColor(darkerBorderColorHsv);

  const ImVec4
    headerColor(darkerBorderColorRgb.r, darkerBorderColorRgb.g, darkerBorderColorRgb.b, 1.0f);
  const ImVec4 headerTextColor = (glm::luminosity(darkerBorderColorRgb) < 0.75f) ? sk_whiteText
                                                                                 : sk_blackText;

  return {headerColor, headerTextColor};
}

} // namespace

void renderImageHeaderInformation(
  AppData& appData,
  const uuids::uuid& imageUid,
  Image& image,
  const std::function<void(void)>& updateImageUniforms,
  const AllViewsRecenterType& recenterAllViews
)
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

  // Dimensions:
  glm::uvec3 dimensions = imgHeader.pixelDimensions();
  ImGui::InputScalarN(
    "Dimensions (vox)",
    ImGuiDataType_U32,
    glm::value_ptr(dimensions),
    3,
    nullptr,
    nullptr,
    nullptr,
    ImGuiInputTextFlags_ReadOnly
  );
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
    "%0.6f",
    ImGuiInputTextFlags_ReadOnly
  );
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
    ImGuiInputTextFlags_ReadOnly
  );
  ImGui::SameLine();
  helpMarker("Image origin (mm): physical coordinates of voxel (0, 0, 0)");
  ImGui::Spacing();

  // Directions:
  glm::mat3 directions = imgHeader.directions();
  ImGui::Text("Voxel coordinate directions:");
  ImGui::SameLine();
  helpMarker("Direction vectors in physical Subject space of the X, Y, Z image voxel axes. "
             "Also known as the voxel direction cosines matrix.");

  ImGui::InputFloat3("X", glm::value_ptr(directions[0]), coordFormat, ImGuiInputTextFlags_ReadOnly);
  ImGui::InputFloat3("Y", glm::value_ptr(directions[1]), coordFormat, ImGuiInputTextFlags_ReadOnly);
  ImGui::InputFloat3("Z", glm::value_ptr(directions[2]), coordFormat, ImGuiInputTextFlags_ReadOnly);

  // Closest orientation code:
  std::string orientation = imgHeader.spiralCode();

  if (imgHeader.isOblique())
  {
    orientation = "Closest to " + orientation + " (oblique)";
  }

  ImGui::InputText("Orientation", &orientation, ImGuiInputTextFlags_ReadOnly);
  ImGui::SameLine();
  helpMarker(
    "Closest orientation 'SPIRAL' code (-x to +x: R to L; -y to +y: A to P; -z to +z: I to S"
  );

  ImGui::Spacing();
  ImGui::Separator();
  ImGui::Spacing();

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
    ImGuiInputTextFlags_ReadOnly
  );
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
    ImGuiInputTextFlags_ReadOnly
  );
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

  auto applyOverrideToAllSegsOfImage = [&imageUid, &appData](const ImageHeaderOverrides& overrides)
  {
    for (const auto& segUid : appData.imageToSegUids(imageUid))
    {
      if (Image* seg = appData.seg(segUid))
      {
        seg->setHeaderOverrides(overrides);
      }
    }
  };

  if (Image::ImageRepresentation::Image == image.imageRep())
  {
    static constexpr bool sk_recenterCrosshairs = true;
    static constexpr bool sk_recenterOnCurrentCrosshairsPosition = true;
    static constexpr bool sk_doNotResetObliqueOrientation = false;
    static constexpr bool sk_doNotResetZoom = false;

    ImGui::Separator();
    ImGui::Spacing();

    ImGui::Text("Forced override of image header parameters:");
    ImGui::SameLine();
    helpMarker("These options override image header parameters that affect the Voxel-to-Subject "
               "transformation");

    ImageHeaderOverrides overrides = image.getHeaderOverrides();

    // Ignore spacing checkbox:
    if (ImGui::Checkbox("Set 1mm isotropic voxel spacing", &(overrides.m_useIdentityPixelSpacings)))
    {
      image.setHeaderOverrides(overrides);
      applyOverrideToAllSegsOfImage(overrides);
      updateImageUniforms();

      recenterAllViews(
        sk_recenterCrosshairs,
        sk_recenterOnCurrentCrosshairsPosition,
        sk_doNotResetObliqueOrientation,
        sk_doNotResetZoom
      );
    }
    ImGui::SameLine();
    helpMarker("Ignore voxel spacing from header: force spacing to (1, 1, 1)mm");

    // Ignore origin checkbox:
    if (ImGui::Checkbox("Set zero voxel origin", &(overrides.m_useZeroPixelOrigin)))
    {
      image.setHeaderOverrides(overrides);
      applyOverrideToAllSegsOfImage(overrides);
      updateImageUniforms();

      recenterAllViews(
        sk_recenterCrosshairs,
        sk_recenterOnCurrentCrosshairsPosition,
        sk_doNotResetObliqueOrientation,
        sk_doNotResetZoom
      );
    }
    ImGui::SameLine();
    helpMarker("Ignore image voxel origin from header: force origin to (0, 0, 0)mm");

    if (ImGui::Checkbox(
          "Set identity cosine direction matrix", &(overrides.m_useIdentityPixelDirections)
        ))
    {
      image.setHeaderOverrides(overrides);
      applyOverrideToAllSegsOfImage(overrides);
      updateImageUniforms();

      recenterAllViews(
        sk_recenterCrosshairs,
        sk_recenterOnCurrentCrosshairsPosition,
        sk_doNotResetObliqueOrientation,
        sk_doNotResetZoom
      );
    }
    ImGui::SameLine();
    helpMarker("Ignore voxel directions from header: force direction cosines matrix to identity");

    // Snap to closest orthogonal directions checkbox:
    if (overrides.m_originalIsOblique && !overrides.m_useIdentityPixelDirections)
    {
      const std::string snapToText = "Set closest orthogonal direction matrix ("
                                     + imgHeader.spiralCode() + ")";

      if (ImGui::Checkbox(snapToText.c_str(), &(overrides.m_snapToClosestOrthogonalPixelDirections)))
      {
        image.setHeaderOverrides(overrides);
        applyOverrideToAllSegsOfImage(overrides);
        updateImageUniforms();
      }
      ImGui::SameLine();
      helpMarker("Snap to the closest orthogonal voxel direction cosines matrix");
    }

    ImGui::Spacing();
  }

  ImGui::Separator();
  ImGui::Spacing();

  // Pixel type:
  std::string pixelType = imgHeader.pixelTypeAsString();
  ImGui::InputText("Pixel type", &pixelType, ImGuiInputTextFlags_ReadOnly);
  ImGui::SameLine();
  helpMarker("Image pixel type");

  // Number of components:
  uint32_t numComponentsPerPixel = imgHeader.numComponentsPerPixel();
  ImGui::InputScalar(
    "Num. components",
    ImGuiDataType_U32,
    &numComponentsPerPixel,
    nullptr,
    nullptr,
    nullptr,
    ImGuiInputTextFlags_ReadOnly
  );
  ImGui::SameLine();
  helpMarker("Number of components per pixel");

  // Component type:
  std::string componentType = imgHeader.fileComponentTypeAsString();
  ImGui::InputText("Component type", &componentType, ImGuiInputTextFlags_ReadOnly);
  ImGui::SameLine();
  helpMarker("Image component type");

  // Image size (bytes):
  uint64_t fileSizeBytes = imgHeader.fileImageSizeInBytes();
  ImGui::InputScalar(
    "Size (bytes)",
    ImGuiDataType_U64,
    &fileSizeBytes,
    nullptr,
    nullptr,
    nullptr,
    ImGuiInputTextFlags_ReadOnly
  );
  ImGui::SameLine();
  helpMarker("Image size in bytes");

  // Image size (MiB):
  double fileSizeMiB = static_cast<double>(imgHeader.fileImageSizeInBytes()) / (1024.0 * 1024.0);
  ImGui::InputScalar(
    "Size (MiB)",
    ImGuiDataType_Double,
    &fileSizeMiB,
    nullptr,
    nullptr,
    nullptr,
    ImGuiInputTextFlags_ReadOnly
  );
  ImGui::SameLine();
  helpMarker("Image size in mebibytes (MiB)");
  ImGui::Spacing();
}

void renderImageHeader(
  AppData& appData,
  GuiData& guiData,
  const uuids::uuid& imageUid,
  size_t imageIndex,
  Image* image,
  bool isActiveImage,
  size_t numImages,
  const std::function<void(void)>& updateAllImageUniforms,
  const std::function<void(void)>& updateImageUniforms,
  const std::function<void(void)>& updateImageInterpolationMode,
  const std::function<void(std::size_t cmapIndex)>& /*updateImageColorMapInterpolationMode*/,
  const std::function<size_t(void)>& getNumImageColorMaps,
  const std::function<ImageColorMap*(size_t cmapIndex)>& getImageColorMap,
  const std::function<bool(const uuids::uuid& imageUid)>& moveImageBackward,
  const std::function<bool(const uuids::uuid& imageUid)>& moveImageForward,
  const std::function<bool(const uuids::uuid& imageUid)>& moveImageToBack,
  const std::function<bool(const uuids::uuid& imageUid)>& moveImageToFront,
  const std::function<bool(const uuids::uuid& imageUid, bool locked)>&
    setLockManualImageTransformation,
  const AllViewsRecenterType& recenterAllViews
)
{
  static const ImGuiColorEditFlags sk_colorNoAlphaEditFlags = ImGuiColorEditFlags_NoInputs
                                                              | ImGuiColorEditFlags_PickerHueBar
                                                              | ImGuiColorEditFlags_DisplayRGB
                                                              | ImGuiColorEditFlags_DisplayHex
                                                              | ImGuiColorEditFlags_Uint8
                                                              | ImGuiColorEditFlags_InputRGB;

  static const ImGuiColorEditFlags sk_colorAlphaEditFlags = ImGuiColorEditFlags_PickerHueBar
                                                            | ImGuiColorEditFlags_DisplayRGB
                                                            | ImGuiColorEditFlags_DisplayHex
                                                            | ImGuiColorEditFlags_AlphaBar
                                                            | ImGuiColorEditFlags_AlphaPreviewHalf
                                                            | ImGuiColorEditFlags_Uint8
                                                            | ImGuiColorEditFlags_InputRGB;

  const auto buttonSize = scaledToolbarButtonSize(appData.windowData().getContentScaleRatios());

  const ImVec4* colors = ImGui::GetStyle().Colors;
  const ImVec4 activeColor = colors[ImGuiCol_ButtonActive];
  ImVec4 inactiveColor = colors[ImGuiCol_Button];

  const std::string minValuesFormatString = std::string("Min: ")
                                            + appData.guiData().m_imageValuePrecisionFormat;
  const std::string maxValuesFormatString = std::string("Max: ")
                                            + appData.guiData().m_imageValuePrecisionFormat;

  const char* minValuesFormat = minValuesFormatString.c_str();
  const char* maxValuesFormat = maxValuesFormatString.c_str();

  const std::string minPercentileFormatString = std::string("Min: ")
                                                + appData.guiData().m_percentilePrecisionFormat
                                                + "%%";
  const std::string maxPercentileFormatString = std::string("Max: ")
                                                + appData.guiData().m_percentilePrecisionFormat
                                                + "%%";

  const char* minPercentilesFormat = minPercentileFormatString.c_str();
  const char* maxPercentilesFormat = maxPercentileFormatString.c_str();

  const char* valuesFormat = appData.guiData().m_imageValuePrecisionFormat.c_str();
  const char* txFormat = appData.guiData().m_txPrecisionFormat.c_str();

  const float windowPercentileStep = std::pow(10.0f, -1.0f * appData.guiData().m_percentilePrecision);

  /// @todo ADD visibility control for gamma
  if (!image)
    return;

  const auto& imgHeader = image->header();
  auto& imgSettings = image->settings();
  auto& imgTx = image->transformations();

  auto activeSegUid = appData.imageToActiveSegUid(imageUid);
  Image* activeSeg = (activeSegUid) ? appData.seg(*activeSegUid) : nullptr;

  auto getCurrentImageColormapIndex = [&imgSettings]() { return imgSettings.colorMapIndex(); };

  auto setCurrentImageColormapIndex = [&imgSettings](size_t cmapIndex)
  { imgSettings.setColorMapIndex(cmapIndex); };

  ImGuiTreeNodeFlags headerFlags = ImGuiTreeNodeFlags_CollapsingHeader;

  if (isActiveImage)
  {
    headerFlags |= ImGuiTreeNodeFlags_DefaultOpen;
  }

  ImGui::PushID(uuids::to_string(imageUid).c_str());

  // Header is ID'ed only by the image index.
  // ### allows the header name to change without changing its ID.

  /// @todo Provide a function shortenedDisplayName that takes an argument indicating
  /// the max number N of characters. It removes the last characters of the name, such that the
  /// total length is N.
  const std::string headerName = std::to_string(imageIndex) + ") " + imgSettings.displayName()
                                 + "###" + std::to_string(imageIndex);

  const auto headerColors = computeHeaderBgAndTextColors(imgSettings.borderColor());
  ImGui::PushStyleColor(ImGuiCol_Header, headerColors.first);
  ImGui::PushStyleColor(ImGuiCol_Text, headerColors.second);

  const bool clicked = ImGui::CollapsingHeader(headerName.c_str(), headerFlags);

  ImGui::PopStyleColor(2); // ImGuiCol_Header, ImGuiCol_Text

  if (!clicked)
  {
    ImGui::PopID(); // imageUid
    return;
  }

  ImGui::Spacing();

  // Border color:
  glm::vec3 borderColor{imgSettings.borderColor()};

  if (ImGui::ColorEdit3("##BorderColor", glm::value_ptr(borderColor), sk_colorNoAlphaEditFlags))
  {
    imgSettings.setBorderColor(borderColor);
    imgSettings.setEdgeColor(borderColor); // Set edge color to border color
    updateImageUniforms();
  }
  //    ImGui::SameLine(); helpMarker("Image border color");

  // Display name text:
  std::string displayName = imgSettings.displayName();
  ImGui::SameLine();

  if (ImGui::InputText("Name", &displayName))
  {
    imgSettings.setDisplayName(displayName);
  }
  ImGui::SameLine();
  helpMarker("Set the image display name and border color");

  if (ImGui::Button(ICON_FK_HAND_O_UP, buttonSize))
  {
    glm::vec3 worldPos{imgTx.worldDef_T_subject() * glm::vec4{imgHeader.subjectBBoxCenter(), 1.0f}};

    worldPos = data::snapWorldPointToImageVoxels(appData, worldPos);
    appData.state().setWorldCrosshairsPos(worldPos);
  }

  if (ImGui::IsItemHovered())
  {
    ImGui::SetTooltip("Move crosshairs to the center of the image");
  }

  ImGui::SameLine();
  ImGui::Text("Go to image center");

  if (!isActiveImage)
  {
    if (ImGui::Button(ICON_FK_TOGGLE_OFF))
    {
      if (appData.setActiveImageUid(imageUid))
        return;
    }

    if (ImGui::IsItemHovered())
    {
      ImGui::SetTooltip("Make this the active image");
    }
  }
  else
  {
    ImGui::PushStyleColor(ImGuiCol_Button, activeColor);
    ImGui::Button(ICON_FK_TOGGLE_ON);
    ImGui::PopStyleColor(1); // ImGuiCol_Button

    if (ImGui::IsItemHovered())
    {
      ImGui::SetTooltip("This is the active image");
    }
  }

  ImGui::SameLine();

  const bool isRef = (0 == imageIndex);

  if (isRef && isActiveImage)
  {
    ImGui::Text("%s", sk_referenceAndActiveImageMessage);
  }
  else if (isRef)
  {
    ImGui::Text("%s", sk_referenceImageMessage);
  }
  else if (isActiveImage)
  {
    ImGui::Text("%s", sk_activeImageMessage);
  }
  else
  {
    ImGui::Text("%s", sk_nonActiveImageMessage);
  }

  if (isActiveImage)
  {
    // We force the reference image transformation to always be locked. The reference image
    // cannot be transformed, since it defines the reference space.
    const bool forceLocked = (isRef);
    const bool isLocked = (forceLocked || image->transformations().is_worldDef_T_affine_locked());

    ImGui::PushStyleColor(ImGuiCol_Button, (isLocked ? inactiveColor : activeColor));
    if (ImGui::Button((isLocked ? ICON_FK_LOCK : ICON_FK_UNLOCK), buttonSize))
    {
      if (!forceLocked)
      {
        setLockManualImageTransformation(imageUid, !isLocked);
      }
    }
    ImGui::PopStyleColor(1); // ImGuiCol_Button

    if (image->transformations().is_worldDef_T_affine_locked())
    {
      if (ImGui::IsItemHovered())
      {
        if (forceLocked)
        {
          ImGui::SetTooltip("Manual image transformation is always locked for the reference image.");
        }
        else
        {
          ImGui::SetTooltip(
            "Manual image transformation is locked.\nClick to unlock and allow movement."
          );
        }
      }

      ImGui::SameLine();
      ImGui::Text("Transformation is locked");
    }
    else
    {
      if (ImGui::IsItemHovered())
      {
        ImGui::SetTooltip(
          "Manual image transformation is unlocked.\nClick to lock and prevent movement."
        );
      }

      ImGui::SameLine();
      ImGui::Text("Transformation is unlocked");
    }
  }

  if (0 < imageIndex)
  {
    // Rules for showing the buttons that change image order:
    const bool showDecreaseIndex = true | (1 < imageIndex);
    const bool showIncreaseIndex = true | (imageIndex < numImages - 1);

    if (showDecreaseIndex || showIncreaseIndex)
    {
      ImGui::Text("Image order: ");
    }

    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, 0.0f));

    if (showDecreaseIndex)
    {
      ImGui::SameLine();
      if (ImGui::Button(ICON_FK_FAST_BACKWARD))
      {
        moveImageToBack(imageUid);
      }
      if (ImGui::IsItemHovered())
      {
        ImGui::SetTooltip("Move image to backmost layer");
      }

      ImGui::SameLine();
      if (ImGui::Button(ICON_FK_BACKWARD))
      {
        moveImageBackward(imageUid);
      }
      if (ImGui::IsItemHovered())
      {
        ImGui::SetTooltip("Move image backward in layers (decrease the image order)");
      }
    }

    if (showIncreaseIndex)
    {
      ImGui::SameLine();
      if (ImGui::Button(ICON_FK_FORWARD))
      {
        moveImageForward(imageUid);
      }
      if (ImGui::IsItemHovered())
      {
        ImGui::SetTooltip("Move image forward in layers (increase the image order)");
      }

      ImGui::SameLine();
      if (ImGui::Button(ICON_FK_FAST_FORWARD))
      {
        moveImageToFront(imageUid);
      }
      if (ImGui::IsItemHovered())
      {
        ImGui::SetTooltip("Move image to frontmost layer");
      }
    }

    /*** ImGuiStyleVar_ItemSpacing ***/
    ImGui::PopStyleVar(1);
  }

  ImGui::Spacing();
  ImGui::Separator();
  ImGui::Spacing();

  // Open View Properties on first appearance
  ImGui::SetNextItemOpen(true, ImGuiCond_Appearing);
  if (ImGui::TreeNode("View Properties"))
  {
    // Component selection combo selection list. The component selection is shown only for
    // multi-component images, where each component is stored as a separate image.
    const bool showComponentSelection = (imgHeader.numComponentsPerPixel() > 1
                && Image::MultiComponentBufferType::SeparateImages == image->bufferType());

    if (showComponentSelection)
    {
      if (3 == imgHeader.numComponentsPerPixel() || 4 == imgHeader.numComponentsPerPixel())
      {
        ImGui::Dummy(ImVec2(0.0f, 1.0f));

        // Display 3- or 4-component images as RGB(A) color:
        bool displayAsColor = imgSettings.displayImageAsColor();

        if (ImGui::Checkbox("Render with color", &displayAsColor))
        {
          imgSettings.setDisplayImageAsColor(displayAsColor);
          updateImageUniforms();
          updateImageInterpolationMode();
        }
        ImGui::SameLine();
        helpMarker("Display multi-component image with color");

        if (imgSettings.displayImageAsColor() && 4 == imgHeader.numComponentsPerPixel())
        {
          bool ignoreAlpha = imgSettings.ignoreAlpha();

          if (ImGui::Checkbox("Ignore alpha component", &ignoreAlpha))
          {
            imgSettings.setIgnoreAlpha(ignoreAlpha);
            updateImageUniforms();
          }
          ImGui::SameLine();
          helpMarker("Ignore alpha component of RGBA image");
        }
      }

      // Global visibility (all components) checkbox:
      bool globalVisibility = imgSettings.globalVisibility();

      if (ImGui::Checkbox("Image visible", &globalVisibility))
      {
        imgSettings.setGlobalVisibility(globalVisibility);
        updateImageUniforms();
      }
      ImGui::SameLine();
      helpMarker("Show/hide all image components on all views (W)");

      if (activeSeg)
      {
        bool segVisible = activeSeg->settings().visibility();

        if (ImGui::Checkbox("Segmentation visible", &segVisible))
        {
          activeSeg->settings().setVisibility(segVisible);
          updateAllImageUniforms();
        }
        ImGui::SameLine();
        helpMarker("Show/hide the segmentation on all views (S)");
      }

      // Global image opacity slider:
      double globalImageOpacity = imgSettings.globalOpacity();
      if (mySliderF64("Image opacity", &globalImageOpacity, 0.0, 1.0))
      {
        imgSettings.setGlobalOpacity(globalImageOpacity);
        updateImageUniforms();
      }
      ImGui::SameLine();
      helpMarker("Image layer opacity");

      if (activeSeg)
      {
        double segOpacity = activeSeg->settings().opacity();
        if (mySliderF64("Seg. opacity", &segOpacity, 0.0, 1.0))
        {
          activeSeg->settings().setOpacity(segOpacity);
          updateAllImageUniforms();
        }
        ImGui::SameLine();
        helpMarker("Segmentation layer opacity");
      }

      ImGui::Spacing();
      ImGui::Separator();
      ImGui::Spacing();

      static const std::vector<std::string>
        sk_rgbaLetters{" (red)", " (green)", " (blue)", " (alpha)"};

      const std::string previewValue = (imgSettings.displayImageAsColor())
                                         ? std::to_string(imgSettings.activeComponent())
                                             + sk_rgbaLetters[imgSettings.activeComponent()]
                                         : std::to_string(imgSettings.activeComponent());

      if (ImGui::BeginCombo("Image component", previewValue.c_str()))
      {
        for (uint32_t comp = 0; comp < imgHeader.numComponentsPerPixel(); ++comp)
        {
          const std::string selectableValue = (imgSettings.displayImageAsColor())
                                                ? std::to_string(comp) + sk_rgbaLetters[comp]
                                                : std::to_string(comp);

          const bool isSelected = (imgSettings.activeComponent() == comp);

          if (ImGui::Selectable(selectableValue.c_str(), isSelected))
          {
            imgSettings.setActiveComponent(comp);
            updateImageUniforms();
          }

          if (isSelected)
            ImGui::SetItemDefaultFocus();
        }

        ImGui::EndCombo();
      }

      ImGui::SameLine();
      helpMarker("Select the image component to display and adjust");

      ImGui::Dummy(ImVec2(0.0f, 1.0f));
    }

    // Visibility checkbox:
    bool visible = imgSettings.visibility();

    static const std::string sk_compVisibleText("Component visible");
    static const std::string sk_imageVisibleText("Image visible");
    static const std::string sk_compOpacityText("Component opacity");
    static const std::string sk_imageOpacityText("Image opacity");

    const char* visibleCheckText = (image->header().numComponentsPerPixel() > 1)
                                     ? sk_compVisibleText.c_str()
                                     : sk_imageVisibleText.c_str();

    const char* opacitySliderText = (image->header().numComponentsPerPixel() > 1)
                                      ? sk_compOpacityText.c_str()
                                      : sk_imageOpacityText.c_str();

    if (ImGui::Checkbox(visibleCheckText, &visible))
    {
      imgSettings.setVisibility(visible);
      updateImageUniforms();
    }
    ImGui::SameLine();
    helpMarker("Show/hide the image on all views");

    if (activeSeg && !showComponentSelection)
    {
      bool segVisible = activeSeg->settings().visibility();

      if (ImGui::Checkbox("Segmentation visible", &segVisible))
      {
        activeSeg->settings().setVisibility(segVisible);
        updateAllImageUniforms();
      }
      ImGui::SameLine();
      helpMarker("Show/hide the segmentation on all views (S)");
    }

    // if (visible)
    {
      // Image opacity slider:
      double imageOpacity = imgSettings.opacity();
      if (mySliderF64(opacitySliderText, &imageOpacity, 0.0, 1.0))
      {
        imgSettings.setOpacity(imageOpacity);
        updateImageUniforms();
      }
      ImGui::SameLine();
      helpMarker("Image layer opacity");

      // Segmentation opacity slider:
      if (activeSeg && !showComponentSelection)
      {
        double segOpacity = activeSeg->settings().opacity();
        if (mySliderF64("Seg. opacity", &segOpacity, 0.0, 1.0))
        {
          activeSeg->settings().setOpacity(segOpacity);
          updateAllImageUniforms();
        }
        ImGui::SameLine();
        helpMarker("Segmentation layer opacity");
      }

      ImGui::Dummy(ImVec2(0.0f, 1.0f));
    }

    const bool imageHasFloatComponents =
            (ComponentType::Float32 == imgHeader.memoryComponentType() ||
             ComponentType::Float64 == imgHeader.memoryComponentType());

    if (imageHasFloatComponents)
    {
      // Threshold range:
      const float threshMin = static_cast<float>(imgSettings.thresholdRange().first);
      const float threshMax = static_cast<float>(imgSettings.thresholdRange().second);

      // Speed of range slider is based on the range
      const float speed = static_cast<float>(threshMax - threshMin) / 1000.0f;

      // Window/level sliders:
      const float windowWidthMin = static_cast<float>(imgSettings.minMaxWindowWidthRange().first);
      const float windowWidthMax = static_cast<float>(imgSettings.minMaxWindowWidthRange().second);

      const float windowCenterMin = static_cast<float>(imgSettings.minMaxWindowCenterRange().first);
      const float windowCenterMax = static_cast<float>(imgSettings.minMaxWindowCenterRange().second);

      const float windowMin = static_cast<float>(imgSettings.minMaxWindowRange().first);
      const float windowMax = static_cast<float>(imgSettings.minMaxWindowRange().second);

      float windowLow = static_cast<float>(imgSettings.windowValuesLowHigh().first);
      float windowHigh = static_cast<float>(imgSettings.windowValuesLowHigh().second);

      double windowWidth = imgSettings.windowWidth();
      double windowCenter = imgSettings.windowCenter();

      ImGui::Text("Windowing:");

      if (mySliderF64("Width", &windowWidth, windowWidthMin, windowWidthMax, valuesFormat))
      {
        imgSettings.setWindowWidth(windowWidth);
        updateImageUniforms();
      }
      ImGui::SameLine();
      helpMarker("Window width");

      if (mySliderF64("Level", &windowCenter, windowCenterMin, windowCenterMax, valuesFormat))
      {
        imgSettings.setWindowCenter(windowCenter);
        updateImageUniforms();
      }
      ImGui::SameLine();
      helpMarker("Window level (center)");

      if (ImGui::DragFloatRange2(
            "Values",
            &windowLow,
            &windowHigh,
            speed,
            windowMin,
            windowMax,
            minValuesFormat,
            maxValuesFormat,
            ImGuiSliderFlags_AlwaysClamp
          ))
      {
        imgSettings.setWindowValueLow(windowLow);
        imgSettings.setWindowValueHigh(windowHigh);
        updateImageUniforms();
      }
      ImGui::SameLine();
      helpMarker("Set the minimum and maximum values of the window range");

      const QuantileOfValue qLow = image->valueToQuantile(imgSettings.activeComponent(), windowLow);
      const QuantileOfValue qHigh = image->valueToQuantile(imgSettings.activeComponent(), windowHigh);

      constexpr float windowPercentileMin = 0.0f;
      constexpr float windowPercentileMax = 100.0f;

      const float windowPercentileLowCurrent = 100.0f * qLow.lowerQuantile;
      const float windowPercentileHighCurrent = 100.0f * qHigh.upperQuantile;

      float windowPercentileLowAttempted = windowPercentileLowCurrent;
      float windowPercentileHighAttempted = windowPercentileHighCurrent;

      if (ImGui::DragFloatRange2(
            "Percentiles",
            &windowPercentileLowAttempted,
            &windowPercentileHighAttempted,
            windowPercentileStep,
            windowPercentileMin,
            windowPercentileMax,
            minPercentilesFormat,
            maxPercentilesFormat,
            ImGuiSliderFlags_AlwaysClamp
          ))
      {
        if (windowPercentileLowCurrent != windowPercentileLowAttempted)
        {
          const double windowPercentileLowBumped = bumpQuantile(
            *image,
            imgSettings.activeComponent(),
            windowPercentileLowCurrent / 100.0,
            windowPercentileLowAttempted / 100.0,
            windowLow
          );

          const double newWindowLow
            = image->quantileToValue(imgSettings.activeComponent(), windowPercentileLowBumped);
          imgSettings.setWindowValueLow(newWindowLow);
          updateImageUniforms();
        }

        if (windowPercentileHighCurrent != windowPercentileHighAttempted)
        {
          const double windowPercentileHighBumped = bumpQuantile(
            *image,
            imgSettings.activeComponent(),
            windowPercentileHighCurrent / 100.0,
            windowPercentileHighAttempted / 100.0,
            windowHigh
          );

          const double newWindowHigh
            = image->quantileToValue(imgSettings.activeComponent(), windowPercentileHighBumped);
          imgSettings.setWindowValueHigh(newWindowHigh);
          updateImageUniforms();
        }
      }
      ImGui::SameLine();
      helpMarker("Set the minimum and maximum percentiles of the window range");

      float threshLow = static_cast<float>(imgSettings.thresholds().first);
      float threshHigh = static_cast<float>(imgSettings.thresholds().second);

      if (ImGui::DragFloatRange2(
            "Thresholds",
            &threshLow,
            &threshHigh,
            speed,
            threshMin,
            threshMax,
            minValuesFormat,
            maxValuesFormat,
            ImGuiSliderFlags_AlwaysClamp
          ))
      {
        imgSettings.setThresholdLow(static_cast<double>(threshLow));
        imgSettings.setThresholdHigh(static_cast<double>(threshHigh));
        updateImageUniforms();
      }
      ImGui::SameLine();
      helpMarker("Lower and upper image thresholds");
    }
    else
    {
      // Use a speed of 1 for integer images:
      constexpr float speed = 1.0f;

      // Window/level sliders:
      const int32_t windowWidthMin = static_cast<int32_t>(
        std::floor(imgSettings.minMaxWindowWidthRange().first)
      );
      const int32_t windowWidthMax = static_cast<int32_t>(
        std::ceil(imgSettings.minMaxWindowWidthRange().second)
      );

      const int32_t windowCenterMin = static_cast<int32_t>(
        std::floor(imgSettings.minMaxWindowCenterRange().first)
      );
      const int32_t windowCenterMax = static_cast<int32_t>(
        std::ceil(imgSettings.minMaxWindowCenterRange().second)
      );

      const int32_t windowMin = static_cast<int32_t>(
        std::floor(imgSettings.minMaxWindowRange().first)
      );
      const int32_t windowMax = static_cast<int32_t>(
        std::ceil(imgSettings.minMaxWindowRange().second)
      );

      int32_t windowLow = static_cast<int32_t>(imgSettings.windowValuesLowHigh().first);
      int32_t windowHigh = static_cast<int32_t>(imgSettings.windowValuesLowHigh().second);

      int64_t windowWidth = static_cast<int64_t>(imgSettings.windowWidth());
      int64_t windowCenter = static_cast<int64_t>(imgSettings.windowCenter());

      ImGui::Text("Windowing:");

      if (mySliderS64("Width", &windowWidth, windowWidthMin, windowWidthMax))
      {
        imgSettings.setWindowWidth(windowWidth);
        updateImageUniforms();
      }
      ImGui::SameLine();
      helpMarker("Window width");

      if (mySliderS64("Level", &windowCenter, windowCenterMin, windowCenterMax))
      {
        imgSettings.setWindowCenter(windowCenter);
        updateImageUniforms();
      }
      ImGui::SameLine();
      helpMarker("Window level (center)");

      if (ImGui::DragIntRange2(
            "Values",
            &windowLow,
            &windowHigh,
            speed,
            windowMin,
            windowMax,
            "Min: %d",
            "Max: %d",
            ImGuiSliderFlags_AlwaysClamp
          ))
      {
        imgSettings.setWindowValueLow(windowLow);
        imgSettings.setWindowValueHigh(windowHigh);
        updateImageUniforms();
      }
      ImGui::SameLine();
      helpMarker("Set the minimum and maximum of the window range");

      const QuantileOfValue qLow
        = image->valueToQuantile(imgSettings.activeComponent(), static_cast<int64_t>(windowLow));
      const QuantileOfValue qHigh
        = image->valueToQuantile(imgSettings.activeComponent(), static_cast<int64_t>(windowHigh));

      constexpr float windowPercentileMin = 0.0f;
      constexpr float windowPercentileMax = 100.0f;

      const float windowPercentileLowCurrent = 100.0f * qLow.lowerQuantile;
      const float windowPercentileHighCurrent = 100.0f * qHigh.upperQuantile;

      float windowPercentileLowAttempted = windowPercentileLowCurrent;
      float windowPercentileHighAttempted = windowPercentileHighCurrent;

      if (ImGui::DragFloatRange2(
            "Percentiles",
            &windowPercentileLowAttempted,
            &windowPercentileHighAttempted,
            windowPercentileStep,
            windowPercentileMin,
            windowPercentileMax,
            minPercentilesFormat,
            maxPercentilesFormat,
            ImGuiSliderFlags_AlwaysClamp
          ))
      {
        if (windowPercentileLowCurrent != windowPercentileLowAttempted)
        {
          const double windowPercentileLowBumped = bumpQuantile(
            *image,
            imgSettings.activeComponent(),
            windowPercentileLowCurrent / 100.0,
            windowPercentileLowAttempted / 100.0,
            windowLow
          );

          const double newWindowLow
            = image->quantileToValue(imgSettings.activeComponent(), windowPercentileLowBumped);
          imgSettings.setWindowValueLow(newWindowLow);
          updateImageUniforms();
        }

        if (windowPercentileHighCurrent != windowPercentileHighAttempted)
        {
          const double windowPercentileHighBumped = bumpQuantile(
            *image,
            imgSettings.activeComponent(),
            windowPercentileHighCurrent / 100.0,
            windowPercentileHighAttempted / 100.0,
            windowHigh
          );

          const double newWindowHigh
            = image->quantileToValue(imgSettings.activeComponent(), windowPercentileHighBumped);
          imgSettings.setWindowValueHigh(newWindowHigh);
          updateImageUniforms();
        }
      }
      ImGui::SameLine();
      helpMarker("Set the minimum and maximum percentiles of the window range");

      // Threshold range:
      const int32_t threshMin = static_cast<int32_t>(imgSettings.thresholdRange().first);
      const int32_t threshMax = static_cast<int32_t>(imgSettings.thresholdRange().second);

      int32_t threshLow = static_cast<int32_t>(imgSettings.thresholds().first);
      int32_t threshHigh = static_cast<int32_t>(imgSettings.thresholds().second);

      /// Speed of range slider is based on the image range
      if (ImGui::DragIntRange2(
            "Thresholds",
            &threshLow,
            &threshHigh,
            speed,
            threshMin,
            threshMax,
            "Min: %d",
            "Max: %d",
            ImGuiSliderFlags_AlwaysClamp
          ))
      {
        imgSettings.setThresholdLow(static_cast<double>(threshLow));
        imgSettings.setThresholdHigh(static_cast<double>(threshHigh));
        updateImageUniforms();
      }
      ImGui::SameLine();
      helpMarker("Lower and upper image thresholds");
    }
    ImGui::Spacing();

    /*
        ImGui::Text("Auto window: "); ImGui::SameLine();

        const auto& stats = imgSettings.componentStatistics(imgSettings.activeComponent());

        if (ImGui::Button("Max"))
        {
            imgSettings.setWindowValueLow(stats.m_minimum);
            imgSettings.setWindowValueHigh(stats.m_maximum);
            updateImageUniforms();
        }
        ImGui::SameLine();

        /// @todo Clear this up.... [1, 99]%
        /// Or separate buttons for low, high window
        if (ImGui::Button("99\%"))
        {
            imgSettings.setWindowValueLow(stats.m_quantiles[1]);
            imgSettings.setWindowValueHigh(stats.m_quantiles[99]);
            updateImageUniforms();
        }
        ImGui::SameLine();

        if (ImGui::Button("98\%"))
        {
            imgSettings.setWindowValueLow(stats.m_quantiles[2]);
            imgSettings.setWindowValueHigh(stats.m_quantiles[98]);
            updateImageUniforms();
        }
        ImGui::SameLine();

        if (ImGui::Button("95\%"))
        {
            imgSettings.setWindowValueLow(stats.m_quantiles[5]);
            imgSettings.setWindowValueHigh(stats.m_quantiles[95]);
            updateImageUniforms();
        }
        ImGui::SameLine();

        if (ImGui::Button("90\%"))
        {
            imgSettings.setWindowValueLow(stats.m_quantiles[10]);
            imgSettings.setWindowValueHigh(stats.m_quantiles[90]);
            updateImageUniforms();
        }
        ImGui::SameLine(); helpMarker("Set window based on percentiles of the image histogram");
*/

    auto getImageInterpMode = [&imgSettings]()
    {
      return (imgSettings.displayImageAsColor()) ? imgSettings.colorInterpolationMode()
                                                 : imgSettings.interpolationMode();
    };

    auto setImageInterpMode = [&imgSettings](const InterpolationMode& mode)
    {
      (imgSettings.displayImageAsColor()) ? imgSettings.setColorInterpolationMode(mode)
                                          : imgSettings.setInterpolationMode(mode);
    };

    if (ImGui::BeginCombo("Sampling", typeString(getImageInterpMode()).c_str()))
    {
      for (const auto& mode : AllInterpolationModes)
      {
        if (ImGui::Selectable(typeString(mode).c_str(), (mode == getImageInterpMode())))
        {
          setImageInterpMode(mode);
          updateImageInterpolationMode();

          if (mode == getImageInterpMode())
          {
            ImGui::SetItemDefaultFocus();
          }
        }
      }
      ImGui::EndCombo();
    }
    ImGui::SameLine();
    helpMarker("Select the image interpolation type");

    std::size_t cmapIndex = getCurrentImageColormapIndex();

    ImageColorMap* cmap = getImageColorMap(cmapIndex);

    if (cmap && !imgSettings.displayImageAsColor())
    {
      bool* showImageColormapWindow = &(guiData.m_showImageColormapWindow[imageUid]);

      glm::vec3 hsvMods = imgSettings.colorMapHsvModFactors();
      glm::ivec3 hsvModsInt
        = glm::ivec3{360.0f * hsvMods[0], 100.0f * hsvMods[1], 100.0f * hsvMods[2]};

      // Colormap preview:
      const float contentWidth = ImGui::CalcItemWidth(); // ImGui::GetContentRegionAvail().x;
      const float height
        = (ImGui::GetIO().Fonts->Fonts[0]->FontSize * ImGui::GetIO().FontGlobalScale);

      char label[128];
      snprintf(label, 128, "%s##cmap_%zu", cmap->name().c_str(), imageIndex);

      const bool doQuantize
        = (!imgSettings.colorMapContinuous() && (ImageColorMap::InterpolationMode::Linear == cmap->interpolationMode()));

      //            ImGui::Dummy(ImVec2(0.0f, 2.0f));
      ImGui::Spacing();

      // ImGui::Text("Color map:");

      *showImageColormapWindow |= ImGui::paletteButton(
        label,
        cmap->data_RGBA_asVector(),
        imgSettings.isColorMapInverted(),
        doQuantize,
        imgSettings.colorMapQuantizationLevels(),
        hsvMods,
        ImVec2(contentWidth, height)
      );

      if (ImGui::IsItemHovered())
      {
        ImGui::SetTooltip("%s", cmap->description().c_str());
      }

      ImGui::SetNextItemOpen(false, ImGuiCond_Appearing);

      if (ImGui::TreeNode("Color map settings"))
      {
        // Image colormap dialog:
        *showImageColormapWindow |= ImGui::Button("Select color map");

        bool invertedCmap = imgSettings.isColorMapInverted();

        if (ImGui::Checkbox("Inverted", &invertedCmap))
        {
          imgSettings.setColorMapInverted(invertedCmap);
          updateImageUniforms();
        }
        ImGui::SameLine();
        helpMarker("Invert the image color map");

        // If the color map has nearest-neighbor interpolation mode,
        // then we are forced to use the discrete setting:
        //                const bool forcedDiscrete = (ImageColorMap::InterpolationMode::Nearest == cmap->interpolationMode());

        bool colorMapContinuous = imgSettings.colorMapContinuous();

        ImGui::SameLine();
        if (ImGui::RadioButton("Continuous", colorMapContinuous /*&& ! forcedDiscrete*/))
        {
          colorMapContinuous = true;
          imgSettings.setColorMapContinuous(colorMapContinuous);
          updateImageUniforms();
        }

        ImGui::SameLine();
        if (ImGui::RadioButton("Discrete", !colorMapContinuous /*|| forcedDiscrete*/))
        {
          colorMapContinuous = false;
          imgSettings.setColorMapContinuous(colorMapContinuous);
          updateImageUniforms();
        }

        ImGui::SameLine();
        helpMarker("Render color map as either continuous or discrete");

        if (!colorMapContinuous)
        {
          int numColorMapLevels = static_cast<int>(imgSettings.colorMapQuantizationLevels());

          ImGui::InputInt("Color levels", &numColorMapLevels);
          {
            numColorMapLevels = std::min(std::max(numColorMapLevels, 2), 256);
            imgSettings.setColorMapQuantizationLevels(static_cast<uint32_t>(numColorMapLevels));
            updateImageUniforms();
          }
          ImGui::SameLine();
          helpMarker("Number of image color map quantization levels");
        }

        static constexpr int hue_min = 0;
        static constexpr int hue_max = 360;

        static constexpr int sat_min = 0;
        static constexpr int sat_max = 100;

        static constexpr int val_min = 0;
        static constexpr int val_max = 100;

        ImGui::Spacing();
        ImGui::Text("HSV color adjustment:");
        /*
                    if (ImGuiKnobs::KnobInt(
                            "Hue", glm::value_ptr(hsvModsInt), hue_min, hue_max, 1, "%i%",
                            ImGuiKnobVariant_Stepped, 0,
                            ImGuiKnobFlags_ValueTooltip | ImGuiKnobFlags_DragHorizontal, 12))
                    {
                        imgSettings.setColorMapHueModFactor(hsvModsInt[0] / 360.0f);
                        updateImageUniforms();
                    }

                    if (ImGui::SliderScalarN("Sat. & value", ImGuiDataType_S32, &(hsvModsInt[1]), 2, &sv_min, &sv_max))
                    {
                        //                    imgSettings.setColormapHsvModfactors(glm::vec3{hsvMods} / 360.0f);
                        imgSettings.setColorMapSatModFactor(hsvModsInt[1] / 100.0f);
                        imgSettings.setColorMapValModFactor(hsvModsInt[2] / 100.0f);
                        updateImageUniforms();
                    }

                    ImGui::SameLine(); helpMarker("Apply saturation and value adjustments to the color map");
*/

        const int* hsv_mins[3] = {&hue_min, &sat_min, &val_min};
        const int* hsv_maxs[3] = {&hue_max, &sat_max, &val_max};

        const std::string h_format("%d deg");
        const std::string s_format("%d");
        const std::string v_format("%d");

        const char* hsv_formats[3] = {h_format.c_str(), s_format.c_str(), v_format.c_str()};

        if (ImGui::SliderScalarN_multiComp(
              "HSV",
              ImGuiDataType_S32,
              glm::value_ptr(hsvModsInt),
              3,
              reinterpret_cast<const void**>(hsv_mins),
              reinterpret_cast<const void**>(hsv_maxs),
              hsv_formats,
              0
            ))
        {
          imgSettings.setColorMapHueModFactor(hsvModsInt[0] / 360.0f);
          imgSettings.setColorMapSatModFactor(hsvModsInt[1] / 100.0f);
          imgSettings.setColorMapValModFactor(hsvModsInt[2] / 100.0f);
          updateImageUniforms();
        }

        ImGui::TreePop();
      }

      auto getImageColorMapInverted = [&imgSettings]() { return imgSettings.isColorMapInverted(); };

      auto getImageColorMapContinuous = [&imgSettings]()
      { return imgSettings.colorMapContinuous(); };

      auto getImageColorMapLevels = [&imgSettings]()
      { return imgSettings.colorMapQuantizationLevels(); };

      renderPaletteWindow(
        std::string(
          "Select colormap for image '" + imgSettings.displayName() + "' (component "
          + std::to_string(imgSettings.activeComponent()) + ")"
        )
          .c_str(),
        showImageColormapWindow,
        getNumImageColorMaps,
        getImageColorMap,
        getCurrentImageColormapIndex,
        setCurrentImageColormapIndex,
        getImageColorMapInverted,
        getImageColorMapContinuous,
        getImageColorMapLevels,
        hsvMods,
        updateImageUniforms
      );
    }

    // Edge settings
    ImGui::Spacing();
    ImGui::Spacing();

    // Show edges:
    bool showEdges = imgSettings.showEdges();
    if (ImGui::Checkbox("Show edges", &showEdges))
    {
      imgSettings.setShowEdges(showEdges);
      updateImageUniforms();
    }
    ImGui::SameLine();
    helpMarker("Show/hide the edges of the image (E)");

    ImGui::SetNextItemOpen(showEdges, ImGuiCond_Appearing);

    if (showEdges && ImGui::TreeNode("Edge settings"))
    {
      // Recommend linear interpolation:
      if (InterpolationMode::NearestNeighbor == imgSettings.interpolationMode())
      {
        ImGui::Text("Note: Linear or cubic interpolation are recommended when showing edges.");
        //                    imgSettings.setInterpolationMode(InterpolationMode::Linear);
        //                    updateImageInterpolationMode();
      }

      // Threshold edges:
      bool thresholdEdges = imgSettings.thresholdEdges();
      if (ImGui::Checkbox("Hard edges", &thresholdEdges))
      {
        imgSettings.setThresholdEdges(thresholdEdges);
        updateImageUniforms();
      }
      ImGui::SameLine();
      helpMarker("Apply thresholding to edge gradient magnitude to get hard edges");

      //                // Windowed edges:
      //                bool windowedEdges = imgSettings.windowedEdges();
      //                if (ImGui::Checkbox("Compute edges after windowing", &windowedEdges))
      //                {
      //                    imgSettings.setWindowedEdges(windowedEdges);
      //                    updateImageUniforms();
      //                }
      //                ImGui::SameLine();
      //                HelpMarker("Compute edges after applying windowing (width/level) to the image");

      // Use Sobel or Frei-Chen:
      //                bool useFreiChen = imgSettings.useFreiChen();
      //                if (ImGui::Checkbox("Frei-Chen filter", &useFreiChen))
      //                {
      //                    imgSettings.setUseFreiChen(useFreiChen);
      //                    updateImageUniforms();
      //                }
      //                ImGui::SameLine();
      //                HelpMarker("Compute edges using Sobel or Frei-Chen convolution filters");

      // Overlay edges:
      bool overlayEdges = imgSettings.overlayEdges();
      if (ImGui::Checkbox("Overlay edges on image", &overlayEdges))
      {
        if (imgSettings.colormapEdges())
        {
          // Do not allow edge overlay if edges are colormapped
          overlayEdges = false;
        }

        imgSettings.setOverlayEdges(overlayEdges);
        updateImageUniforms();
      }
      ImGui::SameLine();
      helpMarker("Overlay edges on top of the image");

      // Colormap the edges (always false if overlaying the edges or thresholding the edges):
      if (overlayEdges || thresholdEdges)
      {
        imgSettings.setColormapEdges(false);
        updateImageUniforms();
      }

      bool colormapEdges = imgSettings.colormapEdges();

      if (!overlayEdges && !thresholdEdges)
      {
        if (ImGui::Checkbox("Apply colormap to edges", &colormapEdges))
        {
          if (overlayEdges)
          {
            colormapEdges = false;
          }

          imgSettings.setColormapEdges(colormapEdges);
          updateImageUniforms();
        }
        ImGui::SameLine();
        helpMarker("Apply the image colormap to image edges");
      }

      if (!colormapEdges)
      {
        glm::vec4 edgeColor{imgSettings.edgeColor(), imgSettings.edgeOpacity()};

        if (ImGui::ColorEdit4("Edge color", glm::value_ptr(edgeColor), sk_colorAlphaEditFlags))
        {
          imgSettings.setEdgeColor(edgeColor);
          imgSettings.setEdgeOpacity(static_cast<double>(edgeColor.a));
          updateImageUniforms();
        }
        ImGui::SameLine();
        helpMarker("Edge color and opacity");
      }
      else
      {
        // Cannot overlay edges with colormapping enabled
        imgSettings.setOverlayEdges(false);
        updateImageUniforms();
      }

      // Edge magnitude (only shown if thresholding edges):
      if (thresholdEdges)
      {
        double edgeMag = imgSettings.edgeMagnitude();
        if (mySliderF64("Magnitude", &edgeMag, 0.01, 1.00))
        {
          imgSettings.setEdgeMagnitude(edgeMag);
          updateImageUniforms();
        }
        ImGui::SameLine();
        helpMarker("Magnitude of threshold above which hard edges are shown");
      }
      else
      {
        double edgeMag = 1.0 - imgSettings.edgeMagnitude();
        if (mySliderF64("Scale", &edgeMag, 0.01, 1.00))
        {
          imgSettings.setEdgeMagnitude(1.0 - edgeMag);
          updateImageUniforms();
        }
        ImGui::SameLine();
        helpMarker("Scale applied to edge magnitude");
      }

      ImGui::TreePop();
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    ImGui::TreePop();
  }

  if (ImGui::TreeNode("Transformations"))
  {
    /// @note This code is commented out for now, since additional images are implicitly locked
    /// to the reference image, since the reference image does not get transformed.

    /*
        if (! isRef)
        {
            ImGui::Spacing();

            // Note: We could change this to imgSettings.isLockedToReference(),
            // if at some time in the future we allow transformation of the reference image.

            bool lockTxToReference = true;

            if (ImGui::Checkbox("Lock to reference", &lockTxToReference))
            {
                imgSettings.setLockedToReference(lockTxToReference);
                updateImageUniforms();
            }
            ImGui::SameLine();
            helpMarker("Lock this image's transformation to the reference image, "
                        "so that it moves with the reference (always true)");

            ImGui::Spacing();
            ImGui::Separator();
        }
        */

    ImGui::Spacing();

    // The initial affine and manual affine transformations are disabled for the reference image:
    const bool forceDisableInitialTxs = isRef;

    ImGui::Text("Initial affine transformation:");
    ImGui::SameLine();
    helpMarker("Initial affine transformation matrix (read from file)");

    bool enable_affine_T_subject = imgTx.get_enable_affine_T_subject();
    if (ImGui::Checkbox("Apply##affine_T_subject", &enable_affine_T_subject) && !forceDisableInitialTxs)
    {
      imgTx.set_enable_affine_T_subject(enable_affine_T_subject);
      updateImageUniforms();
    }
    ImGui::SameLine();

    if (forceDisableInitialTxs)
    {
      helpMarker("Enable/disable application of the initial affine transformation. "
                 "Always disabled for the reference image.");
    }
    else
    {
      helpMarker("Enable/disable application of the initial affine transformation.");
    }

    if (enable_affine_T_subject && !forceDisableInitialTxs)
    {
      if (auto fileName = imgTx.get_affine_T_subject_fileName())
      {
        std::string fileNameString = fileName->string();
        ImGui::InputText("File", &fileNameString, ImGuiInputTextFlags_ReadOnly);
        ImGui::Spacing();
      }

      glm::mat4 aff_T_sub = glm::transpose(imgTx.get_affine_T_subject());

      ImGui::PushItemWidth(-1);
      ImGui::InputFloat4("", glm::value_ptr(aff_T_sub[0]), txFormat, ImGuiInputTextFlags_ReadOnly);
      ImGui::InputFloat4("", glm::value_ptr(aff_T_sub[1]), txFormat, ImGuiInputTextFlags_ReadOnly);
      ImGui::InputFloat4("", glm::value_ptr(aff_T_sub[2]), txFormat, ImGuiInputTextFlags_ReadOnly);
      ImGui::InputFloat4("", glm::value_ptr(aff_T_sub[3]), txFormat, ImGuiInputTextFlags_ReadOnly);
      ImGui::PopItemWidth();
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    ImGui::Text("Manual affine transformation:");
    ImGui::SameLine();
    helpMarker("Manual affine transformation from Subject to World space");

    bool enable_worldDef_T_affine = imgTx.get_enable_worldDef_T_affine();
    if (ImGui::Checkbox("Apply##worldDef_T_affine", &enable_worldDef_T_affine) && !forceDisableInitialTxs)
    {
      imgTx.set_enable_worldDef_T_affine(enable_worldDef_T_affine);
      updateImageUniforms();
    }
    ImGui::SameLine();

    if (forceDisableInitialTxs)
    {
      helpMarker("Enable/disable application of the manual affine transformation from Subject to "
                 "World space. "
                 "Always disabled for the reference image.");
    }
    else
    {
      helpMarker("Enable/disable application of the manual affine transformation from Subject to "
                 "World space.");
    }

    if (enable_worldDef_T_affine && !forceDisableInitialTxs)
    {
      glm::quat w_T_s_rotation = imgTx.get_worldDef_T_affine_rotation();
      glm::vec3 w_T_s_scale = imgTx.get_worldDef_T_affine_scale();
      glm::vec3 w_T_s_trans = imgTx.get_worldDef_T_affine_translation();

      float angle = glm::degrees(glm::angle(w_T_s_rotation));
      glm::vec3 axis = glm::normalize(glm::axis(w_T_s_rotation));

      if (ImGui::InputFloat3("Translation", glm::value_ptr(w_T_s_trans), txFormat))
      {
        imgTx.set_worldDef_T_affine_translation(w_T_s_trans);
        updateImageUniforms();
      }
      ImGui::SameLine();
      helpMarker("Image translation in x, y, z");

      if (ImGui::InputFloat3("Scale", glm::value_ptr(w_T_s_scale), txFormat))
      {
        if (glm::epsilonNotEqual(w_T_s_scale.x, 0.0f, glm::epsilon<float>()) &&
                     glm::epsilonNotEqual(w_T_s_scale.y, 0.0f, glm::epsilon<float>()) &&
                     glm::epsilonNotEqual(w_T_s_scale.z, 0.0f, glm::epsilon<float>()))
        {
          imgTx.set_worldDef_T_affine_scale(w_T_s_scale);
          updateImageUniforms();
        }
      }
      ImGui::SameLine();
      helpMarker("Image scale in x, y, z");

      /// @todo Put in a more friendly rotation widget. For now, disable changing the rotation
      /// @see https://github.com/BrutPitt/imGuIZMO.quat
      /// @see https://github.com/CedricGuillemet/ImGuizmo

      //            ImGui::Text("Rotation");
      if (ImGui::InputFloat("Rotation angle", &angle, 0.01f, 0.1f, txFormat))
      {
        //                const float n = glm::length(axis);
        //                if (n < 1e-6f)
        //                {
        //                    const glm::quat newRot = glm::angleAxis(glm::radians(angle), glm::normalize(axis));
        //                    imgTx.set_worldDef_T_affine_rotation(newRot);
        //                    updateImageUniforms();
        //                }
      }
      ImGui::SameLine();
      helpMarker("Image rotation angle (degrees) [editing disabled]");

      if (ImGui::InputFloat3("Rotation axis", glm::value_ptr(axis), txFormat))
      {
        //                const float n = glm::length(axis);
        //                if (n < 1e-6f)
        //                {
        //                    const glm::quat newRot = glm::angleAxis(glm::radians(angle), glm::normalize(axis));
        //                    imgTx.set_worldDef_T_affine_rotation(newRot);
        //                    updateImageUniforms();
        //                }
      }
      ImGui::SameLine();
      helpMarker("Image rotation axis [editing disabled]");

      //            if (ImGui::InputFloat4("Rotation", glm::value_ptr(w_T_s_rotation), "%.3f"))
      //            {
      //                imgTx.set_worldDef_T_affine_rotation(w_T_s_rotation);
      //                updateImageUniforms();
      //            }
      //            ImGui::SameLine();
      //            HelpMarker("Image rotation defined as a quaternion");

      ImGui::Spacing();
      glm::mat4 world_T_affine = glm::transpose(imgTx.get_worldDef_T_affine());

      ImGui::PushItemWidth(-1);
      ImGui::Text("Subject-to-World matrix:");
      ImGui::InputFloat4(
        "", glm::value_ptr(world_T_affine[0]), txFormat, ImGuiInputTextFlags_ReadOnly
      );
      ImGui::InputFloat4(
        "", glm::value_ptr(world_T_affine[1]), txFormat, ImGuiInputTextFlags_ReadOnly
      );
      ImGui::InputFloat4(
        "", glm::value_ptr(world_T_affine[2]), txFormat, ImGuiInputTextFlags_ReadOnly
      );
      ImGui::InputFloat4(
        "", glm::value_ptr(world_T_affine[3]), txFormat, ImGuiInputTextFlags_ReadOnly
      );
      ImGui::PopItemWidth();

      ImGui::Spacing();
      ImGui::Separator();
      ImGui::Spacing();

      if (ImGui::Button("Reset manual transformation to identity"))
      {
        imgTx.reset_worldDef_T_affine();
        updateImageUniforms();
      }
      ImGui::SameLine();
      helpMarker(
        "Reset the manual component of the affine transformation matrix from Subject to World space"
      );

      // Save manual tx to file:
      static const char* sk_buttonText("Save manual transformation...");
      static const char* sk_dialogTitle("Select Manual Transformation");
      static const std::vector<std::string> sk_dialogFilters{};

      const auto selectedManualTxFile
        = ImGui::renderFileButtonDialogAndWindow(sk_buttonText, sk_dialogTitle, sk_dialogFilters);

      ImGui::SameLine();
      helpMarker(
        "Save the manual component of the affine transformation matrix from Subject to World space"
      );

      if (selectedManualTxFile)
      {
        const glm::dmat4 worldDef_T_affine{imgTx.get_worldDef_T_affine()};

        if (serialize::saveAffineTxFile(worldDef_T_affine, *selectedManualTxFile))
        {
          spdlog::info("Saved manual transformation matrix to file {}", *selectedManualTxFile);
        }
        else
        {
          spdlog::error("Error saving manual transformation matrix to file {}", *selectedManualTxFile);
        }
      }

      if (imgTx.get_enable_affine_T_subject())
      {
        // Save concatenated initial + manual tx to file:
        static const char* sk_saveInitAndManualTxButtonText(
          "Save initial + manual transformation..."
        );
        static const char* sk_saveInitAndManualTxDialogTitle(
          "Select Concatenated Initial and Manual Transformation"
        );

        const auto selectedInitAndManualConcatTxFile = ImGui::renderFileButtonDialogAndWindow(
          sk_saveInitAndManualTxButtonText, sk_saveInitAndManualTxDialogTitle, sk_dialogFilters
        );

        ImGui::SameLine();
        helpMarker("Save the concatenated initial and manual affine transformation matrix from "
                   "Subject to World space");

        if (selectedInitAndManualConcatTxFile)
        {
          const glm::dmat4 affine_T_subject{imgTx.get_affine_T_subject()};
          const glm::dmat4 worldDef_T_affine{imgTx.get_worldDef_T_affine()};

          if (serialize::saveAffineTxFile(
                worldDef_T_affine * affine_T_subject, *selectedInitAndManualConcatTxFile
              ))
          {
            spdlog::info(
              "Saved concatenated initial and manual affine transformation matrix to file {}",
              *selectedInitAndManualConcatTxFile
            );
          }
          else
          {
            spdlog::error(
              "Error saving concatenated initial and manual affine transformation matrix to file "
              "{}",
              *selectedInitAndManualConcatTxFile
            );
          }
        }
      }
    }

    ImGui::Spacing();
    ImGui::Separator();

    ImGui::TreePop();
  }

  if (ImGui::TreeNode("Header Information"))
  {
    renderImageHeaderInformation(appData, imageUid, *image, updateImageUniforms, recenterAllViews);
    ImGui::TreePop();
  }

  if (ImGui::TreeNode("Histogram"))
  {
    if (image->header().numPixels() > std::numeric_limits<int32_t>::max())
    {
      spdlog::warn(
        "Number of pixels in image ({}) exceeds maximum supported by image histogram",
        image->header().numPixels()
      );
    }

    const uint32_t comp = imgSettings.activeComponent();
    const void* buffer = image->bufferSortedAsVoid(comp);
    const int bufferSize = static_cast<int>(image->header().numPixels());
    const std::string& format = appData.guiData().m_imageValuePrecisionFormat;

    switch (image->header().memoryComponentType())
    {
    case ComponentType::Int8:
    {
      drawImageHistogram(static_cast<const int8_t*>(buffer), bufferSize, imgSettings, format);
      break;
    }
    case ComponentType::UInt8:
    {
      drawImageHistogram(static_cast<const uint8_t*>(buffer), bufferSize, imgSettings, format);
      break;
    }
    case ComponentType::Int16:
    {
      drawImageHistogram(static_cast<const int16_t*>(buffer), bufferSize, imgSettings, format);
      break;
    }
    case ComponentType::UInt16:
    {
      drawImageHistogram(static_cast<const uint16_t*>(buffer), bufferSize, imgSettings, format);
      break;
    }
    case ComponentType::Int32:
    {
      drawImageHistogram(static_cast<const int32_t*>(buffer), bufferSize, imgSettings, format);
      break;
    }
    case ComponentType::UInt32:
    {
      drawImageHistogram(static_cast<const uint32_t*>(buffer), bufferSize, imgSettings, format);
      break;
    }
    case ComponentType::Float32:
    {
      drawImageHistogram(static_cast<const float*>(buffer), bufferSize, imgSettings, format);
      break;
    }
    default:
    {
      break;
    }
    }

    ImGui::TreePop();
  }

  ImGui::Spacing();

  ImGui::PopID(); // imageUid
}

void renderSegmentationHeader(
  AppData& appData,
  const uuids::uuid& imageUid,
  size_t imageIndex,
  Image* image,
  bool isActiveImage,
  const std::function<void(void)>& updateImageUniforms,
  const std::function<ParcellationLabelTable*(size_t tableIndex)>& getLabelTable,
  const std::function<void(size_t tableIndex)>& updateLabelColorTableTexture,
  const std::function<void(size_t labelIndex)>& moveCrosshairsToSegLabelCentroid,
  const std::function<std::optional<uuids::uuid>(
    const uuids::uuid& matchingImageUid, const std::string& segDisplayName
  )>& createBlankSeg,
  const std::function<bool(const uuids::uuid& segUid)>& clearSeg,
  const std::function<bool(const uuids::uuid& segUid)>& removeSeg,
  const AllViewsRecenterType& recenterAllViews
)
{
  static const std::string sk_addNewSegString = std::string(ICON_FK_FILE_O)
                                                + std::string(" Create");
  static const std::string sk_clearSegString = std::string(ICON_FK_ERASER) + std::string(" Clear");
  static const std::string sk_removeSegString = std::string(ICON_FK_TRASH_O)
                                                + std::string(" Remove");
  static const std::string sk_SaveSegString = std::string(ICON_FK_FLOPPY_O)
                                              + std::string(" Save...");

  if (!image)
  {
    spdlog::error("Null image");
    return;
  }

  const ImVec4* colors = ImGui::GetStyle().Colors;
  const ImVec4 activeColor = colors[ImGuiCol_ButtonActive];

  ImGuiTreeNodeFlags headerFlags = ImGuiTreeNodeFlags_CollapsingHeader;

  if (isActiveImage)
  {
    // Open header for the active image by default:
    headerFlags |= ImGuiTreeNodeFlags_DefaultOpen;
  }

  auto& imgSettings = image->settings();

  // Header is ID'ed only by the image index.
  // ### allows the header name to change without changing its ID.
  const std::string headerName = std::to_string(imageIndex) + ") " + imgSettings.displayName()
                                 + "###" + std::to_string(imageIndex);

  const auto headerColors = computeHeaderBgAndTextColors(imgSettings.borderColor());
  ImGui::PushStyleColor(ImGuiCol_Header, headerColors.first);
  ImGui::PushStyleColor(ImGuiCol_Text, headerColors.second);

  const bool open = ImGui::CollapsingHeader(headerName.c_str(), headerFlags);

  ImGui::PopStyleColor(2); // ImGuiCol_Header, ImGuiCol_Text

  if (!open)
  {
    return;
  }

  ImGui::Spacing();

  if (!isActiveImage)
  {
    if (ImGui::Button(ICON_FK_TOGGLE_OFF))
    {
      if (appData.setActiveImageUid(imageUid))
        return;
    }
    if (ImGui::IsItemHovered())
    {
      ImGui::SetTooltip("Make this the active image");
    }
  }
  else
  {
    ImGui::PushStyleColor(ImGuiCol_Button, activeColor);
    ImGui::Button(ICON_FK_TOGGLE_ON);
    ImGui::PopStyleColor(1); // ImGuiCol_Button
  }

  const bool isRef = (0 == imageIndex);

  ImGui::SameLine();

  if (isRef && isActiveImage)
  {
    ImGui::Text("%s", sk_referenceAndActiveImageMessage);
  }
  else if (isRef)
  {
    ImGui::Text("%s", sk_referenceImageMessage);
  }
  else if (isActiveImage)
  {
    ImGui::Text("%s", sk_activeImageMessage);
  }
  else
  {
    ImGui::Text("%s", sk_nonActiveImageMessage);
  }

  const auto segUids = appData.imageToSegUids(imageUid);
  if (segUids.empty())
  {
    ImGui::Text("This image has no segmentation");
    spdlog::error("Image {} has no segmentations", imageUid);
    return;
  }

  auto activeSegUid = appData.imageToActiveSegUid(imageUid);

  if (!activeSegUid)
  {
    spdlog::error("Image {} has no active segmentation", imageUid);
    return;
  }

  Image* activeSeg = appData.seg(*activeSegUid);

  if (!activeSeg)
  {
    spdlog::error("Active segmentation for image {} is null", imageUid);
    return;
  }

  ImGui::PushID(uuids::to_string(*activeSegUid).c_str()); /*** PushID activeSegUid ***/

  ImGui::Spacing();
  ImGui::Separator();
  ImGui::Spacing();

  ImGui::Text("Active segmentation:");

  //    ImGui::PushItemWidth(-1);
  if (ImGui::BeginCombo("Name", activeSeg->settings().displayName().c_str()))
  {
    size_t segIndex = 0;
    for (const auto& segUid : segUids)
    {
      ImGui::PushID(static_cast<int>(segIndex++));
      {
        if (Image* seg = appData.seg(segUid))
        {
          const bool isSelected = (segUid == *activeSegUid);

          if (ImGui::Selectable(seg->settings().displayName().c_str(), isSelected))
          {
            appData.assignActiveSegUidToImage(imageUid, segUid);
            activeSeg = appData.seg(segUid);
            updateImageUniforms();
          }

          if (isSelected)
            ImGui::SetItemDefaultFocus();
        }
      }
      ImGui::PopID(); // lmGroupIndex
    }
    ImGui::EndCombo();
  }
  //    ImGui::PopItemWidth();
  ImGui::SameLine();
  helpMarker("Select the active segmentation for this image");

  /// @todo Add button for copying the segmentation to a new segmentation

  // Add segmentation:
  if (ImGui::Button(sk_addNewSegString.c_str()))
  {
    const size_t numSegsForImage = appData.imageToSegUids(imageUid).size();

    std::string segDisplayName = std::string("Untitled segmentation ")
                                 + std::to_string(numSegsForImage + 1) + " for image '"
                                 + image->settings().displayName() + "'";

    if (createBlankSeg(imageUid, std::move(segDisplayName)))
    {
      updateImageUniforms();
    }
    else
    {
      spdlog::error("Error creating new blank segmentation for image {}", imageUid);
    }
  }
  if (ImGui::IsItemHovered())
  {
    ImGui::SetTooltip("Create a new blank segmentation for this image");
  }

  // Remove segmentation:
  // (Do not allow removal of the segmentation if it the only one for this image)
  if (appData.imageToSegUids(imageUid).size() > 1)
  {
    ImGui::SameLine();
    if (ImGui::Button(sk_removeSegString.c_str()))
    {
      if (removeSeg(*activeSegUid))
      {
        updateImageUniforms();
        ImGui::PopID(); /*** PopID activeSegUid ***/
        return;
      }
    }
    if (ImGui::IsItemHovered())
    {
      ImGui::SetTooltip("Remove this segmentation from the image");
    }
  }

  // Clear segmentation:
  ImGui::SameLine();
  if (ImGui::Button(sk_clearSegString.c_str()))
  {
    clearSeg(*activeSegUid);
  }
  if (ImGui::IsItemHovered())
  {
    ImGui::SetTooltip("Clear all values in this segmentation");
  }

  // Save segmentation:
  static const char* sk_dialogTitle("Select Segmentation Image");
  static const std::vector<std::string> sk_dialogFilters{};

  ImGui::SameLine();
  const auto selectedFile = ImGui::renderFileButtonDialogAndWindow(
    sk_SaveSegString.c_str(), sk_dialogTitle, sk_dialogFilters
  );

  if (ImGui::IsItemHovered())
  {
    ImGui::SetTooltip("Save the segmentation to an image file on disk");
  }

  if (selectedFile)
  {
    static constexpr uint32_t sk_compToSave = 0;

    if (activeSeg->saveComponentToDisk(sk_compToSave, *selectedFile))
    {
      spdlog::info("Saved segmentation image to file {}", *selectedFile);
      activeSeg->header().setFileName(*selectedFile);
    }
    else
    {
      spdlog::error("Error saving segmentation image to file {}", *selectedFile);
    }
  }

  ImGui::Spacing();
  ImGui::Separator();
  ImGui::Spacing();

  // Double check that we still have the active segmentation:
  if (!activeSeg)
  {
    spdlog::error("Active segmentation for image {} is null", imageUid);
    ImGui::PopID(); /*** PopID activeSegUid ***/
    return;
  }

  auto& segSettings = activeSeg->settings();

  // Header is ID'ed only by "seg_" and the image index.
  // ### allows the header name to change without changing its ID.
  const std::string segHeaderName = std::string("Seg: ") + segSettings.displayName() + "###seg_"
                                    + std::to_string(imageIndex);

  /// @todo add "*" to end of name and change color of seg header if seg has been modified

  // Open segmentation View Properties on first appearance
  ImGui::SetNextItemOpen(true, ImGuiCond_Appearing);

  if (ImGui::TreeNode("View Properties"))
  {
    // Visibility:
    bool segVisible = segSettings.visibility();
    if (ImGui::Checkbox("Visible", &segVisible))
    {
      segSettings.setVisibility(segVisible);
      updateImageUniforms();
    }
    ImGui::SameLine();
    helpMarker("Show/hide the segmentation on all views (S)");

    //        if (segVisible)
    {
      // Opacity (only shown if segmentation is visible):
      double segOpacity = segSettings.opacity();
      if (mySliderF64("Opacity", &segOpacity, 0.0, 1.0))
      {
        segSettings.setOpacity(segOpacity);
        updateImageUniforms();
      }
      ImGui::SameLine();
      helpMarker("Segmentation layer opacity");
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    ImGui::TreePop();
  }

  if (ImGui::TreeNode("Segmentation Labels"))
  {
    renderSegLabelsChildWindow(
      segSettings.labelTableIndex(),
      getLabelTable(segSettings.labelTableIndex()),
      updateLabelColorTableTexture,
      moveCrosshairsToSegLabelCentroid
    );

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    ImGui::TreePop();
  }

  if (ImGui::TreeNode("Header Information"))
  {
    renderImageHeaderInformation(appData, imageUid, *activeSeg, updateImageUniforms, recenterAllViews);

    ImGui::TreePop();
  }

  ImGui::Spacing();

  ImGui::PopID(); /*** PopID activeSegUid ***/
}

void renderLandmarkGroupHeader(
  AppData& appData,
  const uuids::uuid& imageUid,
  size_t imageIndex,
  bool isActiveImage,
  const AllViewsRecenterType& recenterAllViews
)
{
  static const char* sk_newLmGroupButtonText("Create new group of landmarks");
  static const char* sk_saveLmsButtonText("Save landmarks...");
  static const char* sk_saveLmsDialogTitle("Save Landmark Group");
  static const std::vector<std::string> sk_saveLmsDialogFilters{};

  Image* image = appData.image(imageUid);
  if (!image)
    return;

  auto addNewLmGroupButton = [&appData, &image, &imageUid]()
  {
    if (ImGui::Button(sk_newLmGroupButtonText))
    {
      LandmarkGroup newGroup;
      newGroup.setName(std::string("Landmarks for ") + image->settings().displayName());

      const auto newLmGroupUid = appData.addLandmarkGroup(std::move(newGroup));
      appData.assignLandmarkGroupUidToImage(imageUid, newLmGroupUid);
      appData.setRainbowColorsForAllLandmarkGroups();
      appData.assignActiveLandmarkGroupUidToImage(imageUid, newLmGroupUid);
    }
  };

  ImGuiTreeNodeFlags headerFlags = ImGuiTreeNodeFlags_CollapsingHeader;

  /// @todo This annoyingly pops up the active header each time... not sure why
  if (isActiveImage)
  {
    headerFlags |= ImGuiTreeNodeFlags_DefaultOpen;
  }

  ImGui::PushID(uuids::to_string(imageUid).c_str()); /** PushID imageUid **/

  // Header is ID'ed only by the image index.
  // ### allows the header name to change without changing its ID.
  const std::string headerName = std::to_string(imageIndex) + ") " + image->settings().displayName()
                                 + "###" + std::to_string(imageIndex);

  const auto imgSettings = image->settings();

  const auto headerColors = computeHeaderBgAndTextColors(imgSettings.borderColor());
  ImGui::PushStyleColor(ImGuiCol_Header, headerColors.first);
  ImGui::PushStyleColor(ImGuiCol_Text, headerColors.second);

  const bool open = ImGui::CollapsingHeader(headerName.c_str(), headerFlags);

  ImGui::PopStyleColor(2); // ImGuiCol_Header, ImGuiCol_Text

  if (!open)
  {
    ImGui::PopID(); // imageUid
    return;
  }

  ImGui::Spacing();

  const auto lmGroupUids = appData.imageToLandmarkGroupUids(imageUid);

  if (lmGroupUids.empty())
  {
    ImGui::Text("This image has no landmarks.");
    addNewLmGroupButton();
    ImGui::PopID(); // imageUid
    return;
  }

  // Show a combo box if there are multiple landmark groups
  const bool showLmGroupCombo = (lmGroupUids.size() > 1);

  std::optional<uuids::uuid> activeLmGroupUid = appData.imageToActiveLandmarkGroupUid(imageUid);

  // The default active landmark group is at index 0
  if (!activeLmGroupUid)
  {
    if (appData.assignActiveLandmarkGroupUidToImage(imageUid, lmGroupUids[0]))
    {
      activeLmGroupUid = appData.imageToActiveLandmarkGroupUid(imageUid);
    }
    else
    {
      spdlog::error("Unable to assign active landmark group {} to image {}", lmGroupUids[0], imageUid);
      ImGui::PopID(); // imageUid
      return;
    }
  }

  LandmarkGroup* activeLmGroup = appData.landmarkGroup(*activeLmGroupUid);

  if (!activeLmGroup)
  {
    spdlog::error("Landmark group {} for image {} is null", *activeLmGroupUid, imageUid);
    ImGui::PopID(); // imageUid
    return;
  }

  if (showLmGroupCombo)
  {
    if (ImGui::BeginCombo("Landmark group", activeLmGroup->getName().c_str()))
    {
      size_t lmGroupIndex = 0;
      for (const auto& lmGroupUid : lmGroupUids)
      {
        ImGui::PushID(static_cast<int>(lmGroupIndex++));

        if (LandmarkGroup* lmGroup = appData.landmarkGroup(lmGroupUid))
        {
          const bool isSelected = (lmGroupUid == *activeLmGroupUid);

          if (ImGui::Selectable(lmGroup->getName().c_str(), isSelected))
          {
            appData.assignActiveLandmarkGroupUidToImage(imageUid, lmGroupUid);
            activeLmGroup = appData.landmarkGroup(lmGroupUid);
          }

          if (isSelected)
            ImGui::SetItemDefaultFocus();
        }

        ImGui::PopID(); // lmGroupIndex
      }

      ImGui::EndCombo();
    }

    ImGui::SameLine();
    helpMarker("Select the group of landmarks to view");

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
  }

  if (!activeLmGroup)
  {
    spdlog::error("Active landmark group for image {} is null", imageUid);
    ImGui::PopID(); // imageUid
    return;
  }

  // Landmark group display name:
  std::string groupName = activeLmGroup->getName();
  if (ImGui::InputText("Name", &groupName))
  {
    activeLmGroup->setName(groupName);
  }
  ImGui::SameLine();
  helpMarker("Edit the name of the group of landmarks");

  // Landmark group file name:
  std::string fileName = activeLmGroup->getFileName().string();
  ImGui::InputText("File", &fileName, ImGuiInputTextFlags_ReadOnly);
  ImGui::SameLine();
  helpMarker("Comma-separated file with the landmarks");
  ImGui::Spacing();

  // Visibility checkbox:
  bool groupVisible = activeLmGroup->getVisibility();
  if (ImGui::Checkbox("Visible", &groupVisible))
  {
    activeLmGroup->setVisibility(groupVisible);
  }
  ImGui::SameLine();
  helpMarker("Show/hide the landmarks");

  // Opacity slider:
  float groupOpacity = activeLmGroup->getOpacity();
  if (mySliderF32("Opacity", &groupOpacity, 0.0f, 1.0f))
  {
    activeLmGroup->setOpacity(groupOpacity);
  }
  ImGui::SameLine();
  helpMarker("Landmark opacity");

  // Radius slider:
  float groupRadius = 100.0f * activeLmGroup->getRadiusFactor();
  if (mySliderF32("Radius", &groupRadius, 0.1f, 10.0f))
  {
    activeLmGroup->setRadiusFactor(groupRadius / 100.0f);
  }
  ImGui::SameLine();
  helpMarker("Landmark circle radius");
  ImGui::Spacing();

  // Rendering of landmark indices:
  bool renderLandmarkIndices = activeLmGroup->getRenderLandmarkIndices();
  if (ImGui::Checkbox("Show indices", &renderLandmarkIndices))
  {
    activeLmGroup->setRenderLandmarkIndices(renderLandmarkIndices);
  }
  ImGui::SameLine();
  helpMarker("Show/hide the landmark indices");

  // Rendering of landmark indices:
  bool renderLandmarkNames = activeLmGroup->getRenderLandmarkNames();
  if (ImGui::Checkbox("Show names", &renderLandmarkNames))
  {
    activeLmGroup->setRenderLandmarkNames(renderLandmarkNames);
  }
  ImGui::SameLine();
  helpMarker("Show/hide the landmark names");

  // Uniform color for all landmarks:
  bool hasGroupColor = activeLmGroup->getColorOverride();

  if (ImGui::Checkbox("Global color", &hasGroupColor))
  {
    activeLmGroup->setColorOverride(hasGroupColor);
  }

  if (hasGroupColor)
  {
    auto groupColor = activeLmGroup->getColor();

    ImGui::SameLine();
    if (ImGui::ColorEdit3("##uniformColor", glm::value_ptr(groupColor), sk_colorEditFlags))
    {
      activeLmGroup->setColor(groupColor);
    }
  }
  ImGui::SameLine();
  helpMarker("Set a global color for all landmarks in this group");

  // Text color for all landmarks:
  if (activeLmGroup->getTextColor().has_value())
  {
    auto textColor = activeLmGroup->getTextColor().value();
    if (ImGui::ColorEdit3("Text color", glm::value_ptr(textColor), sk_colorEditFlags))
    {
      activeLmGroup->setTextColor(textColor);
    }
    ImGui::SameLine();
    helpMarker("Set text color for all landmarks");
    ImGui::Spacing();
  }

  // Voxel vs physical space radio buttons:
  ImGui::Spacing();
  ImGui::Text("Landmark coordinate space:");
  int inVoxelSpace = activeLmGroup->getInVoxelSpace() ? 1 : 0;

  if (ImGui::RadioButton("Physical subject (mm)", &inVoxelSpace, 0))
  {
    activeLmGroup->setInVoxelSpace((1 == inVoxelSpace) ? true : false);
  }

  ImGui::SameLine();
  if (ImGui::RadioButton("Voxels", &inVoxelSpace, 1))
  {
    activeLmGroup->setInVoxelSpace((1 == inVoxelSpace) ? true : false);
  }

  ImGui::SameLine();
  helpMarker("Space in which landmark coordinates are defined");
  ImGui::Spacing();

  // Child window for points:
  ImGui::Dummy(ImVec2(0.0f, 4.0f));

  auto setWorldCrosshairsPos = [&appData](const glm::vec3& worldCrosshairsPos)
  { appData.state().setWorldCrosshairsPos(worldCrosshairsPos); };

  renderLandmarkChildWindow(
    appData,
    image->transformations(),
    activeLmGroup,
    appData.state().worldCrosshairs().worldOrigin(),
    setWorldCrosshairsPos,
    recenterAllViews
  );

  ImGui::Spacing();
  ImGui::Separator();
  ImGui::Spacing();

  addNewLmGroupButton();

  // Save landmarks to CSV and save settings to project file:
  const auto selectedFile = ImGui::renderFileButtonDialogAndWindow(
    sk_saveLmsButtonText, sk_saveLmsDialogTitle, sk_saveLmsDialogFilters
  );

  ImGui::SameLine();
  helpMarker("Save the landmarks to a CSV file");

  if (selectedFile)
  {
    if (serialize::saveLandmarkGroupCsvFile(activeLmGroup->getPoints(), *selectedFile))
    {
      spdlog::info("Saved landmarks to CSV file {}", *selectedFile);

      /// @todo How to handle changing the file name?
      activeLmGroup->setFileName(*selectedFile);
    }
    else
    {
      spdlog::error("Error saving landmarks to CSV file {}", *selectedFile);
    }
  }

  ImGui::Spacing();

  ImGui::PopID(); /** PopID imageUid **/
}

void renderAnnotationsHeader(
  AppData& appData,
  const uuids::uuid& imageUid,
  size_t imageIndex,
  bool isActiveImage,
  const std::function<void(const uuids::uuid& viewUid, const glm::vec3& worldFwdDirection)>&
    setViewDirection,
  const std::function<void()>& paintActiveSegmentationWithActivePolygon,
  const AllViewsRecenterType& recenterAllViews
)
{
  static constexpr bool sk_doNotRecenterCrosshairs = false;
  static constexpr bool sk_doNotRecenterOnCurrentCrosshairsPosition = false;
  static constexpr bool sk_doNotResetObliqueOrientation = false;
  static constexpr bool sk_doNotResetZoom = false;

  static constexpr size_t sk_minNumLines = 6;
  static constexpr size_t sk_maxNumLines = 12;

  static const ImGuiColorEditFlags sk_annotColorEditFlags = ImGuiColorEditFlags_PickerHueBar
                                                            | ImGuiColorEditFlags_DisplayRGB
                                                            | ImGuiColorEditFlags_DisplayHex
                                                            | ImGuiColorEditFlags_AlphaBar
                                                            | ImGuiColorEditFlags_AlphaPreviewHalf
                                                            | ImGuiColorEditFlags_Uint8
                                                            | ImGuiColorEditFlags_InputRGB;

  static const std::string sk_saveAnnotButtonText = std::string(ICON_FK_FLOPPY_O)
                                                    + std::string(" Save all...");

  static const std::string sk_removeAnnotButtonText = std::string(ICON_FK_TRASH_O)
                                                      + std::string(" Remove");

  static const std::string sk_fillAnnotButtonText = std::string(ICON_FK_PAINT_BRUSH)
                                                    + std::string(" Fill segmentation");

  static const char* sk_saveAnnotDialogTitle("Save Annotations to JSON");
  static const std::vector<std::string> sk_saveAnnotDialogFilters{};

  Image* image = appData.image(imageUid);
  if (!image)
    return;

  auto moveCrosshairsToAnnotationCenter = [&appData, &image](const Annotation& annot)
  {
    const glm::vec4 subjectCentroid{
      annot.unprojectFromAnnotationPlaneToSubjectPoint(annot.polygon().getCentroid()), 1.0f
    };
    const glm::vec4 worldCentroid = image->transformations().worldDef_T_subject() * subjectCentroid;
    appData.state().setWorldCrosshairsPos(glm::vec3{worldCentroid / worldCentroid.w});
  };

  // Finds a view with normal vector maching the annotation plane. (Todo: make this view active.)
  // If none found, make the largest view oblique and align it to the annotation.
  auto alignViewToAnnotationPlane =
    [&appData, &imageUid, &image, &setViewDirection](const Annotation& annot)
  {
    const glm::mat3 world_T_subject_invTranspose = glm::inverseTranspose(
      glm::mat3{image->transformations().worldDef_T_subject()}
    );
    const glm::vec3 worldAnnotNormal = glm::normalize(
      world_T_subject_invTranspose * glm::vec3{annot.getSubjectPlaneEquation()}
    );

    // Does the current layout have a view with this orientaion?
    const auto viewsWithNormal = appData.windowData().findCurrentViewsWithNormal(worldAnnotNormal);

    if (viewsWithNormal.empty())
    {
      const uuids::uuid largestCurrentViewUid = appData.windowData().findLargestCurrentView();

      if (View* view = appData.windowData().getCurrentView(largestCurrentViewUid))
      {
        // Rather than check if the plane of the annotation (which, recall is defined in
        // Subject space) is aligned with either an axial, coronal, or sagittal view,
        // we simple set the view to oblique.
        view->setViewType(ViewType::Oblique);
        setViewDirection(largestCurrentViewUid, worldAnnotNormal);

        // Render the image in this view if not currently rendered:
        if (!view->isImageRendered(imageUid))
        {
          view->setImageRendered(appData, imageUid, true);
        }

        spdlog::trace(
          "Changed view {} normal direction to {}",
          largestCurrentViewUid,
          glm::to_string(worldAnnotNormal)
        );
      }
      else
      {
        spdlog::error("Unable to orient a view to the annotation plane");
      }
    }
  };

  ImGuiTreeNodeFlags headerFlags = ImGuiTreeNodeFlags_CollapsingHeader;

  /// @todo This annoyingly pops up the active header each time... not sure why
  if (isActiveImage)
  {
    headerFlags |= ImGuiTreeNodeFlags_DefaultOpen;
  }

  ImGui::PushID(uuids::to_string(imageUid).c_str()); /** PushID imageUid **/

  // Header is ID'ed only by the image index.
  // ### allows the header name to change without changing its ID.
  const std::string headerName = std::to_string(imageIndex) + ") " + image->settings().displayName()
                                 + "###" + std::to_string(imageIndex);

  const auto headerColors = computeHeaderBgAndTextColors(image->settings().borderColor());
  ImGui::PushStyleColor(ImGuiCol_Header, headerColors.first);
  ImGui::PushStyleColor(ImGuiCol_Text, headerColors.second);

  const bool open = ImGui::CollapsingHeader(headerName.c_str(), headerFlags);

  ImGui::PopStyleColor(2); // ImGuiCol_Header, ImGuiCol_Text

  if (!open)
  {
    ImGui::PopID(); // imageUid
    return;
  }

  ImGui::Spacing();

  const auto& annotUids = appData.annotationsForImage(imageUid);
  if (annotUids.empty())
  {
    ImGui::Text("This image has no annotations.");
    ImGui::PopID(); // imageUid
    return;
  }

  auto activeAnnotUid = appData.imageToActiveAnnotationUid(imageUid);

  const ImVec4* colors = ImGui::GetStyle().Colors;
  ImGui::PushStyleColor(ImGuiCol_Header, colors[ImGuiCol_ButtonActive]);

  const size_t numLines = std::max(std::min(annotUids.size(), sk_maxNumLines), sk_minNumLines);

  /// @todo Change this into a child window, like for Landmarks.
  /// then do ImGui::SetScrollHereY(1.0f); to put activeAnnot at bottom

  const float listBoxWidth = (activeAnnotUid ? -FLT_MIN : 250.0f);
  const float listBoxHeight = static_cast<float>(numLines) * ImGui::GetTextLineHeightWithSpacing();

  if (ImGui::BeginListBox("##annotList", ImVec2(listBoxWidth, listBoxHeight)))
  {
    size_t annotIndex = 0;
    for (const auto& annotUid : annotUids)
    {
      ImGui::PushID(static_cast<int>(annotIndex++));

      Annotation* annot = appData.annotation(annotUid);
      if (!annot)
      {
        spdlog::error("Null annotation {}", annotUid);
        ImGui::PopID(); // lmGroupIndex
      }

      /// @see Line 2791 of demo:
      /// ImGui::SetScrollHereY(i * 0.25f); // 0.0f:top, 0.5f:center, 1.0f:bottom

      const std::string text = annot->getDisplayName() + " ["
                               + data::getAnnotationSubjectPlaneName(*annot) + "]";

      const bool isSelected = (activeAnnotUid && (annotUid == *activeAnnotUid));

      if (ImGui::Selectable(text.c_str(), isSelected))
      {
        // Make the annotation active and move crosshairs to it:
        if (!appData.assignActiveAnnotationUidToImage(imageUid, annotUid))
        {
          spdlog::error("Unable to assign active annotation {} to image {}", annotUid, imageUid);
        }

        // Need to synchronize the active annotation change with the highlighting
        // state of annotations.
        ASM::synchronizeAnnotationHighlights();

        if (const Annotation* activeAnnot = appData.annotation(annotUid))
        {
          moveCrosshairsToAnnotationCenter(*activeAnnot);
          alignViewToAnnotationPlane(*activeAnnot);

          recenterAllViews(
            sk_doNotRecenterCrosshairs,
            sk_doNotRecenterOnCurrentCrosshairsPosition,
            sk_doNotResetObliqueOrientation,
            sk_doNotResetZoom
          );
        }
        else
        {
          spdlog::error("Null active annotation {}", annotUid);
        }
      }

      // Set the initial focus when opening the combo (scrolling + keyboard navigation focus)
      if (isSelected)
        ImGui::SetItemDefaultFocus();

      ImGui::PopID(); // lmGroupIndex
    }

    ImGui::EndListBox();
  }
  ImGui::PopStyleColor(1); // ImGuiCol_Header

  activeAnnotUid = appData.imageToActiveAnnotationUid(imageUid);
  if (!activeAnnotUid)
  {
    // If there is no active/selected annotation, then do not render the rest of the header,
    // which shows view properites of the annotation
    return;
  }

  Annotation* activeAnnot = appData.annotation(*activeAnnotUid);
  if (!activeAnnot)
  {
    spdlog::error("Null active annotation {}", *activeAnnotUid);
    return;
  }

  // Annotation display name:
  std::string displayName = activeAnnot->getDisplayName();
  if (ImGui::InputText("Name", &displayName))
  {
    activeAnnot->setDisplayName(displayName);
  }
  ImGui::SameLine();
  helpMarker("Edit the name of the annotation");

  ImGui::Text("Layer order: ");

  ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, 0.0f));

  ImGui::SameLine();
  if (ImGui::Button(ICON_FK_FAST_BACKWARD))
  {
    appData.moveAnnotationToBack(imageUid, *activeAnnotUid);
  }
  if (ImGui::IsItemHovered())
  {
    ImGui::SetTooltip("Move annotation to backmost layer");
  }

  ImGui::SameLine();
  if (ImGui::Button(ICON_FK_BACKWARD))
  {
    appData.moveAnnotationBackwards(imageUid, *activeAnnotUid);
  }
  if (ImGui::IsItemHovered())
  {
    ImGui::SetTooltip("Move annotation backward in layers (decrease the annotation order)");
  }

  ImGui::SameLine();
  if (ImGui::Button(ICON_FK_FORWARD))
  {
    appData.moveAnnotationForwards(imageUid, *activeAnnotUid);
  }
  if (ImGui::IsItemHovered())
  {
    ImGui::SetTooltip("Move annotation forward in layers (increase the annotation order)");
  }

  ImGui::SameLine();
  if (ImGui::Button(ICON_FK_FAST_FORWARD))
  {
    appData.moveAnnotationToFront(imageUid, *activeAnnotUid);
  }
  if (ImGui::IsItemHovered())
  {
    ImGui::SetTooltip("Move annotation to frontmost layer");
  }

  /*** ImGuiStyleVar_ItemSpacing ***/
  ImGui::PopStyleVar(1);

  // Remove the annotation:
  bool removeAnnot = false;
  static bool doNotAskAagain = false;

  ImGui::Spacing();
  const bool clickedRemoveButton = ImGui::Button(sk_removeAnnotButtonText.c_str());
  if (ImGui::IsItemHovered())
  {
    ImGui::SetTooltip("Remove the annotation. (The saved file on disk will not be deleted.)");
  }

  if (clickedRemoveButton)
  {
    if (!doNotAskAagain && !ImGui::IsPopupOpen("Remove Annotation"))
    {
      ImGui::OpenPopup("Remove Annotation", ImGuiWindowFlags_AlwaysAutoResize);
    }
    else if (doNotAskAagain)
    {
      removeAnnot = true;
    }
  }

  // Fill the active segmentation with the annotation:
  if (activeAnnot->isClosed() && !activeAnnot->isSmoothed())
  {
    ImGui::SameLine();
    if (ImGui::Button(sk_fillAnnotButtonText.c_str()))
    {
      if (paintActiveSegmentationWithActivePolygon)
      {
        paintActiveSegmentationWithActivePolygon();
      }
    }
    if (ImGui::IsItemHovered())
    {
      ImGui::SetTooltip("Fill the active image segmentation with the selected annotation polygon");
    }
  }

  const ImVec2 center(ImGui::GetIO().DisplaySize.x * 0.5f, ImGui::GetIO().DisplaySize.y * 0.5f);

  ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

  if (ImGui::BeginPopupModal("Remove Annotation", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
  {
    ImGui::Text(
      "Are you sure that you want to remove annotation '%s'?", activeAnnot->getDisplayName().c_str()
    );
    ImGui::Separator();

    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));
    ImGui::Checkbox("Do not ask again", &doNotAskAagain);
    ImGui::PopStyleVar();

    if (ImGui::Button("Yes", ImVec2(80, 0)))
    {
      removeAnnot = true;
      ImGui::CloseCurrentPopup();
    }
    ImGui::SetItemDefaultFocus();

    ImGui::SameLine();
    if (ImGui::Button("No", ImVec2(80, 0)))
    {
      removeAnnot = false;
      ImGui::CloseCurrentPopup();
    }

    ImGui::EndPopup();
  }

  if (removeAnnot)
  {
    if (appData.removeAnnotation(*activeAnnotUid))
    {
      spdlog::info("Removed annotation {}", *activeAnnotUid);
      return;
    }
    else
    {
      spdlog::error("Unable to remove annotation {}", *activeAnnotUid);
    }
  }

  ImGui::Spacing();
  ImGui::Separator();
  ImGui::Spacing();

  // Boundary:
  bool isClosed = activeAnnot->isClosed();
  if (ImGui::RadioButton("Open", !isClosed))
  {
    activeAnnot->setClosed(false);
  }

  ImGui::SameLine();
  if (ImGui::RadioButton("Closed boundary", isClosed))
  {
    activeAnnot->setClosed(true);
  }
  ImGui::SameLine();
  helpMarker("Set whether the annotation polygon boundary is open or closed");

  // Smoothing:
  bool smooth = activeAnnot->isSmoothed();
  if (ImGui::Checkbox("Smooth", &smooth))
  {
    activeAnnot->setSmoothed(smooth);
  }
  ImGui::SameLine();
  helpMarker("Smooth the annotation boundary");

  if (activeAnnot->isSmoothed())
  {
    float smoothing = activeAnnot->getSmoothingFactor();
    if (mySliderF32("Smoothing", &smoothing, 0.0f, 0.2f, "%0.2f"))
    {
      activeAnnot->setSmoothingFactor(smoothing);
    }
    ImGui::SameLine();
    helpMarker("Smoothing factor");
  }

  ImGui::Spacing();
  ImGui::Separator();
  ImGui::Spacing();

  // Visibility checkbox:
  bool annotVisible = activeAnnot->isVisible();
  if (ImGui::Checkbox("Visible", &annotVisible))
  {
    activeAnnot->setVisible(annotVisible);
  }
  ImGui::SameLine();
  helpMarker("Show/hide the annotation");

  // Show vertices checkbox:
  if (!appData.renderData().m_globalAnnotationParams.hidePolygonVertices)
  {
    bool showVertices = activeAnnot->getVertexVisibility();
    if (ImGui::Checkbox("Show vertices", &showVertices))
    {
      activeAnnot->setVertexVisibility(showVertices);
    }
    ImGui::SameLine();
    helpMarker("Show/hide the annotation vertices");
  }

  // Filled checkbox:
  if (activeAnnot->isClosed())
  {
    bool filled = activeAnnot->isFilled();
    if (ImGui::Checkbox("Filled", &filled))
    {
      activeAnnot->setFilled(filled);
    }
    ImGui::SameLine();
    helpMarker("Fill the annotation interior");
  }

  // Opacity slider:
  float annotOpacity = activeAnnot->getOpacity();
  if (mySliderF32("Opacity", &annotOpacity, 0.0f, 1.0f))
  {
    activeAnnot->setOpacity(annotOpacity);
  }
  ImGui::SameLine();
  helpMarker("Overall annotation opacity");

  // Line stroke thickness:
  float annotThickness = activeAnnot->getLineThickness();
  if (ImGui::InputFloat("Line thickness", &annotThickness, 0.1f, 1.0f, "%0.2f"))
  {
    if (annotThickness >= 0.0f)
    {
      activeAnnot->setLineThickness(annotThickness);
    }
  }
  ImGui::SameLine();
  helpMarker("Annotation line thickness");

  // Line color:
  glm::vec4 annotLineColor = activeAnnot->getLineColor();
  if (ImGui::ColorEdit4("Line color", glm::value_ptr(annotLineColor), sk_annotColorEditFlags))
  {
    activeAnnot->setLineColor(annotLineColor);
    activeAnnot->setVertexColor(annotLineColor);
  }
  ImGui::SameLine();
  helpMarker("Annotation line color");

  const bool showFillColorButton = (activeAnnot->isClosed() && activeAnnot->isFilled());

  if (showFillColorButton)
  {
    ImGui::SameLine();
    if (ImGui::Button(ICON_FK_LEVEL_DOWN))
    {
      glm::vec4 fillColor{annotLineColor};
      fillColor.a = activeAnnot->getFillColor().a;
      activeAnnot->setFillColor(fillColor);
    }
    if (ImGui::IsItemHovered())
    {
      ImGui::SetTooltip("Match fill color to line color");
    }

    // Fill color:
    glm::vec4 annotFillColor = activeAnnot->getFillColor();
    if (ImGui::ColorEdit4("Fill color", glm::value_ptr(annotFillColor), sk_annotColorEditFlags))
    {
      activeAnnot->setFillColor(annotFillColor);
    }
    ImGui::SameLine();
    helpMarker("Annotation fill color");

    ImGui::SameLine();
    if (ImGui::Button(ICON_FK_LEVEL_UP))
    {
      glm::vec4 lineColor{annotFillColor};
      lineColor.a = activeAnnot->getLineColor().a;
      activeAnnot->setLineColor(lineColor);
    }
    if (ImGui::IsItemHovered())
    {
      ImGui::SetTooltip("Match line color to fill color");
    }
  }

  ImGui::Spacing();
  ImGui::Separator();
  ImGui::Spacing();

  // Plane normal vector and offset:
  ImGui::Text("Annotation plane (Subject space):");

  const char* coordFormat = appData.guiData().m_coordsPrecisionFormat.c_str();

  glm::vec4 annotPlaneEq = activeAnnot->getSubjectPlaneEquation();
  ImGui::InputFloat3("Normal", glm::value_ptr(annotPlaneEq), coordFormat);
  ImGui::SameLine();
  helpMarker("Annotation plane normal vector (x, y, z) in image Subject space");

  ImGui::InputFloat("Offset (mm)", &annotPlaneEq[3], 0.0f, 0.0f, coordFormat);
  ImGui::SameLine();
  helpMarker("Offset distance (mm) of annotation plane from the image Subject space origin");

  // Number of vertices
  static constexpr size_t OUTER_BOUNDARY = 0;
  if (activeAnnot->polygon().numBoundaries() > 0)
  {
    ImGui::Spacing();
    ImGui::Text(
      "Polygon has %ld vertices", activeAnnot->polygon().getBoundaryVertices(OUTER_BOUNDARY).size()
    );
  }

  ImGui::Spacing();
  ImGui::Separator();
  ImGui::Spacing();

  // Annotation file name:
  std::string fileName = activeAnnot->getFileName().string();
  ImGui::InputText("File", &fileName, ImGuiInputTextFlags_ReadOnly);
  ImGui::SameLine();
  helpMarker("File storing the annotation");

  if (!annotUids.empty())
  {
    // Save annotations to disk:
    const auto selectedFile = ImGui::renderFileButtonDialogAndWindow(
      sk_saveAnnotButtonText.c_str(), sk_saveAnnotDialogTitle, sk_saveAnnotDialogFilters
    );

    if (ImGui::IsItemHovered())
      ImGui::SetTooltip("Save annotations to disk");

    if (selectedFile)
    {
      // Add all annotations belonging to this image to a json structure,
      // then save the json to disk.
      nlohmann::json j;

      for (const auto& annotUid : appData.annotationsForImage(imageUid))
      {
        if (const Annotation* annot = appData.annotation(annotUid))
        {
          serialize::appendAnnotationToJson(*annot, j);
        }
      }

      if (serialize::saveToJsonFile(j, *selectedFile))
      {
        spdlog::info("Saved annotations for image {} to JSON file {}", imageUid, *selectedFile);
        activeAnnot->setFileName(*selectedFile);
      }
      else
      {
        spdlog::error("Error saving annotation to SVG file {}", *selectedFile);
      }
    }
  }

  ImGui::PopID(); /** PopID imageUid **/
}
