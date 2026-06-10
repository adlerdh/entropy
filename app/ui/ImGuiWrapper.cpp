#include "ui/ImGuiWrapper.h"

#include <spdlog/fmt/std.h>

#include "common/MathFuncs.h"

#include "ui/Helpers.h"
#include "ui/ImageExport.h"
#include "ui/MainMenuBar.h"
#ifdef __APPLE__
#include "ui/MacNativeMainMenu.h"
#endif
#include "ui/NativeFileDialogs.h"
#include "ui/Popups.h"
#include "ui/Style.h"
#include "ui/Toolbars.h"
#include "ui/Windows.h"
#ifdef _WIN32
#include "ui/WinNativeMainMenu.h"
#endif

#include "logic/app/AppPaths.h"
#include "logic/app/CallbackHandler.h"
#include "logic/app/Data.h"
#include "logic/annotation/PointRecord.h"
#include "logic/annotation/SerializeAnnot.h"
#include "logic/camera/CameraHelpers.h"
#include "logic/serialization/ProjectSerialization.h"
#include "logic/states/annotation/AnnotationStateHelpers.h"
#include "logic/states/annotation/AnnotationStateMachine.h"

#include <IconsForkAwesome.h>

#include <imgui/imgui.h>

// GLFW and OpenGL 3 bindings for ImGui:
#include <imgui/backends/imgui_impl_glfw.h>
#include <imgui/backends/imgui_impl_opengl3.h>

#include <implot/implot.h>

#include <cmrc/cmrc.hpp>

#include <glm/glm.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/color_space.hpp>

#include <spdlog/fmt/ostr.h>
#include <spdlog/spdlog.h>

#include <unordered_map>

CMRC_DECLARE(fonts);

namespace fs = std::filesystem;

namespace
{
static const glm::quat sk_identityRotation{1.0f, 0.0f, 0.0f, 0.0f};
static const glm::vec3 sk_zeroVec{0.0f, 0.0f, 0.0f};

std::string layoutImageDisplayName(const AppData& appData, const Layout& layout)
{
  if (layout.renderedImages().empty()) {
    return {};
  }

  const uuids::uuid& imageUid = layout.renderedImages().front();
  const Image* image = appData.image(imageUid);
  if (!image) {
    return {};
  }

  const auto imageIndex = appData.imageIndex(imageUid);
  const std::string ordinal = imageIndex ? (std::to_string(*imageIndex + 1) + ". ") : std::string{};
  return ordinal + image->settings().displayName();
}

void renderLoadingStatusBar()
{
  const ImGuiViewport* viewport = ImGui::GetMainViewport();
  const float height = ImGui::GetFrameHeight() + 10.0f;
  const ImVec2 pos{viewport->WorkPos.x, viewport->WorkPos.y + viewport->WorkSize.y - height};
  const ImVec2 size{viewport->WorkSize.x, height};

  ImGui::SetNextWindowPos(pos, ImGuiCond_Always);
  ImGui::SetNextWindowSize(size, ImGuiCond_Always);

  constexpr ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                                     ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoScrollbar |
                                     ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoFocusOnAppearing;

  ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2{12.0f, 5.0f});
  ImGui::PushStyleColor(ImGuiCol_WindowBg, ImGui::GetStyleColorVec4(ImGuiCol_MenuBarBg));

  if (ImGui::Begin("LoadingStatusBar", nullptr, flags)) {
    const int dotCount = static_cast<int>(ImGui::GetTime() * 3.0) % 4;
    std::string text = "Loading image";
    text.append(static_cast<std::size_t>(dotCount), '.');
    ImGui::TextUnformatted(text.c_str());
  }

  ImGui::End();
  ImGui::PopStyleColor();
  ImGui::PopStyleVar(3);
}

void renderEmptyWorkspace(
  ProjectLoadState projectLoadState,
  const std::function<void(const std::vector<fs::path>& fileNames)>& openImageFiles,
  const std::function<void(const std::vector<fs::path>& folderNames)>& openDicomFolders,
  const std::function<void()>& requestDicomFolderPathDialog,
  const std::function<void(const fs::path& fileName)>& openProjectFile)
{
  if (ProjectLoadState::Empty != projectLoadState && ProjectLoadState::Failed != projectLoadState) {
    return;
  }

  const ImGuiViewport* viewport = ImGui::GetMainViewport();
  const ImVec2 panelSize{600.0f, 98.0f};
  ImGui::SetNextWindowPos(viewport->GetCenter(), ImGuiCond_Always, ImVec2{0.5f, 0.5f});
  ImGui::SetNextWindowSize(panelSize, ImGuiCond_Always);

  constexpr ImGuiWindowFlags flags =
    ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings;

  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2{16.0f, 12.0f});
  if (ImGui::Begin("EmptyWorkspace", nullptr, flags)) {
    constexpr float buttonWidth = 160.0f;
    const float textHeight = ImGui::GetTextLineHeight();
    const float buttonHeight = ImGui::GetFrameHeight();
    const float blockHeight = textHeight + ImGui::GetStyle().ItemSpacing.y + buttonHeight;
    const float contentHeight = ImGui::GetContentRegionAvail().y;
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + std::max(0.0f, (contentHeight - blockHeight) * 0.5f));

    const char* text = ProjectLoadState::Failed == projectLoadState ? "Project failed to load" : "No project loaded.";
    ImGui::SetCursorPosX(
      std::max(ImGui::GetCursorPosX(), (ImGui::GetWindowSize().x - ImGui::CalcTextSize(text).x) * 0.5f));
    ImGui::TextUnformatted(text);
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + ImGui::GetStyle().ItemSpacing.y);

    const float buttonsWidth = 3.0f * buttonWidth + 2.0f * ImGui::GetStyle().ItemSpacing.x;
    ImGui::SetCursorPosX(std::max(ImGui::GetCursorPosX(), (ImGui::GetWindowSize().x - buttonsWidth) * 0.5f));

    if (ImGui::Button("Open Image(s)...", ImVec2{buttonWidth, 0.0f})) {
      const auto selectedFiles = native_dialog::openFiles(native_dialog::imageFilters());
      if (!selectedFiles.empty() && openImageFiles) {
        openImageFiles(selectedFiles);
      }
    }

    ImGui::SameLine();

    if (ImGui::Button("Open DICOM Series...", ImVec2{buttonWidth, 0.0f})) {
      const auto selectedFolders = native_dialog::pickFolders();
      if (!selectedFolders.empty() && openDicomFolders) {
        openDicomFolders(selectedFolders);
      }
      else if (requestDicomFolderPathDialog) {
        requestDicomFolderPathDialog();
      }
    }

    ImGui::SameLine();

    if (ImGui::Button("Open Project...", ImVec2{buttonWidth, 0.0f})) {
      if (const auto selectedFile = native_dialog::openFile(native_dialog::projectFilters())) {
        if (openProjectFile) {
          openProjectFile(*selectedFile);
        }
      }
    }
  }
  ImGui::End();
  ImGui::PopStyleVar();
}

ImFont* loadFont(
  const std::string& fontPath,
  const ImFontConfig& fontConfig,
  float fontSize,
  const ImWchar* glyphRange = nullptr)
{
  auto filesystem = cmrc::fonts::get_filesystem();

  cmrc::file fontFile = filesystem.open(fontPath);

  // ImGui will take ownership of the font (and be responsible for deleting it), so make a copy:
  char* fontData = new char[fontFile.size()];

  for (std::size_t i = 0; i < fontFile.size(); ++i) {
    fontData[i] = fontFile.cbegin()[i];
  }

  // Note: Transfer ownership of 'ttf_data' to ImFontAtlas!
  // Will be deleted after destruction of the atlas.
  // Set font_cfg->FontDataOwnedByAtlas=false to keep ownership of data and it won't be freed.

  return ImGui::GetIO().Fonts->AddFontFromMemoryTTF(
    static_cast<void*>(fontData),
    static_cast<int32_t>(fontFile.size()),
    static_cast<float>(fontSize),
    &fontConfig,
    glyphRange);
}

} // namespace

ImGuiWrapper::ImGuiWrapper(GLFWwindow* window, AppData& appData, CallbackHandler& callbackHandler)
  : m_appData(appData), m_callbackHandler(callbackHandler), m_window(window), m_contentScale(1.0f)
{
  IMGUI_CHECKVERSION();

  ImGui::CreateContext();
  spdlog::debug("Created ImGui context");

  ImPlot::CreateContext();
  spdlog::debug("Created ImPlot context");

  ImGuiIO& io = ImGui::GetIO();

  m_iniFilePath = app_paths::userDataDirectory() / "entropy_ui.ini";
  m_logFilePath = app_paths::logDirectory() / "entropy_ui.log";
  std::filesystem::create_directories(m_iniFilePath.parent_path());
  std::filesystem::create_directories(m_logFilePath.parent_path());

  m_iniFileName = m_iniFilePath.string();
  m_logFileName = m_logFilePath.string();
  io.IniFilename = m_iniFileName.c_str();
  io.LogFilename = m_logFileName.c_str();

  io.ConfigDragClickToInputText = true;
  //    io.MouseDrawCursor = true;

  /// @todo Add window option for unsaved document (a little dot) when project changes

  io.ConfigFlags = io.ConfigFlags & ImGuiConfigFlags_NoMouseCursorChange;

  // Setup ImGui platform/renderer bindings:
  static const char* glsl_version = "#version 150";
  ImGui_ImplGlfw_InitForOpenGL(window, true);
  ImGui_ImplOpenGL3_Init(glsl_version);

  applyCustomDarkStyle();

  ImGuiStyle& style = ImGui::GetStyle();
  style.ScaleAllSizes(m_contentScale);

  spdlog::debug("Done setup of ImGui platform and renderer bindings");

  initializeFonts();
  setContentScale(appData.windowData().getContentScaleRatio());
}

ImGuiWrapper::~ImGuiWrapper()
{
  ImGui_ImplOpenGL3_Shutdown();
  ImGui_ImplGlfw_Shutdown();

  ImPlot::DestroyContext();
  spdlog::debug("Destroyed ImPlot context");

  ImGui::DestroyContext();
  spdlog::debug("Destroyed ImGui context");
}

