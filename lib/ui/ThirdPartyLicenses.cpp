#include "ui/ThirdPartyLicenses.h"

#include <cmrc/cmrc.hpp>
#include <imgui/imgui.h>

#include <algorithm>
#include <array>
#include <map>
#include <stdexcept>
#include <string>
#include <string_view>

CMRC_DECLARE(licenses);

namespace
{
thread_local std::map<std::string, std::string> g_licenseCache;

struct LicenseDocument
{
  const char* title;
  const char* resourcePath;
};

struct LicenseEntry
{
  const char* name;
  const char* version;
  const char* source;
  const char* license;
  const char* note;
  const LicenseDocument* documents;
  std::size_t numDocuments;
};

template<std::size_t N>
constexpr std::size_t count(const LicenseDocument (&)[N])
{
  return N;
}

static const LicenseDocument sk_cli11Docs[] = {{"License text:", "licenses/CLI11.txt"}};
static const LicenseDocument sk_cmakercDocs[] = {{"License text:", "licenses/CMakeRC.txt"}};
static const LicenseDocument sk_glfwDocs[] = {{"License text:", "licenses/GLFW.txt"}};
static const LicenseDocument sk_glmDocs[] = {{"License text:", "licenses/GLM.txt"}};
static const LicenseDocument sk_iconFontDocs[] = {{"License text:", "licenses/IconFontCppHeaders.txt"}};
static const LicenseDocument sk_imguiDocs[] = {{"License text:", "licenses/DearImGui.txt"}};
static const LicenseDocument sk_implotDocs[] = {{"License text:", "licenses/ImPlot.txt"}};
static const LicenseDocument sk_itkDocs[] = {
  {"License text:", "licenses/Apache-2.0.txt"},
  {"Notice", "licenses/ITK-NOTICE.txt"}};
static const LicenseDocument sk_nanovgDocs[] = {{"License text:", "licenses/NanoVG.txt"}};
static const LicenseDocument sk_nfdDocs[] = {{"License text:", "licenses/NativeFileDialogExtended.txt"}};
static const LicenseDocument sk_jsonDocs[] = {{"License text:", "licenses/nlohmann-json.txt"}};
static const LicenseDocument sk_qtDocs[] = {{"License text:", "licenses/Qt-LGPL-3.0-only.txt"}};
static const LicenseDocument sk_spdlogDocs[] = {{"License text:", "licenses/spdlog.txt"}};
static const LicenseDocument sk_stduuidDocs[] = {{"License text:", "licenses/stduuid.txt"}};
static const LicenseDocument sk_tinyfsmDocs[] = {{"License text:", "licenses/TinyFSM.txt"}};
static const LicenseDocument sk_vtkDocs[] = {{"Copyright and license text:", "licenses/VTK.txt"}};
static const LicenseDocument sk_gladDocs[] = {{"License text:", "licenses/GLAD.txt"}};
static const LicenseDocument sk_imguizmoDocs[] = {{"License text:", "licenses/imGuIZMO.quat.txt"}};
static const LicenseDocument sk_imguiKnobsDocs[] = {{"License text:", "licenses/imgui-knobs.txt"}};
static const LicenseDocument sk_pnpolyDocs[] = {{"License text:", "licenses/PNPOLY.txt"}};
static const LicenseDocument sk_safeclibDocs[] = {{"License text:", "licenses/SafeCLib.txt"}};
static const LicenseDocument sk_tdigestDocs[] = {{"License text:", "licenses/Apache-2.0.txt"}};
static const LicenseDocument sk_cubicDocs[] = {
  {"License and citation request", "licenses/CubicBSplineInterpolation.txt"}};
static const LicenseDocument sk_cousineDocs[] = {{"License text:", "licenses/Cousine.txt"}};
static const LicenseDocument sk_forkAwesomeDocs[] = {{"Licenses", "licenses/ForkAwesome.txt"}};
static const LicenseDocument sk_ibmPlexSansDocs[] = {{"License text:", "licenses/IBMPlexSans.txt"}};
static const LicenseDocument sk_interDocs[] = {{"License text:", "licenses/Inter.txt"}};
static const LicenseDocument sk_notoSansDocs[] = {{"License text:", "licenses/NotoSans.txt"}};
static const LicenseDocument sk_robotoDocs[] = {{"License text:", "licenses/Roboto.txt"}};
static const LicenseDocument sk_satoshiDocs[] = {{"License text:", "licenses/Satoshi.txt"}};
static const LicenseDocument sk_sourceSans3Docs[] = {{"License text:", "licenses/SourceSans3.txt"}};
static const LicenseDocument sk_spaceGroteskDocs[] = {{"License text:", "licenses/SpaceGrotesk.txt"}};
static const LicenseDocument sk_supremeDocs[] = {{"License text:", "licenses/Supreme.txt"}};
static const LicenseDocument sk_peterKovesiDocs[] = {{"License text:", "licenses/PeterKovesiColourMaps.txt"}};
static const LicenseDocument sk_matplotlibDocs[] = {
  {"Attribution", "licenses/MatplotlibColourMaps.txt"},
  {"CC0-1.0", "licenses/CC0-1.0.txt"}};
static const LicenseDocument sk_cividisDocs[] = {{"License text:", "licenses/Cividis.txt"}};

static const LicenseEntry sk_entries[] = {
  {"Cividis color map",
   "None",
   "https://www.ncl.ucar.edu/Document/Graphics/ColorTables/cividis.shtml",
   "BSD-3-Clause",
   "",
   sk_cividisDocs,
   count(sk_cividisDocs)},
  {"CLI11", "v2.6.2", "https://github.com/CLIUtils/CLI11", "BSD-3-Clause", "", sk_cli11Docs, count(sk_cli11Docs)},
  {"CMakeRC", "2.0.1", "https://github.com/vector-of-bool/cmrc", "MIT", "", sk_cmakercDocs, count(sk_cmakercDocs)},
  {"Cousine font",
   "None",
   "https://fonts.google.com/specimen/Cousine",
   "Apache License 2.0",
   "",
   sk_cousineDocs,
   count(sk_cousineDocs)},
  {"CUDA Cubic B-Spline Interpolation",
   "None",
   "http://www.dannyruijters.nl/cubicinterpolation/",
   "BSD-3-Clause-style license",
   "The upstream notice requests scientific citation when used in scientific projects.",
   sk_cubicDocs,
   count(sk_cubicDocs)},
  {"Dear ImGui",
   "v1.92.8-docking",
   "https://github.com/ocornut/imgui/tree/v1.92.8-docking",
   "MIT",
   "Docking branch.",
   sk_imguiDocs,
   count(sk_imguiDocs)},
  {"Fork Awesome font",
   "None",
   "https://forkaweso.me/Fork-Awesome/",
   "SIL Open Font License 1.1, MIT, and CC-BY-3.0",
   "",
   sk_forkAwesomeDocs,
   count(sk_forkAwesomeDocs)},
  {"GLAD OpenGL loaders",
   "Generated by glad 0.1.33",
   "https://github.com/Dav1dde/glad",
   "MIT and Khronos MIT-style license",
   "",
   sk_gladDocs,
   count(sk_gladDocs)},
  {"GLFW", "3.4", "https://github.com/glfw/glfw", "zlib/libpng", "", sk_glfwDocs, count(sk_glfwDocs)},
  {"GLM", "1.0.3", "https://github.com/g-truc/glm", "Happy Bunny License or MIT", "", sk_glmDocs, count(sk_glmDocs)},
  {"IconFontCppHeaders",
   "210b5a3",
   "https://github.com/juliettef/IconFontCppHeaders",
   "zlib",
   "",
   sk_iconFontDocs,
   count(sk_iconFontDocs)},
  {"IBM Plex Sans font",
   "None",
   "https://github.com/IBM/plex",
   "SIL Open Font License 1.1",
   "",
   sk_ibmPlexSansDocs,
   count(sk_ibmPlexSansDocs)},
  {"imGuIZMO.quat",
   "v3.0",
   "https://github.com/BrutPitt/imGuIZMO.quat",
   "BSD-2-Clause",
   "Includes the Entropy palette button adaptation.",
   sk_imguizmoDocs,
   count(sk_imguizmoDocs)},
  {"imgui-knobs",
   "None",
   "https://github.com/altschuler/imgui-knobs",
   "MIT",
   "",
   sk_imguiKnobsDocs,
   count(sk_imguiKnobsDocs)},
  {"ImPlot", "v0.17", "https://github.com/epezent/implot", "MIT", "", sk_implotDocs, count(sk_implotDocs)},
  {"Insight Toolkit (ITK)",
   "v5.4.3",
   "https://github.com/InsightSoftwareConsortium/ITK",
   "Apache License 2.0",
   "Enabled ITK modules: ITKCommon, ITKIOImageBase, ITKIONIFTI, ITKIONRRD, ITKIOMeta, ITKIOGDCM, ITKIOJPEG, "
   "ITKIOPNG, ITKIOTIFF, ITKIOBMP, ITKImageFilterBase, ITKImageIntensity, ITKImageStatistics, ITKThresholding, "
   "ITKDistanceMap, ITKDisplacementField, ITKImageGrid, ITKImageFunction, and ITKStatistics. Compiled ITK libraries, "
   "including transitive dependencies: ITKCommon, ITKConvolution, ITKFFT, ITKIOBMP, ITKIOGDCM, ITKIOImageBase, "
   "ITKIOJPEG, ITKIOMeta, ITKIONIFTI, ITKIONRRD, ITKIOPNG, ITKIOTIFF, ITKImageIntensity, ITKLabelMap, "
   "ITKMathematicalMorphology, ITKMesh, ITKMetaIO, ITKNrrdIO, ITKPath, ITKSmoothing, ITKSpatialObjects, "
   "ITKStatistics, ITKTransform, and ITKVNLInstantiation. ITKVTK and all other optional ITK modules are disabled. "
   "Includes upstream NOTICE text.",
   sk_itkDocs,
   count(sk_itkDocs)},
  {"Inter font",
   "None",
   "https://github.com/rsms/inter",
   "SIL Open Font License 1.1",
   "",
   sk_interDocs,
   count(sk_interDocs)},
  {"Matplotlib viridis, plasma, inferno, and magma color maps",
   "None",
   "https://bids.github.io/colormap/",
   "CC0-1.0",
   "",
   sk_matplotlibDocs,
   count(sk_matplotlibDocs)},
  {"NanoVG", "ce3bf74", "https://github.com/memononen/nanovg", "zlib", "", sk_nanovgDocs, count(sk_nanovgDocs)},
  {"Native File Dialog Extended",
   "v1.3.0",
   "https://github.com/btzy/nativefiledialog-extended",
   "zlib",
   "",
   sk_nfdDocs,
   count(sk_nfdDocs)},
  {"Noto Sans font",
   "None",
   "https://github.com/notofonts/noto-fonts",
   "SIL Open Font License 1.1",
   "",
   sk_notoSansDocs,
   count(sk_notoSansDocs)},
  {"nlohmann::json", "v3.12.0", "https://github.com/nlohmann/json", "MIT", "", sk_jsonDocs, count(sk_jsonDocs)},
  {"Peter Kovesi color maps",
   "2014-2018 copy",
   "https://peterkovesi.com/projects/colourmaps",
   "Creative Commons Attribution 4.0",
   "Attribution text is included in the embedded notice.",
   sk_peterKovesiDocs,
   count(sk_peterKovesiDocs)},
  {"PNPOLY point-in-polygon test",
   "1970-2003 copy",
   "https://wrf.ecse.rpi.edu/Research/Short_Notes/pnpoly.html",
   "BSD-style permissive license",
   "Binary redistribution requires reproducing the copyright notice, conditions, disclaimers, and non-endorsement "
   "term.",
   sk_pnpolyDocs,
   count(sk_pnpolyDocs)},
  {"Qt Base / Qt Core",
   "6.8.1",
   "https://www.qt.io/product/qt6",
   "LGPL-3.0-only",
   "Qt Base is built without GUI, Widgets, OpenGL, D-Bus, OpenSSL, or ICU support. The resulting Qt modules are "
   "Core, Concurrent, Network, SQL, Test, and XML. Entropy links only Qt Core as a dynamically linked shared library.",
   sk_qtDocs,
   count(sk_qtDocs)},
  {"Roboto fonts",
   "None",
   "https://fonts.google.com/specimen/Roboto",
   "Apache License 2.0",
   "",
   sk_robotoDocs,
   count(sk_robotoDocs)},
  {"Safe C Library strerrorlen_s fallback",
   "2012-2017 copy",
   "https://github.com/rurban/safeclib",
   "MIT",
   "",
   sk_safeclibDocs,
   count(sk_safeclibDocs)},
  {"Satoshi font",
   "None",
   "https://www.fontshare.com/fonts/satoshi",
   "Fontshare Free Font License",
   "",
   sk_satoshiDocs,
   count(sk_satoshiDocs)},
  {"Source Sans 3 font",
   "None",
   "https://github.com/adobe-fonts/source-sans",
   "SIL Open Font License 1.1",
   "",
   sk_sourceSans3Docs,
   count(sk_sourceSans3Docs)},
  {"Space Grotesk font",
   "None",
   "https://github.com/floriankarsten/space-grotesk",
   "SIL Open Font License 1.1",
   "",
   sk_spaceGroteskDocs,
   count(sk_spaceGroteskDocs)},
  {"spdlog", "v1.17.0", "https://github.com/gabime/spdlog", "MIT", "", sk_spdlogDocs, count(sk_spdlogDocs)},
  {"stduuid", "v1.2.3", "https://github.com/mariusbancila/stduuid", "MIT", "", sk_stduuidDocs, count(sk_stduuidDocs)},
  {"Supreme font",
   "None",
   "https://www.fontshare.com/fonts/supreme",
   "Fontshare Free Font License",
   "",
   sk_supremeDocs,
   count(sk_supremeDocs)},
  {"T-Digest for C++",
   "None",
   "https://github.com/derrickburns/tdigest",
   "Apache License 2.0",
   "",
   sk_tdigestDocs,
   count(sk_tdigestDocs)},
  {"TinyFSM", "v1.15.1", "https://github.com/digint/tinyfsm", "MIT", "", sk_tinyfsmDocs, count(sk_tinyfsmDocs)},
  {"Visualization Toolkit (VTK)",
   "v9.6.2",
   "https://github.com/Kitware/VTK",
   "BSD-3-Clause",
   "Entropy builds a selected VTK module set for CPU-side mesh extraction and processing. VTK modules built, "
   "including transitive dependencies: VTK::CommonComputationalGeometry, VTK::CommonCore, "
   "VTK::CommonDataModel, VTK::CommonExecutionModel, VTK::CommonMath, VTK::CommonMisc, VTK::CommonSystem, "
   "VTK::CommonTransforms, VTK::FiltersCellGrid, VTK::FiltersCore, VTK::FiltersGeneral, VTK::FiltersGeometry, "
   "VTK::FiltersModeling, VTK::FiltersReduction, VTK::FiltersSources, VTK::FiltersVerdict, VTK::IOCellGrid, "
   "VTK::IOCore, VTK::IOLegacy, VTK::ImagingCore, VTK::ImagingGeneral, VTK::ImagingSources, and "
   "VTK::ImagingStatistics. Rendering, Views, Web, MPI, and Qt integration modules are disabled.",
   sk_vtkDocs,
   count(sk_vtkDocs)},
};

std::string loadLicenseText(const char* resourcePath)
{
  auto it = g_licenseCache.find(resourcePath);
  if (it != g_licenseCache.end()) {
    return it->second;
  }

  const auto fs = cmrc::licenses::get_filesystem();
  try {
    const cmrc::file file = fs.open(resourcePath);
    const std::string text(file.begin(), file.end());
    return g_licenseCache.emplace(resourcePath, text).first->second;
  }
  catch (const std::runtime_error&) {
    return g_licenseCache.emplace(resourcePath, std::string{"License resource not found: "} + resourcePath)
      .first->second;
  }
}

void renderReadonlyText(const char* label, const std::string& text)
{
  ImGui::InputTextMultiline(
    label,
    const_cast<char*>(text.c_str()),
    text.size(),
    ImVec2(-FLT_MIN, ImGui::GetTextLineHeight() * 14.0f),
    ImGuiInputTextFlags_ReadOnly);
}

void renderSourceUrl(const LicenseEntry& entry)
{
  ImGui::TextUnformatted("Source:");
  ImGui::SameLine();
  ImGui::TextLinkOpenURL(entry.source, entry.source);
  ImGui::SameLine();
  if (ImGui::SmallButton("Copy URL")) {
    ImGui::SetClipboardText(entry.source);
  }
}

bool isBundledResource(const LicenseEntry& entry)
{
  const std::string name(entry.name);
  return name.find("font") != std::string::npos || name.find("color map") != std::string::npos ||
         name.find("color maps") != std::string::npos;
}

bool hasToolkitBuildNote(const LicenseEntry& entry)
{
  return std::string_view(entry.name) == "Insight Toolkit (ITK)" ||
         std::string_view(entry.name) == "Qt Base / Qt Core" ||
         std::string_view(entry.name) == "Visualization Toolkit (VTK)";
}

void renderLicenseEntries(bool resources)
{
  for (const auto& entry : sk_entries) {
    if (isBundledResource(entry) != resources) {
      continue;
    }

    ImGui::PushID(entry.name);
    if (ImGui::CollapsingHeader(entry.name)) {
      ImGui::Text("Version: %s", entry.version);
      ImGui::Text("License: %s", entry.license);
      renderSourceUrl(entry);
      if (entry.note && entry.note[0] != '\0') {
        const bool disabledNote = hasToolkitBuildNote(entry);
        if (disabledNote) {
          ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
        }
        ImGui::TextWrapped("%s", entry.note);
        if (disabledNote) {
          ImGui::PopStyleColor();
        }
      }

      ImGui::Spacing();
      for (std::size_t i = 0; i < entry.numDocuments; ++i) {
        const auto& doc = entry.documents[i];
        ImGui::Text("%s", doc.title);
        const std::string label = std::string("##") + entry.name + doc.title;
        renderReadonlyText(label.c_str(), loadLicenseText(doc.resourcePath));
        ImGui::Spacing();
      }
    }
    ImGui::PopID();
  }
}

} // namespace

void renderThirdPartyLicenses()
{
  ImGui::TextWrapped(
    "Entropy uses the following external libraries, source snippets, fonts, and color maps. Each section includes the "
    "attribution and license material required for redistribution.");
  ImGui::Spacing();

  ImGui::BeginChild("##externalLicenses", ImVec2(-FLT_MIN, -FLT_MIN), true);

  if (ImGui::BeginTabBar("##licenseGroups")) {
    if (ImGui::BeginTabItem("Libraries")) {
      renderLicenseEntries(false);
      ImGui::EndTabItem();
    }

    if (ImGui::BeginTabItem("Resources")) {
      renderLicenseEntries(true);
      ImGui::EndTabItem();
    }

    ImGui::EndTabBar();
  }

  ImGui::EndChild();
}