void ImGuiWrapper::setCallbacks(
  std::function<void(void)> postEmptyGlfwEvent,
  std::function<void(void)> readjustViewport,
  std::function<void(const std::vector<fs::path>& fileNames)> openImageFiles,
  std::function<void(const std::vector<fs::path>& fileNames)> addImageFiles,
  std::function<void(const std::vector<fs::path>& folderNames)> openDicomFolders,
  std::function<void(const fs::path& fileName)> addSegmentationFile,
  std::function<void(const uuids::uuid& imageUid, const fs::path& fileName)> addSegmentationFileToImage,
  std::function<void(const fs::path& fileName)> openProjectFile,
  std::function<void(GuiData::LargeImageLoadDecision decision)> largeImageLoadDecision,
  std::function<void(
    const std::vector<dicom::SeriesInfo>& series,
    std::optional<std::size_t> referenceSeriesIndex,
    bool addToExistingProject)> loadDicomSeries,
  std::function<bool()> saveProject,
  std::function<bool(const fs::path& fileName)> saveProjectAs,
  std::function<void()> closeProject,
  std::function<void(const fs::path& fileName)> loadLayoutsFile,
  std::function<bool(const fs::path& fileName)> saveLayoutsFile,
  std::function<void()> closeProjectWithoutPrompt,
  std::function<void()> requestQuitApp,
  std::function<void()> quitAppWithoutPrompt,
  std::function<void(const uuids::uuid& viewUid)> recenterView,
  AllViewsRecenterType recenterCurrentViews,
  std::function<bool(void)> getOverlayVisibility,
  std::function<void(bool)> setOverlayVisibility,
  std::function<void(void)> updateAllImageUniforms,
  std::function<void(const uuids::uuid& imageUid)> updateImageUniforms,
  std::function<void(const uuids::uuid& imageUid)> updateImageInterpolationMode,
  std::function<void(std::size_t cmapIndex)> updateImageColorMapInterpolationMode,
  std::function<void(std::size_t tableIndex)> updateLabelColorTableTexture,
  std::function<void(const uuids::uuid& imageUid, std::size_t labelIndex)> moveCrosshairsToSegLabelCentroid,
  std::function<void()> updateMetricUniforms,
  std::function<glm::vec3()> getWorldDeformedPos,
  std::function<std::optional<glm::vec3>(std::size_t imageIndex)> getSubjectPos,
  std::function<std::optional<glm::ivec3>(std::size_t imageIndex)> getVoxelPos,
  std::function<void(std::size_t imageIndex, const glm::vec3& subjectPos)> setSubjectPos,
  std::function<void(std::size_t imageIndex, const glm::ivec3& voxelPos)> setVoxelPos,
  std::function<std::vector<double>(std::size_t imageIndex, bool getOnlyActiveComponent)> getImageValuesNN,
  std::function<std::vector<double>(std::size_t imageIndex, bool getOnlyActiveComponent)> getImageValuesLinear,
  std::function<std::optional<int64_t>(std::size_t imageIndex)> getSegLabel,
  std::function<std::optional<uuids::uuid>(const uuids::uuid& matchingImageUid, const std::string& segDisplayName)>
    createBlankSeg,
  std::function<bool(const uuids::uuid& segUid)> clearSeg,
  std::function<bool(const uuids::uuid& segUid)> removeSeg,
  std::function<bool(const uuids::uuid& imageUid, const uuids::uuid& seedSegUid, const SeedSegmentationType&)>
    executePoissonSeg,
  std::function<bool(const uuids::uuid& imageUid, bool locked)> setLockManualImageTransformation,
  std::function<bool(const uuids::uuid& imageUid)> setReferenceImage,
  std::function<bool(const uuids::uuid& imageUid)> removeImage,
  std::function<void()> paintActiveSegmentationWithActivePolygon)
{
  m_postEmptyGlfwEvent = postEmptyGlfwEvent;
  m_readjustViewport = readjustViewport;
  m_openImageFiles = openImageFiles;
  m_addImageFiles = addImageFiles;
  m_openDicomFolders = openDicomFolders;
  m_addSegmentationFile = addSegmentationFile;
  m_addSegmentationFileToImage = addSegmentationFileToImage;
  m_openProjectFile = openProjectFile;
  m_largeImageLoadDecision = largeImageLoadDecision;
  m_loadDicomSeries = loadDicomSeries;
  m_saveProject = saveProject;
  m_saveProjectAs = saveProjectAs;
  m_closeProject = closeProject;
  m_loadLayoutsFile = loadLayoutsFile;
  m_saveLayoutsFile = saveLayoutsFile;
  m_closeProjectWithoutPrompt = closeProjectWithoutPrompt;
  m_requestQuitApp = requestQuitApp;
  m_quitAppWithoutPrompt = quitAppWithoutPrompt;
  m_recenterView = recenterView;
  m_recenterAllViews = recenterCurrentViews;
  m_getOverlayVisibility = getOverlayVisibility;
  m_setOverlayVisibility = setOverlayVisibility;
  m_updateAllImageUniforms = updateAllImageUniforms;
  m_updateImageUniforms = updateImageUniforms;
  m_updateImageInterpolationMode = updateImageInterpolationMode;
  m_updateImageColorMapInterpolationMode = updateImageColorMapInterpolationMode;
  m_updateLabelColorTableTexture = updateLabelColorTableTexture;
  m_moveCrosshairsToSegLabelCentroid = moveCrosshairsToSegLabelCentroid;
  m_updateMetricUniforms = updateMetricUniforms;
  m_getWorldDeformedPos = getWorldDeformedPos;
  m_getSubjectPos = getSubjectPos;
  m_getVoxelPos = getVoxelPos;
  m_setSubjectPos = setSubjectPos;
  m_setVoxelPos = setVoxelPos;
  m_getImageValuesNN = getImageValuesNN;
  m_getImageValuesLinear = getImageValuesLinear;
  m_getSegLabel = getSegLabel;
  m_createBlankSeg = createBlankSeg;
  m_clearSeg = clearSeg;
  m_removeSeg = removeSeg;
  m_executePoissonSeg = executePoissonSeg;
  m_setLockManualImageTransformation = setLockManualImageTransformation;
  m_setReferenceImage = setReferenceImage;
  m_removeImage = removeImage;
  m_paintActiveSegmentationWithActivePolygon = paintActiveSegmentationWithActivePolygon;
}

void ImGuiWrapper::storeFuture(const uuids::uuid& taskUid, std::future<AsyncTaskDetails> future)
{
  std::lock_guard<std::mutex> lock(m_futuresMutex);

  if (!future.valid()) {
    spdlog::warn("Future for task {} is not valid", taskUid);
    return;
  }

  m_futures.emplace(taskUid, std::move(future));

  spdlog::debug("Storing future for UI task {}. Total number of UI task futures: {}", taskUid, m_futures.size());
}

void ImGuiWrapper::addTaskToIsosurfaceGpuMeshGenerationQueue(const uuids::uuid& taskUid)
{
  std::lock_guard<std::mutex> lock(m_isosurfaceTaskQueueMutex);

  m_isosurfaceTaskQueueForGpuMeshGeneration.push(taskUid);

  // Post an empty event to notify render thread
  if (m_postEmptyGlfwEvent) {
    m_postEmptyGlfwEvent();
  }
}

void ImGuiWrapper::generateIsosurfaceMeshGpuRecords()
{
  std::lock_guard<std::mutex> lock(m_isosurfaceTaskQueueMutex);

  while (!m_isosurfaceTaskQueueForGpuMeshGeneration.empty()) {
    const uuids::uuid taskUid = m_isosurfaceTaskQueueForGpuMeshGeneration.front();
    m_isosurfaceTaskQueueForGpuMeshGeneration.pop();

    auto it = m_futures.find(taskUid);
    if (std::end(m_futures) == it) {
      spdlog::error("Invalid task {}", taskUid);
      continue;
    }

    auto& future = it->second;

    // In case the CPU mesh generation task is not done, then wait for it to finish
    // and get the result. (Note: it should be done, since tasks only get on this queue when
    // CPU mesh generation is done.)
    const AsyncTaskDetails value = future.get();

    // Remove the future
    m_futures.erase(it);

    if (
      AsyncTasks::IsosurfaceMeshGeneration != value.task || false == value.success || !value.imageUid ||
      !value.imageComponent || !value.objectUid)
    {
      spdlog::error("Failed task {}", taskUid);
      continue;
    }

    spdlog::info("Task {}: Start generating GPU mesh for isosurface {} ", taskUid, *value.objectUid);

    // Get the isosurface associated with this task
    const Isosurface* surface = m_appData.isosurface(*value.imageUid, *value.imageComponent, *value.objectUid);
    if (!surface) {
      spdlog::error("Null isosurface for isosurface {} of image {}", *value.objectUid, *value.imageUid);
      continue;
    }

#if 0
    const MeshCpuRecord* cpuMeshRecord = surface->mesh.cpuData();

    if (!cpuMeshRecord)
    {
      spdlog::error(
        "Null CPU mesh record for isosurface {} of image {}", *value.objectUid, *value.imageUid
      );
      continue;
    }

    std::unique_ptr<MeshGpuRecord> gpuMeshRecord = gpuhelper::createMeshGpuRecordFromVtkPolyData(
      cpuMeshRecord->polyData(),
      cpuMeshRecord->meshInfo().primitiveType(),
      BufferUsagePattern::StreamDraw
    );

    if (!gpuMeshRecord)
    {
      spdlog::error(
        "Error generating GPU mesh record for isosurface {} of image {}",
        *value.objectUid,
        *value.imageUid
      );
      continue;
    }

    spdlog::info("Task {}: Done generating GPU mesh for isosurface {} ", taskUid, *value.objectUid);

    const bool updated = m_appData.updateIsosurfaceMeshGpuRecord(
      *value.imageUid, *value.imageComponent, *value.objectUid, std::move(gpuMeshRecord)
    );

    if (updated)
    {
      spdlog::info(
        "Updated GPU record for isosurface mesh {} of image {}", *value.objectUid, *value.imageUid
      );
    }
    else
    {
      spdlog::error(
        "Could not update GPU record for isosurface mesh {} of image {}",
        *value.objectUid,
        *value.imageUid
      );
    }
#endif
  }
}

/*
Q: How should I handle DPI in my application?
The short answer is: obtain the desired DPI scale, load your fonts resized with that scale (always
round down fonts size to the nearest integer), and scale your Style structure accordingly using
style.ScaleAllSizes().

Your application may want to detect DPI change and reload the fonts and reset style between frames.

Your ui code should avoid using hardcoded constants for size and positioning. Prefer to express
values as multiple of reference values such as ImGui::GetFontSize() or ImGui::GetFrameHeight(). So
e.g. instead of seeing a hardcoded height of 500 for a given item/window, you may want to use
30*ImGui::GetFontSize() instead.
*/
void ImGuiWrapper::setContentScale(float scale)
{
  if (m_contentScale == scale) {
    return;
  }

  spdlog::info("Setting content scale to {}", scale);

  m_contentScale = scale;

  ImGuiStyle& style = ImGui::GetStyle();
  style.ScaleAllSizes(m_contentScale);

  // For correct scaling, prefer to reload font + rebuild ImFontAtlas
  initializeFonts();
}

void ImGuiWrapper::initializeFonts()
{
  static const std::string cousineFontPath("res/fonts/Cousine/Cousine-Regular.ttf");
  static const std::string spaceGroteskFontPath("res/fonts/SpaceGrotesk/SpaceGrotesk-Light.ttf");
  static const std::string forkAwesomeFontPath = std::string("res/fonts/ForkAwesome/") + FONT_ICON_FILE_NAME_FK;

  spdlog::debug("Begin loading fonts");

  ImFontConfig cousineFontConfig;
  const float cousineFontSize = 14.0f;

  myImFormatString(
    cousineFontConfig.Name,
    IM_ARRAYSIZE(cousineFontConfig.Name),
    "%s, %.0fpx",
    "Cousine Regular",
    cousineFontSize);

  ImFontConfig spaceGroteskFontConfig;
  const float spaceGroteskFontSize = 16.0f;

  myImFormatString(
    spaceGroteskFontConfig.Name,
    IM_ARRAYSIZE(spaceGroteskFontConfig.Name),
    "%s, %.0fpx",
    "Space Grotesk Light",
    spaceGroteskFontSize);

  // Merge in icons from Fork Awesome:
  ImFontConfig forkAwesomeFontConfig;
  forkAwesomeFontConfig.MergeMode = true;
  forkAwesomeFontConfig.PixelSnapH = true;

  const float forkAwesomeFontSize = 14.0f;

  myImFormatString(
    forkAwesomeFontConfig.Name,
    IM_ARRAYSIZE(forkAwesomeFontConfig.Name),
    "%s, %.0fpx",
    "Fork Awesome",
    forkAwesomeFontSize);

  /// @see For details about Fork Awesome fonts: https://forkaweso.me/Fork-Awesome/icons/
  static const ImWchar forkAwesomeIconGlyphRange[] = {ICON_MIN_FK, ICON_MAX_FK, 0};

  // Clear all
  // ImGui::GetIO().Fonts->Clear();
  // m_appData.guiData().m_fonts.clear();

  // Load fonts: If no fonts are loaded, dear imgui will use the default font.
  // You can also load multiple fonts and use ImGui::PushFont()/PopFont() to select them.
  // AddFontFromFileTTF() will return the ImFont* so you can store it if you need to select the
  // font among multiple. If the file cannot be loaded, the function will return NULL.
  // Please handle those errors in your application (e.g. use an assertion, or display an error and
  // quit). The fonts will be rasterized at a given size (w/ oversampling) and stored into a texture
  // when calling ImFontAtlas::Build()/GetTexDataAsXXXX(), which ImGui_ImplXXXX_NewFrame below will
  // call.
  /// @todo use Freetype Rasterizer and Small Font Sizes

  ImFont* cousineFontPtr = loadFont(cousineFontPath, cousineFontConfig, m_contentScale * cousineFontSize, nullptr);
  ImFont* fork1Ptr = loadFont(
    forkAwesomeFontPath,
    forkAwesomeFontConfig,
    m_contentScale * forkAwesomeFontSize,
    forkAwesomeIconGlyphRange);

  ImFont* spaceFontPtr =
    loadFont(spaceGroteskFontPath, spaceGroteskFontConfig, m_contentScale * spaceGroteskFontSize, nullptr);
  ImFont* fork2Ptr = loadFont(
    forkAwesomeFontPath,
    forkAwesomeFontConfig,
    m_contentScale * forkAwesomeFontSize,
    forkAwesomeIconGlyphRange);

  if (cousineFontPtr && fork1Ptr) {
    m_appData.guiData().m_fonts[cousineFontPath] = cousineFontPtr;
    m_appData.guiData().m_fonts[cousineFontPath + forkAwesomeFontPath] = fork1Ptr;
    spdlog::debug("Loaded font {}", cousineFontPath);
  }
  else {
    spdlog::error("Unable to load font {}", forkAwesomeFontPath);
  }

  if (spaceFontPtr && fork2Ptr) {
    m_appData.guiData().m_fonts[spaceGroteskFontPath] = spaceFontPtr;
    m_appData.guiData().m_fonts[spaceGroteskFontPath + forkAwesomeFontPath] = fork2Ptr;
    spdlog::debug("Loaded font {}", spaceGroteskFontPath);
  }
  else {
    spdlog::error("Unable to load font {}", forkAwesomeFontPath);
  }

  spdlog::debug("Done loading fonts");
}

std::pair<std::string, std::string> ImGuiWrapper::getImageDisplayAndFileNames(std::size_t imageIndex) const
{
  static const std::string s_empty("<unknown>");

  if (const auto imageUid = m_appData.imageUid(imageIndex)) {
    if (const Image* image = m_appData.image(*imageUid)) {
      return {image->settings().displayName(), image->header().fileName().string()};
    }
  }

  return {s_empty.c_str(), s_empty.c_str()};
}

void ImGuiWrapper::render()
{
  using namespace std::placeholders;

  generateIsosurfaceMeshGpuRecords();

  ImGui_ImplOpenGL3_NewFrame();
  ImGui_ImplGlfw_NewFrame();

  /// @todo Move these functions elsewhere

  /// @todo remove this
  auto getActiveImageIndex = [this]() -> std::size_t {
    if (0 == m_appData.numImages()) {
      return 0;
    }

    if (const auto imageUid = m_appData.activeImageUid()) {
      if (const auto index = m_appData.imageIndex(*imageUid)) {
        return *index;
      }
    }

    spdlog::warn("No valid active image for {} loaded images", m_appData.numImages());
    return 0;
  };

  auto setActiveImageIndex = [this](std::size_t index) {
    if (const auto imageUid = m_appData.imageUid(index)) {
      if (!m_appData.setActiveImageUid(*imageUid)) {
        spdlog::warn("Cannot set active image to {}", *imageUid);
      }
    }
    else {
      spdlog::warn("Cannot set active image to invalid index {}", index);
    }
  };

  auto getImageHasActiveSeg = [this](std::size_t index) -> bool {
    if (const auto imageUid = m_appData.imageUid(index)) {
      return m_appData.isImageBeingSegmented(*imageUid);
    }
    else {
      spdlog::warn("Cannot get whether seg is active for invalid image index {}", index);
      return false;
    }
  };

  auto setImageHasActiveSeg = [this](std::size_t index, bool set) {
    if (const auto imageUid = m_appData.imageUid(index)) {
      m_appData.setImageBeingSegmented(*imageUid, set);
    }
    else {
      spdlog::warn("Cannot set whether seg is active for invalid image index {}", index);
    }
  };

  /// @todo remove this
  auto getMouseMode = [this]() {
    return m_appData.state().mouseMode();
  };

  auto setMouseMode = [this](MouseMode mouseMode) {
    m_callbackHandler.setMouseMode(mouseMode);
  };

  auto cycleViewLayout = [this](int step) {
    m_appData.windowData().cycleCurrentLayout(step);
  };

  /// @todo remove this
  auto getNumImageColorMaps = [this]() -> std::size_t {
    return m_appData.numImageColorMaps();
  };

  auto getImageColorMap = [this](std::size_t cmapIndex) -> ImageColorMap* {
    if (const auto cmapUid = m_appData.imageColorMapUid(cmapIndex)) {
      return m_appData.imageColorMap(*cmapUid);
    }
    return nullptr;
  };

  auto getLabelTable = [this](std::size_t tableIndex) -> ParcellationLabelTable* {
    if (const auto tableUid = m_appData.labelTableUid(tableIndex)) {
      return m_appData.labelTable(*tableUid);
    }
    return nullptr;
  };

  auto getImageIsVisibleSetting = [this](std::size_t imageIndex) -> bool {
    if (const auto imageUid = m_appData.imageUid(imageIndex)) {
      if (const Image* image = m_appData.image(*imageUid)) {
        return image->settings().visibility();
      }
    }
    return false;
  };

  auto getImageIsActive = [this](std::size_t imageIndex) -> bool {
    if (const auto imageUid = m_appData.imageUid(imageIndex)) {
      if (const auto activeImageUid = m_appData.activeImageUid()) {
        return (*imageUid == *activeImageUid);
      }
    }
    return false;
  };

  auto moveImageBackward = [this](const uuids::uuid& imageUid) -> bool {
    if (m_appData.moveImageBackwards(imageUid)) {
      m_appData.windowData().updateImageOrdering(m_appData.imageUidsOrdered());
      return true;
    }
    return false;
  };

  auto moveImageForward = [this](const uuids::uuid& imageUid) -> bool {
    if (m_appData.moveImageForwards(imageUid)) {
      m_appData.windowData().updateImageOrdering(m_appData.imageUidsOrdered());
      return true;
    }
    return false;
  };

  auto moveImageToBack = [this](const uuids::uuid& imageUid) -> bool {
    if (m_appData.moveImageToBack(imageUid)) {
      m_appData.windowData().updateImageOrdering(m_appData.imageUidsOrdered());
      return true;
    }
    return false;
  };

  auto moveImageToFront = [this](const uuids::uuid& imageUid) -> bool {
    if (m_appData.moveImageToFront(imageUid)) {
      m_appData.windowData().updateImageOrdering(m_appData.imageUidsOrdered());
      return true;
    }
    return false;
  };

  auto activeImageUid = [this]() -> std::optional<uuids::uuid> {
    return m_appData.activeImageUid();
  };

  auto activeSegUid = [activeImageUid, this]() -> std::optional<uuids::uuid> {
    const auto imageUid = activeImageUid();
    return imageUid ? m_appData.imageToActiveSegUid(*imageUid) : std::nullopt;
  };

  auto activeAnnotation = [activeImageUid,
                           this]() -> std::pair<std::optional<uuids::uuid>, std::optional<uuids::uuid>> {
    const auto imageUid = activeImageUid();
    if (!imageUid) {
      return {std::nullopt, std::nullopt};
    }
    return {*imageUid, m_appData.imageToActiveAnnotationUid(*imageUid)};
  };

  auto activeLandmarkGroupUid = [activeImageUid, this]() -> std::optional<uuids::uuid> {
    const auto imageUid = activeImageUid();
    return imageUid ? m_appData.imageToActiveLandmarkGroupUid(*imageUid) : std::nullopt;
  };

  auto createActiveSegmentation = [activeImageUid, getActiveImageIndex, this]() {
    const auto imageUid = activeImageUid();
    const Image* image = imageUid ? m_appData.image(*imageUid) : nullptr;
    if (!imageUid || !image || !m_createBlankSeg) {
      return;
    }

    const std::size_t numSegsForImage = m_appData.imageToSegUids(*imageUid).size();
    const std::string displayName = "Untitled segmentation " + std::to_string(numSegsForImage + 1) + " for image '" +
                                    image->settings().displayName() + "'";
    if (m_createBlankSeg(*imageUid, displayName) && m_updateImageUniforms) {
      m_updateImageUniforms(*imageUid);
    }
    static_cast<void>(getActiveImageIndex);
  };

  auto saveActiveSegmentation = [activeSegUid, this]() {
    const auto segUid = activeSegUid();
    Image* seg = segUid ? m_appData.seg(*segUid) : nullptr;
    if (!seg) {
      return;
    }

    if (const auto selectedFile = native_dialog::saveFile(native_dialog::segmentationFilters())) {
      static constexpr uint32_t compToSave = 0;
      if (seg->saveComponentToDisk(compToSave, *selectedFile)) {
        spdlog::info("Saved segmentation image to file {}", *selectedFile);
        seg->header().setFileName(*selectedFile);
      }
      else {
        spdlog::error("Error saving segmentation image to file {}", *selectedFile);
      }
    }
  };

  auto projectHasAnnotations = [this]() {
    for (const auto& imageUid : m_appData.imageUidsOrdered()) {
      if (!m_appData.annotationsForImage(imageUid).empty()) {
        return true;
      }
    }
    return false;
  };

  auto saveAllAnnotations = [projectHasAnnotations, this]() {
    if (!projectHasAnnotations()) {
      return;
    }

    if (const auto selectedFile = native_dialog::saveFile(native_dialog::annotationFilters())) {
      nlohmann::json annotationsJson;
      std::size_t numSavedAnnotations = 0;
      for (const auto& imageUid : m_appData.imageUidsOrdered()) {
        for (const auto& annotUid : m_appData.annotationsForImage(imageUid)) {
          if (const Annotation* annot = m_appData.annotation(annotUid)) {
            serialize::appendAnnotationToJson(*annot, annotationsJson);
            ++numSavedAnnotations;
          }
        }
      }

      if (serialize::saveToJsonFile(annotationsJson, *selectedFile)) {
        spdlog::info("Saved {} annotations to JSON file {}", numSavedAnnotations, *selectedFile);
        for (const auto& imageUid : m_appData.imageUidsOrdered()) {
          for (const auto& annotUid : m_appData.annotationsForImage(imageUid)) {
            if (Annotation* annot = m_appData.annotation(annotUid)) {
              annot->setFileName(*selectedFile);
              annot->markClean();
            }
          }
        }
      }
      else {
        spdlog::error("Error saving annotations to JSON file {}", *selectedFile);
      }
    }
  };

  auto createActiveLandmarkGroup = [activeImageUid, this]() {
    const auto imageUid = activeImageUid();
    const Image* image = imageUid ? m_appData.image(*imageUid) : nullptr;
    if (!imageUid || !image) {
      return;
    }

    LandmarkGroup newGroup;
    newGroup.setName("Landmarks for " + image->settings().displayName());
    const auto landmarkGroupUid = m_appData.addLandmarkGroup(std::move(newGroup));
    m_appData.assignLandmarkGroupUidToImage(*imageUid, landmarkGroupUid);
    m_appData.setRainbowColorsForAllLandmarkGroups();
    m_appData.assignActiveLandmarkGroupUidToImage(*imageUid, landmarkGroupUid);
  };

  auto saveActiveLandmarkGroup = [activeLandmarkGroupUid, this]() {
    const auto landmarkGroupUid = activeLandmarkGroupUid();
    LandmarkGroup* landmarkGroup = landmarkGroupUid ? m_appData.landmarkGroup(*landmarkGroupUid) : nullptr;
    if (!landmarkGroup) {
      return;
    }

    if (const auto selectedFile = native_dialog::saveFile(native_dialog::landmarkFilters())) {
      if (serialize::saveLandmarkGroupCsvFile(landmarkGroup->getPoints(), *selectedFile)) {
        spdlog::info("Saved landmarks to CSV file {}", *selectedFile);
        landmarkGroup->setFileName(*selectedFile);
      }
      else {
        spdlog::error("Error saving landmarks to CSV file {}", *selectedFile);
      }
    }
  };

  auto addLandmarkAtCrosshairs = [activeImageUid, activeLandmarkGroupUid, this]() {
    const auto imageUid = activeImageUid();
    const auto landmarkGroupUid = activeLandmarkGroupUid();
    const Image* image = imageUid ? m_appData.image(*imageUid) : nullptr;
    LandmarkGroup* landmarkGroup = landmarkGroupUid ? m_appData.landmarkGroup(*landmarkGroupUid) : nullptr;
    if (!image || !landmarkGroup) {
      return;
    }

    const glm::mat4 landmark_T_world = landmarkGroup->getInVoxelSpace() ? image->transformations().pixel_T_worldDef()
                                                                        : image->transformations().subject_T_worldDef();
    const glm::vec4 landmarkPosition =
      landmark_T_world * glm::vec4{m_appData.state().worldCrosshairs().worldOrigin(), 1.0f};

    PointRecord<glm::vec3> point{glm::vec3{landmarkPosition / landmarkPosition.w}};
    const size_t newIndex = landmarkGroup->getPoints().empty() ? 0u : landmarkGroup->maxIndex() + 1;
    const auto colors = math::generateRandomHsvSamples(
      1,
      std::make_pair(0.0f, 360.0f),
      std::make_pair(0.3f, 1.0f),
      std::make_pair(0.3f, 1.0f),
      static_cast<uint32_t>(newIndex));
    if (!colors.empty()) {
      point.setColor(glm::rgbColor(colors.front()));
    }

    landmarkGroup->addPoint(newIndex, point);
  };

  auto performMenuAction = [activeImageUid,
                            activeSegUid,
                            activeAnnotation,
                            createActiveSegmentation,
                            saveActiveSegmentation,
                            saveAllAnnotations,
                            createActiveLandmarkGroup,
                            saveActiveLandmarkGroup,
                            addLandmarkAtCrosshairs,
                            moveImageBackward,
                            moveImageForward,
                            moveImageToBack,
                            moveImageToFront,
                            this](MainMenuAction action) {
    switch (action) {
      case MainMenuAction::SetModePointer:
        m_callbackHandler.setMouseMode(MouseMode::Pointer);
        break;
      case MainMenuAction::SetModeWindowLevel:
        m_callbackHandler.setMouseMode(MouseMode::WindowLevel);
        break;
      case MainMenuAction::SetModeZoom:
        m_callbackHandler.setMouseMode(MouseMode::CameraZoom);
        break;
      case MainMenuAction::SetModePan:
        m_callbackHandler.setMouseMode(MouseMode::CameraTranslate);
        break;
      case MainMenuAction::SetModeRotateView:
        m_callbackHandler.setMouseMode(MouseMode::CameraRotate);
        break;
      case MainMenuAction::SetModeRotateCrosshairs:
        m_callbackHandler.setMouseMode(MouseMode::CrosshairsRotate);
        break;
      case MainMenuAction::SetModeSegment:
        m_callbackHandler.setMouseMode(MouseMode::Segment);
        break;
      case MainMenuAction::SetModeAnnotate:
        m_callbackHandler.setMouseMode(MouseMode::Annotate);
        break;
      case MainMenuAction::SetModeTranslateImage:
        m_callbackHandler.setMouseMode(MouseMode::ImageTranslate);
        break;
      case MainMenuAction::SetModeRotateImage:
        m_callbackHandler.setMouseMode(MouseMode::ImageRotate);
        break;
      case MainMenuAction::Recenter:
        if (m_recenterAllViews) m_recenterAllViews(false, false, true, false, true);
        break;
      case MainMenuAction::ResetView:
        if (m_recenterAllViews) m_recenterAllViews(true, true, true, true, true);
        break;
      case MainMenuAction::ToggleImageVisibility:
        m_callbackHandler.toggleImageVisibility();
        break;
      case MainMenuAction::ToggleSegmentationVisibility:
        m_callbackHandler.toggleSegVisibility();
        break;
      case MainMenuAction::ToggleImageEdges:
        m_callbackHandler.toggleImageEdges();
        break;
      case MainMenuAction::ToggleSegmentationOutline:
        m_callbackHandler.toggleSegGlobalOutline();
        break;
      case MainMenuAction::DecreaseSegmentationOpacity:
        m_callbackHandler.changeSegOpacity(-0.05, false);
        break;
      case MainMenuAction::IncreaseSegmentationOpacity:
        m_callbackHandler.changeSegOpacity(0.05, false);
        break;
      case MainMenuAction::ToggleScaleBars:
        m_appData.renderData().m_showScaleBars = !m_appData.renderData().m_showScaleBars;
        break;
      case MainMenuAction::ToggleLightboxOffsets:
        m_appData.renderData().m_showLightboxOffsetLabels = !m_appData.renderData().m_showLightboxOffsetLabels;
        break;
      case MainMenuAction::ToggleOverlays:
        m_callbackHandler.cycleOverlayAndUiVisibility();
        break;
      case MainMenuAction::ToggleFullScreen:
        m_callbackHandler.toggleFullScreenMode();
        break;
      case MainMenuAction::ToggleSync:
        m_appData.settings().setCursorSyncEnabled(!m_appData.settings().cursorSyncEnabled());
        break;
      case MainMenuAction::ToggleSyncSendCursor:
        m_appData.settings().setSendCursorSync(!m_appData.settings().sendCursorSync());
        break;
      case MainMenuAction::ToggleSyncReceiveCursor:
        m_appData.settings().setReceiveCursorSync(!m_appData.settings().receiveCursorSync());
        break;
      case MainMenuAction::ToggleSyncSendZoom:
        m_appData.settings().setSendZoomSync(!m_appData.settings().sendZoomSync());
        break;
      case MainMenuAction::ToggleSyncReceiveZoom:
        m_appData.settings().setReceiveZoomSync(!m_appData.settings().receiveZoomSync());
        break;
      case MainMenuAction::ToggleSyncSendPan:
        m_appData.settings().setSendPanSync(!m_appData.settings().sendPanSync());
        break;
      case MainMenuAction::ToggleSyncReceivePan:
        m_appData.settings().setReceivePanSync(!m_appData.settings().receivePanSync());
        break;
      case MainMenuAction::SetActiveImageAsReference:
        if (const auto imageUid = activeImageUid()) {
          m_appData.guiData().m_pendingReferenceImageUid = *imageUid;
          m_appData.guiData().m_showConfirmSetReferenceImagePopup = true;
        }
        break;
      case MainMenuAction::ExportActiveImage:
        if (const auto imageUid = activeImageUid()) {
          image_export::exportDicomImage(m_appData, *imageUid);
        }
        break;
      case MainMenuAction::RemoveActiveImage:
        if (const auto imageUid = activeImageUid()) {
          m_appData.guiData().m_pendingRemoveImageUid = *imageUid;
          m_appData.guiData().m_showConfirmRemoveImagePopup = true;
        }
        break;
      case MainMenuAction::MoveActiveImageBackward:
        if (const auto imageUid = activeImageUid()) moveImageBackward(*imageUid);
        break;
      case MainMenuAction::MoveActiveImageForward:
        if (const auto imageUid = activeImageUid()) moveImageForward(*imageUid);
        break;
      case MainMenuAction::MoveActiveImageToBack:
        if (const auto imageUid = activeImageUid()) moveImageToBack(*imageUid);
        break;
      case MainMenuAction::MoveActiveImageToFront:
        if (const auto imageUid = activeImageUid()) moveImageToFront(*imageUid);
        break;
      case MainMenuAction::ToggleActiveImageTransformationLock:
        if (const auto imageUid = activeImageUid()) {
          const Image* image = m_appData.image(*imageUid);
          if (image && m_setLockManualImageTransformation) {
            m_setLockManualImageTransformation(*imageUid, !image->transformations().is_worldDef_T_affine_locked());
          }
        }
        break;
      case MainMenuAction::ResetActiveImageManualTransformation:
        if (const auto imageUid = activeImageUid()) {
          Image* image = m_appData.image(*imageUid);
          if (image) {
            image->transformations().reset_worldDef_T_affine();
            if (m_updateImageUniforms) {
              m_updateImageUniforms(*imageUid);
            }
          }
        }
        break;
      case MainMenuAction::SaveActiveImageManualTransformation:
        if (const auto imageUid = activeImageUid()) {
          const Image* image = m_appData.image(*imageUid);
          const auto selectedFile = image ? native_dialog::saveFile(native_dialog::transformFilters()) : std::nullopt;
          if (selectedFile) {
            const glm::dmat4 worldDef_T_affine{image->transformations().get_worldDef_T_affine()};
            if (serialize::saveAffineTxFile(worldDef_T_affine, *selectedFile)) {
              spdlog::info("Saved manual transformation matrix to file {}", *selectedFile);
            }
            else {
              spdlog::error("Error saving manual transformation matrix to file {}", *selectedFile);
            }
          }
        }
        break;
      case MainMenuAction::SaveActiveImageInitialAndManualTransformation:
        if (const auto imageUid = activeImageUid()) {
          const Image* image = m_appData.image(*imageUid);
          const auto selectedFile = image ? native_dialog::saveFile(native_dialog::transformFilters()) : std::nullopt;
          if (selectedFile) {
            const auto& tx = image->transformations();
            const glm::dmat4 affine_T_subject{tx.get_affine_T_subject()};
            const glm::dmat4 worldDef_T_affine{tx.get_worldDef_T_affine()};
            if (serialize::saveAffineTxFile(worldDef_T_affine * affine_T_subject, *selectedFile)) {
              spdlog::info(
                "Saved concatenated initial and manual affine transformation matrix to file {}",
                *selectedFile);
            }
            else {
              spdlog::error(
                "Error saving concatenated initial and manual affine transformation matrix to file {}",
                *selectedFile);
            }
          }
        }
        break;
      case MainMenuAction::ShowOpacityMixer:
        m_appData.guiData().m_showOpacityBlenderWindow = !m_appData.guiData().m_showOpacityBlenderWindow;
        break;
      case MainMenuAction::CreateSegmentation:
        createActiveSegmentation();
        break;
      case MainMenuAction::SaveSegmentation:
        saveActiveSegmentation();
        break;
      case MainMenuAction::ClearSegmentation:
        if (const auto segUid = activeSegUid(); segUid && m_clearSeg) m_clearSeg(*segUid);
        break;
      case MainMenuAction::RemoveSegmentation:
        if (const auto segUid = activeSegUid(); segUid && m_removeSeg && m_removeSeg(*segUid)) {
          if (const auto imageUid = activeImageUid(); imageUid && m_updateImageUniforms)
            m_updateImageUniforms(*imageUid);
        }
        break;
      case MainMenuAction::PreviousForegroundLabel:
        m_callbackHandler.cycleForegroundSegLabel(-1);
        break;
      case MainMenuAction::NextForegroundLabel:
        m_callbackHandler.cycleForegroundSegLabel(1);
        break;
      case MainMenuAction::PreviousBackgroundLabel:
        m_callbackHandler.cycleBackgroundSegLabel(-1);
        break;
      case MainMenuAction::NextBackgroundLabel:
        m_callbackHandler.cycleBackgroundSegLabel(1);
        break;
      case MainMenuAction::DecreaseBrushSize:
        m_callbackHandler.cycleBrushSize(-1);
        break;
      case MainMenuAction::IncreaseBrushSize:
        m_callbackHandler.cycleBrushSize(1);
        break;
      case MainMenuAction::PaintSegmentationFromAnnotation:
        if (m_paintActiveSegmentationWithActivePolygon) m_paintActiveSegmentationWithActivePolygon();
        break;
      case MainMenuAction::SaveAnnotations:
        saveAllAnnotations();
        break;
      case MainMenuAction::RemoveAnnotation:
        if (const auto [imageUid, annotUid] = activeAnnotation(); imageUid && annotUid) {
          m_appData.removeAnnotation(*annotUid);
          ASM::synchronizeAnnotationHighlights();
        }
        break;
      case MainMenuAction::MoveAnnotationBackward:
        if (const auto [imageUid, annotUid] = activeAnnotation(); imageUid && annotUid) {
          m_appData.moveAnnotationBackwards(*imageUid, *annotUid);
        }
        break;
      case MainMenuAction::MoveAnnotationForward:
        if (const auto [imageUid, annotUid] = activeAnnotation(); imageUid && annotUid) {
          m_appData.moveAnnotationForwards(*imageUid, *annotUid);
        }
        break;
      case MainMenuAction::MoveAnnotationToBack:
        if (const auto [imageUid, annotUid] = activeAnnotation(); imageUid && annotUid) {
          m_appData.moveAnnotationToBack(*imageUid, *annotUid);
        }
        break;
      case MainMenuAction::MoveAnnotationToFront:
        if (const auto [imageUid, annotUid] = activeAnnotation(); imageUid && annotUid) {
          m_appData.moveAnnotationToFront(*imageUid, *annotUid);
        }
        break;
      case MainMenuAction::CreateLandmarkGroup:
        createActiveLandmarkGroup();
        break;
      case MainMenuAction::SaveLandmarkGroup:
        saveActiveLandmarkGroup();
        break;
      case MainMenuAction::AddLayout:
        m_appData.guiData().m_showAddLayoutPopup = true;
        break;
      case MainMenuAction::RemoveLayout: {
        auto& windowData = m_appData.windowData();
        if (windowData.numLayouts() >= 2) {
          const std::size_t layoutToDelete = windowData.currentLayoutIndex();
          windowData.cycleCurrentLayout(-1);
          windowData.removeLayout(layoutToDelete);
        }
        break;
      }
      case MainMenuAction::ToggleImagesWindow:
        m_appData.guiData().m_showImagePropertiesWindow = !m_appData.guiData().m_showImagePropertiesWindow;
        break;
      case MainMenuAction::ToggleSegmentationsWindow:
        m_appData.guiData().m_showSegmentationsWindow = !m_appData.guiData().m_showSegmentationsWindow;
        break;
      case MainMenuAction::ToggleLandmarksWindow:
        m_appData.guiData().m_showLandmarksWindow = !m_appData.guiData().m_showLandmarksWindow;
        break;
      case MainMenuAction::ToggleAnnotationsWindow:
        m_appData.guiData().m_showAnnotationsWindow = !m_appData.guiData().m_showAnnotationsWindow;
        break;
      case MainMenuAction::ToggleIsosurfacesWindow:
        m_appData.guiData().m_showIsosurfacesWindow = !m_appData.guiData().m_showIsosurfacesWindow;
        break;
      case MainMenuAction::ToggleSettingsWindow:
        m_appData.guiData().m_showSettingsWindow = !m_appData.guiData().m_showSettingsWindow;
        break;
      case MainMenuAction::ShowSettingsWindow:
        m_appData.guiData().m_showSettingsWindow = true;
        break;
      case MainMenuAction::ShowSynchronizeSettingsWindow:
        m_appData.guiData().m_showSettingsWindow = true;
        m_appData.guiData().m_requestedSettingsTab = GuiData::SettingsTab::Synchronize;
        break;
      case MainMenuAction::ToggleInspectorWindow:
        m_appData.guiData().m_showInspectionWindow = !m_appData.guiData().m_showInspectionWindow;
        break;
      case MainMenuAction::ToggleOpacityMixerWindow:
        m_appData.guiData().m_showOpacityBlenderWindow = !m_appData.guiData().m_showOpacityBlenderWindow;
        break;
      case MainMenuAction::ToggleImGuiDemoWindow:
        m_appData.guiData().m_showImGuiDemoWindow = !m_appData.guiData().m_showImGuiDemoWindow;
        break;
      case MainMenuAction::ToggleImPlotDemoWindow:
        m_appData.guiData().m_showImPlotDemoWindow = !m_appData.guiData().m_showImPlotDemoWindow;
        break;
      case MainMenuAction::ToggleToolbar:
        m_appData.guiData().m_showModeToolbar = !m_appData.guiData().m_showModeToolbar;
        if (m_readjustViewport) m_readjustViewport();
        break;
      case MainMenuAction::AddLandmark:
        addLandmarkAtCrosshairs();
        break;
    }
    if (m_postEmptyGlfwEvent) {
      m_postEmptyGlfwEvent();
    }
  };

  auto isMenuActionEnabled =
    [activeImageUid, activeSegUid, activeAnnotation, activeLandmarkGroupUid, projectHasAnnotations, this](
      MainMenuAction action) {
      const bool loaded = ProjectLoadState::Loaded == m_appData.state().projectLoadState();
      const bool backgroundTaskRunning = m_appData.state().animating();
      const bool canUseProjectActions = loaded && !backgroundTaskRunning;
      const bool hasActiveImage = activeImageUid().has_value();
      const bool hasActiveSeg = activeSegUid().has_value();
      const auto [annotImageUid, annotUid] = activeAnnotation();
      const bool hasActiveAnnotation = annotImageUid.has_value() && annotUid.has_value();
      switch (action) {
        case MainMenuAction::SetModePointer:
        case MainMenuAction::SetModeWindowLevel:
        case MainMenuAction::SetModeZoom:
        case MainMenuAction::SetModePan:
        case MainMenuAction::SetModeRotateView:
        case MainMenuAction::SetModeRotateCrosshairs:
        case MainMenuAction::SetModeSegment:
        case MainMenuAction::SetModeAnnotate:
        case MainMenuAction::SetModeTranslateImage:
        case MainMenuAction::SetModeRotateImage:
        case MainMenuAction::Recenter:
        case MainMenuAction::ResetView:
        case MainMenuAction::ToggleImageVisibility:
        case MainMenuAction::ToggleImageEdges:
        case MainMenuAction::ToggleScaleBars:
        case MainMenuAction::ToggleLightboxOffsets:
        case MainMenuAction::ToggleOverlays:
        case MainMenuAction::ToggleSync:
        case MainMenuAction::ToggleSyncSendCursor:
        case MainMenuAction::ToggleSyncReceiveCursor:
        case MainMenuAction::ToggleSyncSendZoom:
        case MainMenuAction::ToggleSyncReceiveZoom:
        case MainMenuAction::ToggleSyncSendPan:
        case MainMenuAction::ToggleSyncReceivePan:
        case MainMenuAction::ShowOpacityMixer:
        case MainMenuAction::CreateSegmentation:
        case MainMenuAction::CreateLandmarkGroup:
        case MainMenuAction::AddLayout:
          return canUseProjectActions && hasActiveImage;
        case MainMenuAction::PreviousForegroundLabel:
        case MainMenuAction::NextForegroundLabel:
        case MainMenuAction::PreviousBackgroundLabel:
        case MainMenuAction::NextBackgroundLabel:
        case MainMenuAction::DecreaseBrushSize:
        case MainMenuAction::IncreaseBrushSize:
          return canUseProjectActions && hasActiveSeg;
        case MainMenuAction::ToggleFullScreen:
        case MainMenuAction::ToggleImagesWindow:
        case MainMenuAction::ToggleSegmentationsWindow:
        case MainMenuAction::ToggleLandmarksWindow:
        case MainMenuAction::ToggleAnnotationsWindow:
        case MainMenuAction::ToggleIsosurfacesWindow:
        case MainMenuAction::ToggleSettingsWindow:
        case MainMenuAction::ShowSettingsWindow:
        case MainMenuAction::ShowSynchronizeSettingsWindow:
        case MainMenuAction::ToggleInspectorWindow:
        case MainMenuAction::ToggleOpacityMixerWindow:
        case MainMenuAction::ToggleImGuiDemoWindow:
        case MainMenuAction::ToggleImPlotDemoWindow:
        case MainMenuAction::ToggleToolbar:
          return !backgroundTaskRunning;
        case MainMenuAction::ToggleSegmentationVisibility:
        case MainMenuAction::ToggleSegmentationOutline:
        case MainMenuAction::DecreaseSegmentationOpacity:
        case MainMenuAction::IncreaseSegmentationOpacity:
        case MainMenuAction::SaveSegmentation:
        case MainMenuAction::ClearSegmentation:
          return canUseProjectActions && hasActiveSeg;
        case MainMenuAction::RemoveSegmentation:
          if (!canUseProjectActions || !hasActiveSeg || !hasActiveImage) return false;
          return m_appData.imageToSegUids(*activeImageUid()).size() > 1;
        case MainMenuAction::ExportActiveImage:
          if (!canUseProjectActions || !hasActiveImage) return false;
          return image_export::imageHasDicomSource(m_appData, *activeImageUid());
        case MainMenuAction::SetActiveImageAsReference:
        case MainMenuAction::RemoveActiveImage:
        case MainMenuAction::MoveActiveImageBackward:
        case MainMenuAction::MoveActiveImageForward:
        case MainMenuAction::MoveActiveImageToBack:
        case MainMenuAction::MoveActiveImageToFront:
          return canUseProjectActions && hasActiveImage;
        case MainMenuAction::ToggleActiveImageTransformationLock:
          if (!canUseProjectActions || !hasActiveImage || !m_setLockManualImageTransformation) return false;
          return activeImageUid() != m_appData.refImageUid();
        case MainMenuAction::ResetActiveImageManualTransformation:
        case MainMenuAction::SaveActiveImageManualTransformation:
          return canUseProjectActions && hasActiveImage && activeImageUid() != m_appData.refImageUid();
        case MainMenuAction::SaveActiveImageInitialAndManualTransformation:
          if (!canUseProjectActions || !hasActiveImage || activeImageUid() == m_appData.refImageUid()) return false;
          if (const Image* image = m_appData.image(*activeImageUid())) {
            return image->transformations().get_enable_affine_T_subject();
          }
          return false;
        case MainMenuAction::PaintSegmentationFromAnnotation:
          return canUseProjectActions && hasActiveSeg && hasActiveAnnotation;
        case MainMenuAction::SaveAnnotations:
          return canUseProjectActions && projectHasAnnotations();
        case MainMenuAction::RemoveAnnotation:
        case MainMenuAction::MoveAnnotationBackward:
        case MainMenuAction::MoveAnnotationForward:
        case MainMenuAction::MoveAnnotationToBack:
        case MainMenuAction::MoveAnnotationToFront:
          return canUseProjectActions && hasActiveAnnotation;
        case MainMenuAction::SaveLandmarkGroup:
          return canUseProjectActions && activeLandmarkGroupUid().has_value();
        case MainMenuAction::RemoveLayout:
          return canUseProjectActions && m_appData.windowData().numLayouts() >= 2;
        case MainMenuAction::AddLandmark:
          return canUseProjectActions && activeLandmarkGroupUid().has_value();
      }
      return false;
    };

  auto isMenuActionChecked = [this](MainMenuAction action) {
    switch (action) {
      case MainMenuAction::SetModePointer:
        return MouseMode::Pointer == m_appData.state().mouseMode();
      case MainMenuAction::SetModeWindowLevel:
        return MouseMode::WindowLevel == m_appData.state().mouseMode();
      case MainMenuAction::SetModeZoom:
        return MouseMode::CameraZoom == m_appData.state().mouseMode();
      case MainMenuAction::SetModePan:
        return MouseMode::CameraTranslate == m_appData.state().mouseMode();
      case MainMenuAction::SetModeRotateView:
        return MouseMode::CameraRotate == m_appData.state().mouseMode();
      case MainMenuAction::SetModeRotateCrosshairs:
        return MouseMode::CrosshairsRotate == m_appData.state().mouseMode();
      case MainMenuAction::SetModeSegment:
        return MouseMode::Segment == m_appData.state().mouseMode();
      case MainMenuAction::SetModeAnnotate:
        return MouseMode::Annotate == m_appData.state().mouseMode();
      case MainMenuAction::SetModeTranslateImage:
        return MouseMode::ImageTranslate == m_appData.state().mouseMode();
      case MainMenuAction::SetModeRotateImage:
        return MouseMode::ImageRotate == m_appData.state().mouseMode();
      case MainMenuAction::ToggleImageVisibility: {
        const auto imageUid = m_appData.activeImageUid();
        const Image* image = imageUid ? m_appData.image(*imageUid) : nullptr;
        if (!image) {
          return false;
        }
        const bool isMulticomponentImage =
          (image->header().numComponentsPerPixel() > 1 &&
           Image::MultiComponentBufferType::SeparateImages == image->bufferType());
        return isMulticomponentImage ? image->settings().globalVisibility() : image->settings().visibility();
      }
      case MainMenuAction::ToggleSegmentationVisibility: {
        const auto imageUid = m_appData.activeImageUid();
        const auto segUid = imageUid ? m_appData.imageToActiveSegUid(*imageUid) : std::nullopt;
        const Image* seg = segUid ? m_appData.seg(*segUid) : nullptr;
        return seg ? seg->settings().visibility() : false;
      }
      case MainMenuAction::ToggleImageEdges: {
        const auto imageUid = m_appData.activeImageUid();
        const Image* image = imageUid ? m_appData.image(*imageUid) : nullptr;
        return image ? image->settings().showEdges() : false;
      }
      case MainMenuAction::ToggleSegmentationOutline:
        return SegmentationOutlineStyle::Disabled != m_appData.renderData().m_segOutlineStyle;
      case MainMenuAction::ToggleActiveImageTransformationLock: {
        const auto imageUid = m_appData.activeImageUid();
        const Image* image = imageUid ? m_appData.image(*imageUid) : nullptr;
        return image ? image->transformations().is_worldDef_T_affine_locked() : false;
      }
      case MainMenuAction::ToggleScaleBars:
        return m_appData.renderData().m_showScaleBars;
      case MainMenuAction::ToggleLightboxOffsets:
        return m_appData.renderData().m_showLightboxOffsetLabels;
      case MainMenuAction::ToggleOverlays:
        return m_getOverlayVisibility ? m_getOverlayVisibility() : false;
      case MainMenuAction::ToggleSync:
        return m_appData.settings().cursorSyncEnabled();
      case MainMenuAction::ToggleSyncSendCursor:
        return m_appData.settings().sendCursorSync();
      case MainMenuAction::ToggleSyncReceiveCursor:
        return m_appData.settings().receiveCursorSync();
      case MainMenuAction::ToggleSyncSendZoom:
        return m_appData.settings().sendZoomSync();
      case MainMenuAction::ToggleSyncReceiveZoom:
        return m_appData.settings().receiveZoomSync();
      case MainMenuAction::ToggleSyncSendPan:
        return m_appData.settings().sendPanSync();
      case MainMenuAction::ToggleSyncReceivePan:
        return m_appData.settings().receivePanSync();
      case MainMenuAction::ToggleToolbar:
        return m_appData.guiData().m_showModeToolbar;
      case MainMenuAction::ToggleImagesWindow:
        return m_appData.guiData().m_showImagePropertiesWindow;
      case MainMenuAction::ToggleSegmentationsWindow:
        return m_appData.guiData().m_showSegmentationsWindow;
      case MainMenuAction::ToggleLandmarksWindow:
        return m_appData.guiData().m_showLandmarksWindow;
      case MainMenuAction::ToggleAnnotationsWindow:
        return m_appData.guiData().m_showAnnotationsWindow;
      case MainMenuAction::ToggleIsosurfacesWindow:
        return m_appData.guiData().m_showIsosurfacesWindow;
      case MainMenuAction::ToggleSettingsWindow:
        return m_appData.guiData().m_showSettingsWindow;
      case MainMenuAction::ToggleInspectorWindow:
        return m_appData.guiData().m_showInspectionWindow;
      case MainMenuAction::ToggleOpacityMixerWindow:
        return m_appData.guiData().m_showOpacityBlenderWindow;
      case MainMenuAction::ShowOpacityMixer:
        return m_appData.guiData().m_showOpacityBlenderWindow;
      case MainMenuAction::ToggleImGuiDemoWindow:
        return m_appData.guiData().m_showImGuiDemoWindow;
      case MainMenuAction::ToggleImPlotDemoWindow:
        return m_appData.guiData().m_showImPlotDemoWindow;
      default:
        return false;
    }
  };

  auto applyImageSelectionAndRenderModesToAllViews = [this](const uuids::uuid& viewUid) {
    m_appData.windowData().applyImageSelectionToAllCurrentViews(viewUid);
    m_appData.windowData().applyViewRenderModeAndProjectionToAllCurrentViews(viewUid);
  };

  auto getViewCameraRotation = [this](const uuids::uuid& viewUid) -> glm::quat {
    const View* view = m_appData.windowData().getCurrentView(viewUid);
    if (!view) return sk_identityRotation;

    return helper::computeCameraRotationRelativeToWorld(view->camera());
  };

  auto setViewCameraRotation = [this](const uuids::uuid& viewUid, const glm::quat& camera_T_world_rotationDelta) {
    m_callbackHandler.doCameraRotate3d(viewUid, camera_T_world_rotationDelta);
  };

  auto setViewCameraDirection = [this](const uuids::uuid& viewUid, const glm::vec3& worldFwdDirection) {
    m_callbackHandler.handleSetViewForwardDirection(viewUid, worldFwdDirection);
  };

  auto getViewNormal = [this](const uuids::uuid& viewUid) {
    View* view = m_appData.windowData().getCurrentView(viewUid);
    if (!view) return sk_zeroVec;
    return helper::worldDirection(view->camera(), Directions::View::Back);
  };

  auto getObliqueViewDirections = [this](const uuids::uuid& viewUidToExclude) -> std::vector<glm::vec3> {
    std::vector<glm::vec3> obliqueViewDirections;

    for (std::size_t i = 0; i < m_appData.windowData().numLayouts(); ++i) {
      const Layout* layout = m_appData.windowData().layout(i);
      if (!layout) continue;

      for (const auto& view : layout->views()) {
        if (view.first == viewUidToExclude) continue;
        if (!view.second) continue;

        if (!helper::looksAlongOrthogonalAxis(view.second->camera())) {
          obliqueViewDirections.emplace_back(helper::worldDirection(view.second->camera(), Directions::View::Front));
        }
      }
    }

    return obliqueViewDirections;
  };

  ImGui::NewFrame();

  const ProjectLoadState projectLoadState = m_appData.state().projectLoadState();
  const bool backgroundTaskRunning = m_appData.state().animating();
  const bool hasLoadedProject =
    ProjectLoadState::Loaded == projectLoadState && 0 != m_appData.windowData().numLayouts();

  auto defaultProjectSaveDirectory = [this]() {
    if (m_appData.projectFileName()) {
      return m_appData.projectFileName()->parent_path();
    }
    const auto refImageUid = m_appData.refImageUid();
    const Image* refImage = refImageUid ? m_appData.image(*refImageUid) : nullptr;
    return refImage ? refImage->header().fileName().parent_path() : fs::path{};
  };

  auto defaultProjectSaveName = [this]() {
    if (m_appData.projectFileName()) {
      return m_appData.projectFileName()->filename().string();
    }
    const auto refImageUid = m_appData.refImageUid();
    const Image* refImage = refImageUid ? m_appData.image(*refImageUid) : nullptr;
    return refImage ? (refImage->header().fileName().stem().string() + ".json") : std::string{"project.json"};
  };

  auto defaultLayoutsSaveDirectory = defaultProjectSaveDirectory;
  auto defaultLayoutsSaveName = [this]() {
    if (m_appData.projectFileName()) {
      return m_appData.projectFileName()->stem().string() + "-layouts.json";
    }
    const auto refImageUid = m_appData.refImageUid();
    const Image* refImage = refImageUid ? m_appData.image(*refImageUid) : nullptr;
    return refImage ? (refImage->header().fileName().stem().string() + "-layouts.json") : std::string{"layouts.json"};
  };

  auto layoutNames = [this]() {
    const auto& layouts = m_appData.windowData().layouts();
    std::vector<std::string> baseNames;
    baseNames.reserve(layouts.size());

    std::unordered_map<std::string, std::size_t> baseNameCounts;
    for (std::size_t index = 0; index < layouts.size(); ++index) {
      baseNames.emplace_back(m_appData.windowData().layoutDisplayName(index));
      ++baseNameCounts[baseNames.back()];
    }

    std::vector<std::string> names;
    names.reserve(layouts.size());
    for (std::size_t index = 0; index < layouts.size(); ++index) {
      std::string displayName = baseNames.at(index);
      if (baseNameCounts.at(displayName) > 1) {
        const std::string imageName = layoutImageDisplayName(m_appData, layouts.at(index));
        if (!imageName.empty()) {
          displayName += " - " + imageName;
        }
      }
      names.emplace_back(std::to_string(index + 1) + ". " + displayName);
    }
    return names;
  };

  renderConfirmCloseAppPopup(m_appData, m_quitAppWithoutPrompt);
  renderUnsavedProjectPopup(
    m_appData,
    m_saveProject,
    m_saveProjectAs,
    m_closeProjectWithoutPrompt,
    m_quitAppWithoutPrompt,
    defaultProjectSaveDirectory,
    defaultProjectSaveName);

  if (m_appData.guiData().m_renderUiWindows) {
    renderConfirmSetReferenceImagePopup(m_appData, m_setReferenceImage);
    renderConfirmRemoveImagePopup(m_appData, m_removeImage);
    renderLargeImageLoadPromptPopup(m_appData, m_largeImageLoadDecision);
    renderDicomFolderPathPopup(m_appData, m_openDicomFolders);
    renderDicomSeriesSelectionPopup(m_appData, m_loadDicomSeries);

    if (m_appData.guiData().m_showImGuiDemoWindow) {
      ImGui::ShowDemoWindow(&m_appData.guiData().m_showImGuiDemoWindow);
    }

    if (m_appData.guiData().m_showImPlotDemoWindow) {
      ImPlot::ShowDemoWindow(&m_appData.guiData().m_showImPlotDemoWindow);
    }

    const MainMenuBarCallbacks mainMenuCallbacks{
      .openImageFiles = m_openImageFiles,
      .addImageFiles = m_addImageFiles,
      .openDicomFolders = m_openDicomFolders,
      .requestDicomFolderPathDialog = [this]() { m_appData.guiData().m_showDicomFolderPathPopup = true; },
      .addSegmentationFile = m_addSegmentationFile,
      .openProjectFile = m_openProjectFile,
      .saveProject = m_saveProject,
      .saveProjectAs = m_saveProjectAs,
      .projectFileName = [this]() { return m_appData.projectFileName(); },
      .defaultProjectSaveDirectory = defaultProjectSaveDirectory,
      .defaultProjectSaveName = defaultProjectSaveName,
      .closeProject = m_closeProject,
      .quitApp = m_requestQuitApp,
      .loadLayoutsFile = m_loadLayoutsFile,
      .saveLayoutsFile = m_saveLayoutsFile,
      .defaultLayoutsSaveDirectory = defaultLayoutsSaveDirectory,
      .defaultLayoutsSaveName = defaultLayoutsSaveName,
      .layoutNames = layoutNames,
      .currentLayoutIndex = [this]() { return m_appData.windowData().currentLayoutIndex(); },
      .setCurrentLayoutIndex =
        [this](std::size_t index) {
          m_appData.windowData().setCurrentLayoutIndex(index);
          if (m_postEmptyGlfwEvent) {
            m_postEmptyGlfwEvent();
          }
        },
      .cycleLayouts =
        [this](int step) {
          m_appData.windowData().cycleCurrentLayout(step);
          if (m_postEmptyGlfwEvent) {
            m_postEmptyGlfwEvent();
          }
        },
      .imageNames =
        [this]() {
          std::vector<std::string> names;
          names.reserve(m_appData.numImages());
          for (std::size_t i = 0; i < m_appData.numImages(); ++i) {
            names.emplace_back(std::to_string(i + 1) + ". " + getImageDisplayAndFileNames(i).first);
          }
          return names;
        },
      .activeImageIndex = getActiveImageIndex,
      .setActiveImageIndex = setActiveImageIndex,
      .performAction = performMenuAction,
      .isActionEnabled = isMenuActionEnabled,
      .isActionChecked = isMenuActionChecked,
      .showAbout = [this]() { m_appData.guiData().m_showAboutDialog = true; },
      .canOpenProject = ProjectLoadState::Loading != projectLoadState && !backgroundTaskRunning,
      .canAddImage = ProjectLoadState::Loaded == projectLoadState && !backgroundTaskRunning,
      .canAddSegmentation =
        ProjectLoadState::Loaded == projectLoadState && !backgroundTaskRunning && m_appData.activeImageUid(),
      .canSaveProject = ProjectLoadState::Loaded == projectLoadState && !backgroundTaskRunning,
      .canCloseProject = ProjectLoadState::Empty != projectLoadState && !backgroundTaskRunning,
      .canUseLayouts = ProjectLoadState::Loaded == projectLoadState && !backgroundTaskRunning};

#ifdef __APPLE__
    updateMacOSNativeMainMenu(mainMenuCallbacks);
#elif defined(_WIN32)
    updateWindowsNativeMainMenu(m_window, mainMenuCallbacks);
#else
    renderMainMenuBar(m_appData.guiData(), mainMenuCallbacks);
#endif

    renderEmptyWorkspace(
      projectLoadState,
      m_openImageFiles,
      m_openDicomFolders,
      [this]() { m_appData.guiData().m_showDicomFolderPathPopup = true; },
      m_openProjectFile);

    if (ProjectLoadState::Loaded == projectLoadState && backgroundTaskRunning) {
      renderLoadingStatusBar();
    }

    if (!hasLoadedProject) {
      if (
        m_postEmptyGlfwEvent &&
        (ImGui::IsAnyItemActive() || ImGui::IsMouseClicked(ImGuiMouseButton_Left) ||
         ImGui::IsMouseReleased(ImGuiMouseButton_Left) || ImGui::IsMouseClicked(ImGuiMouseButton_Right) ||
         ImGui::IsMouseReleased(ImGuiMouseButton_Right)))
      {
        m_postEmptyGlfwEvent();
      }
      ImGui::Render();
      ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
      return;
    }

    if (m_appData.guiData().m_showIsosurfacesWindow) {
      renderIsosurfacesWindow(
        m_appData,
        std::bind(&ImGuiWrapper::storeFuture, this, _1, _2),
        std::bind(&ImGuiWrapper::addTaskToIsosurfaceGpuMeshGenerationQueue, this, _1));
    }

    if (m_appData.guiData().m_showSettingsWindow) {
      renderSettingsWindow(
        m_appData,
        getNumImageColorMaps,
        getImageColorMap,
        m_updateMetricUniforms,
        m_recenterAllViews);
    }

    using namespace std::placeholders;

    if (m_appData.guiData().m_showInspectionWindow) {
      renderInspectionWindowWithTable(
        m_appData,
        std::bind(&ImGuiWrapper::getImageDisplayAndFileNames, this, _1),
        m_getSubjectPos,
        m_getVoxelPos,
        m_setSubjectPos,
        m_setVoxelPos,
        m_getImageValuesNN,
        m_getImageValuesLinear,
        m_getSegLabel,
        getLabelTable);
    }

    if (m_appData.guiData().m_showImagePropertiesWindow) {
      renderImagePropertiesWindow(
        m_appData,
        m_appData.numImages(),
        std::bind(&ImGuiWrapper::getImageDisplayAndFileNames, this, _1),
        getActiveImageIndex,
        setActiveImageIndex,
        getNumImageColorMaps,
        getImageColorMap,
        moveImageBackward,
        moveImageForward,
        moveImageToBack,
        moveImageToFront,
        m_updateAllImageUniforms,
        m_updateImageUniforms,
        m_updateImageInterpolationMode,
        m_updateImageColorMapInterpolationMode,
        m_setLockManualImageTransformation,
        [this](const uuids::uuid& imageUid) {
          m_appData.guiData().m_pendingReferenceImageUid = imageUid;
          m_appData.guiData().m_showConfirmSetReferenceImagePopup = true;
        },
        [this](const uuids::uuid& imageUid) {
          m_appData.guiData().m_pendingRemoveImageUid = imageUid;
          m_appData.guiData().m_showConfirmRemoveImagePopup = true;
        },
        m_recenterAllViews);
    }

    if (m_appData.guiData().m_showSegmentationsWindow) {
      renderSegmentationPropertiesWindow(
        m_appData,
        getLabelTable,
        m_updateImageUniforms,
        m_updateLabelColorTableTexture,
        m_moveCrosshairsToSegLabelCentroid,
        m_createBlankSeg,
        m_addSegmentationFileToImage,
        m_clearSeg,
        m_removeSeg,
        m_recenterAllViews);
    }

    if (m_appData.guiData().m_showLandmarksWindow) {
      renderLandmarkPropertiesWindow(m_appData, m_recenterAllViews);
    }

    if (m_appData.guiData().m_showAnnotationsWindow) {
      renderAnnotationWindow(
        m_appData,
        setViewCameraDirection,
        m_paintActiveSegmentationWithActivePolygon,
        m_recenterAllViews);
    }

    if (m_appData.guiData().m_showOpacityBlenderWindow) {
      renderOpacityBlenderWindow(m_appData, m_updateImageUniforms);
    }

    renderModeToolbar(
      m_appData,
      getMouseMode,
      setMouseMode,
      m_readjustViewport,
      m_recenterAllViews,
      m_getOverlayVisibility,
      m_setOverlayVisibility,
      cycleViewLayout,

      m_appData.numImages(),
      std::bind(&ImGuiWrapper::getImageDisplayAndFileNames, this, _1),
      getActiveImageIndex,
      setActiveImageIndex);

    renderAddLayoutModalPopup(m_appData, m_appData.guiData().m_showAddLayoutPopup, [this]() {
      if (m_recenterAllViews) {
        m_recenterAllViews(false, false, true, false, true);
      }
    });
    m_appData.guiData().m_showAddLayoutPopup = false;

    renderAboutDialogModalPopup(m_appData.guiData().m_showAboutDialog);
    m_appData.guiData().m_showAboutDialog = false;

    renderSegToolbar(
      m_appData,
      m_appData.numImages(),
      std::bind(&ImGuiWrapper::getImageDisplayAndFileNames, this, _1),
      getActiveImageIndex,
      setActiveImageIndex,
      getImageHasActiveSeg,
      setImageHasActiveSeg,
      m_readjustViewport,
      m_executePoissonSeg);

    annotationToolbar(m_paintActiveSegmentationWithActivePolygon);
  }

  if (ProjectLoadState::Loaded != m_appData.state().projectLoadState() || 0 == m_appData.windowData().numLayouts()) {
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    return;
  }

  Layout& currentLayout = m_appData.windowData().currentLayout();

  const float wholeWindowHeight = static_cast<float>(m_appData.windowData().getWindowSize().y);

  if (m_appData.guiData().m_renderUiOverlays && currentLayout.isLightbox()) {
    // Per-layout UI controls:

    static constexpr bool sk_recenterCrosshairs = false;
    static constexpr bool sk_realignCrosshairs = false;
    static constexpr bool sk_doNotRecenterOnCurrentCrosshairsPosition = false;
    static constexpr bool sk_resetObliqueOrientation = false;
    static constexpr bool sk_resetZoom = true;

    const auto mindowFrameBounds = helper::computeMindowFrameBounds(
      currentLayout.windowClipViewport(),
      m_appData.windowData().viewport().getAsVec4(),
      wholeWindowHeight);

    renderViewSettingsComboWindow(
      currentLayout.uid(),
      mindowFrameBounds,
      currentLayout.uiControls(),
      true,
      false,

      m_appData.state().worldCrosshairs(),
      m_appData.windowData().getContentScaleRatios(),

      m_appData.numImages(),
      [this, &currentLayout](std::size_t index) { return currentLayout.isImageRendered(m_appData, index); },
      [this, &currentLayout](std::size_t index, bool visible) {
        currentLayout.setImageRendered(m_appData, index, visible);
      },

      [this, &currentLayout](std::size_t index) { return currentLayout.isImageUsedForMetric(m_appData, index); },
      [this, &currentLayout](std::size_t index, bool visible) {
        currentLayout.setImageUsedForMetric(m_appData, index, visible);
      },
      std::bind(&ImGuiWrapper::getImageDisplayAndFileNames, this, _1),

      getImageIsVisibleSetting,
      getImageIsActive,

      currentLayout.viewType(),
      currentLayout.renderMode(),
      currentLayout.intensityProjectionMode(),

      [&currentLayout](const ViewType& viewType) { return currentLayout.setViewType(viewType); },
      [&currentLayout](const ViewRenderMode& renderMode) { return currentLayout.setRenderMode(renderMode); },
      [&currentLayout](const IntensityProjectionMode& ipMode) {
        return currentLayout.setIntensityProjectionMode(ipMode);
      },

      [this]() {
        m_recenterAllViews(
          sk_recenterCrosshairs,
          sk_realignCrosshairs,
          sk_doNotRecenterOnCurrentCrosshairsPosition,
          sk_resetObliqueOrientation,
          sk_resetZoom);
      },
      nullptr,

      [this]() { return m_appData.renderData().m_intensityProjectionSlabThickness; },
      [this](float thickness) { m_appData.renderData().m_intensityProjectionSlabThickness = thickness; },
      [this]() { return m_appData.renderData().m_doMaxExtentIntensityProjection; },
      [this](bool set) { m_appData.renderData().m_doMaxExtentIntensityProjection = set; },

      [this]() { return m_appData.renderData().m_xrayIntensityWindow; },
      [this](float window) { m_appData.renderData().m_xrayIntensityWindow = window; },
      [this]() { return m_appData.renderData().m_xrayIntensityLevel; },
      [this](float level) { m_appData.renderData().m_xrayIntensityLevel = level; },
      [this]() { return m_appData.renderData().m_xrayEnergyKeV; },
      [this](float energy) { m_appData.renderData().setXrayEnergy(energy); });

    renderViewOrientationToolWindow(
      currentLayout.uid(),
      mindowFrameBounds,
      currentLayout.uiControls(),
      true,
      currentLayout.viewType(),
      [&getViewCameraRotation, &currentLayout]() { return getViewCameraRotation(currentLayout.uid()); },
      [&setViewCameraRotation, &currentLayout](const glm::quat& q) {
        return setViewCameraRotation(currentLayout.uid(), q);
      },
      [&setViewCameraDirection, &currentLayout](const glm::vec3& dir) {
        return setViewCameraDirection(currentLayout.uid(), dir);
      },
      [&getViewNormal, &currentLayout]() { return getViewNormal(currentLayout.uid()); },
      getObliqueViewDirections);
  }
  else if (m_appData.guiData().m_renderUiOverlays && !currentLayout.isLightbox()) {
    // Per-view UI controls:

    for (const auto& viewUid : m_appData.windowData().currentViewUids()) {
      View* view = m_appData.windowData().getCurrentView(viewUid);
      if (!view) return;

      auto setViewType = [view](const ViewType& viewType) {
        if (view) view->setViewType(viewType);
      };

      auto setRenderMode = [view](const ViewRenderMode& renderMode) {
        if (view) view->setRenderMode(renderMode);
      };

      auto setIntensityProjectionMode = [view](const IntensityProjectionMode& ipMode) {
        if (view) view->setIntensityProjectionMode(ipMode);
      };

      auto recenter = [this, &viewUid]() {
        m_recenterView(viewUid);
      };

      const auto mindowFrameBounds = helper::computeMindowFrameBounds(
        view->windowClipViewport(),
        m_appData.windowData().viewport().getAsVec4(),
        wholeWindowHeight);

      renderViewSettingsComboWindow(
        viewUid,
        mindowFrameBounds,
        view->uiControls(),
        false,
        true,

        m_appData.state().worldCrosshairs(),
        m_appData.windowData().getContentScaleRatios(),

        m_appData.numImages(),
        [this, view](std::size_t index) { return view->isImageRendered(m_appData, index); },
        [this, view](std::size_t index, bool visible) { view->setImageRendered(m_appData, index, visible); },

        [this, view](std::size_t index) { return view->isImageUsedForMetric(m_appData, index); },
        [this, view](std::size_t index, bool visible) { view->setImageUsedForMetric(m_appData, index, visible); },

        std::bind(&ImGuiWrapper::getImageDisplayAndFileNames, this, _1),
        getImageIsVisibleSetting,
        getImageIsActive,

        view->viewType(),
        view->renderMode(),
        view->intensityProjectionMode(),

        setViewType,
        setRenderMode,
        setIntensityProjectionMode,

        recenter,
        applyImageSelectionAndRenderModesToAllViews,

        [this]() { return m_appData.renderData().m_intensityProjectionSlabThickness; },
        [this](float thickness) { m_appData.renderData().m_intensityProjectionSlabThickness = thickness; },
        [this]() { return m_appData.renderData().m_doMaxExtentIntensityProjection; },
        [this](bool set) { m_appData.renderData().m_doMaxExtentIntensityProjection = set; },

        [this]() { return m_appData.renderData().m_xrayIntensityWindow; },
        [this](float window) { m_appData.renderData().m_xrayIntensityWindow = window; },
        [this]() { return m_appData.renderData().m_xrayIntensityLevel; },
        [this](float level) { m_appData.renderData().m_xrayIntensityLevel = level; },
        [this]() { return m_appData.renderData().m_xrayEnergyKeV; },
        [this](float energy) { m_appData.renderData().setXrayEnergy(energy); });

      renderViewOrientationToolWindow(
        viewUid,
        mindowFrameBounds,
        view->uiControls(),
        false,
        view->viewType(),
        [&getViewCameraRotation, &viewUid]() { return getViewCameraRotation(viewUid); },
        [&setViewCameraRotation, &viewUid](const glm::quat& q) { return setViewCameraRotation(viewUid, q); },
        [&setViewCameraDirection, &viewUid](const glm::vec3& dir) { return setViewCameraDirection(viewUid, dir); },
        [&getViewNormal, &viewUid]() { return getViewNormal(viewUid); },
        getObliqueViewDirections);
    }
  }

  m_callbackHandler.refreshBrushPreviewIfNeeded();

  if (
    m_postEmptyGlfwEvent &&
    (ImGui::IsAnyItemActive() || ImGui::IsMouseClicked(ImGuiMouseButton_Left) ||
     ImGui::IsMouseReleased(ImGuiMouseButton_Left) || ImGui::IsMouseClicked(ImGuiMouseButton_Right) ||
     ImGui::IsMouseReleased(ImGuiMouseButton_Right)))
  {
    m_postEmptyGlfwEvent();
  }

  ImGui::Render();
  ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

void ImGuiWrapper::annotationToolbar(const std::function<void()> paintActiveAnnotation)
{
  if (!state::annot::isInStateWhereToolbarVisible()) {
    return;
  }

  if (!ASM::current_state_ptr || !ASM::current_state_ptr->selectedViewUid()) {
    return;
  }

  // Position the annotation toolbar at the bottom of this view:
  const View* annotationView = m_appData.windowData().getView(*ASM::current_state_ptr->selectedViewUid());

  const float wholeWindowHeight = static_cast<float>(m_appData.windowData().getWindowSize().y);

  const auto mindowAnnotViewFrameBounds = helper::computeMindowFrameBounds(
    annotationView->windowClipViewport(),
    m_appData.windowData().viewport().getAsVec4(),
    wholeWindowHeight);

  renderAnnotationToolbar(m_appData, mindowAnnotViewFrameBounds, paintActiveAnnotation);
}
