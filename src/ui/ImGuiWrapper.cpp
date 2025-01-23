#include "ui/ImGuiWrapper.h"

#include "ui/Helpers.h"
#include "ui/MainMenuBar.h"
#include "ui/Popups.h"
#include "ui/Style.h"
#include "ui/Toolbars.h"
#include "ui/Windows.h"

#include "logic/app/CallbackHandler.h"
#include "logic/app/Data.h"
#include "logic/camera/CameraHelpers.h"
#include "logic/states/AnnotationStateHelpers.h"
#include "logic/states/AnnotationStateMachine.h"

#include "rendering/utility/CreateGLObjects.h"

#include <IconFontCppHeaders/IconsForkAwesome.h>

#include <imgui/imgui.h>

// GLFW and OpenGL 3 bindings for ImGui:
#include <imgui/backends/imgui_impl_glfw.h>
#include <imgui/backends/imgui_impl_opengl3.h>

#include <implot/implot.h>

#include <cmrc/cmrc.hpp>

#include <glm/glm.hpp>

#include <spdlog/fmt/ostr.h>
#include <spdlog/spdlog.h>

CMRC_DECLARE(fonts);

namespace
{

static const glm::quat sk_identityRotation{1.0f, 0.0f, 0.0f, 0.0f};
static const glm::vec3 sk_zeroVec{0.0f, 0.0f, 0.0f};

ImFont* loadFont(
  const std::string& fontPath,
  const ImFontConfig& fontConfig,
  float fontSize,
  const ImWchar* glyphRange = nullptr
)
{
  auto filesystem = cmrc::fonts::get_filesystem();

  cmrc::file fontFile = filesystem.open(fontPath);

  // ImGui will take ownership of the font (and be responsible for deleting it), so make a copy:
  char* fontData = new char[fontFile.size()];

  for (std::size_t i = 0; i < fontFile.size(); ++i)
  {
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
    glyphRange
  );
}

} // namespace

ImGuiWrapper::ImGuiWrapper(GLFWwindow* window, AppData& appData, CallbackHandler& callbackHandler)
  : m_appData(appData)
  , m_callbackHandler(callbackHandler)
  , m_contentScale(1.0f)
{
  IMGUI_CHECKVERSION();

  ImGui::CreateContext();
  spdlog::debug("Created ImGui context");

  ImPlot::CreateContext();
  spdlog::debug("Created ImPlot context");

  ImGuiIO& io = ImGui::GetIO();
  io.IniFilename = "entropy_ui.ini";
  io.LogFilename = "logs/entropy_ui.log";

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
  std::function<void(const uuids::uuid& viewUid)> recenterView,
  AllViewsRecenterType recenterCurrentViews,
  std::function<bool(void)> getOverlayVisibility,
  std::function<void(bool)> setOverlayVisibility,
  std::function<void(void)> updateAllImageUniforms,
  std::function<void(const uuids::uuid& viewUid)> updateImageUniforms,
  std::function<void(const uuids::uuid& imageUid)> updateImageInterpolationMode,
  std::function<void(std::size_t cmapIndex)> updateImageColorMapInterpolationMode,
  std::function<void(std::size_t tableIndex)> updateLabelColorTableTexture,
  std::function<void(const uuids::uuid& imageUid, std::size_t labelIndex)>
    moveCrosshairsToSegLabelCentroid,
  std::function<void()> updateMetricUniforms,
  std::function<glm::vec3()> getWorldDeformedPos,
  std::function<std::optional<glm::vec3>(std::size_t imageIndex)> getSubjectPos,
  std::function<std::optional<glm::ivec3>(std::size_t imageIndex)> getVoxelPos,
  std::function<void(std::size_t imageIndex, const glm::vec3& subjectPos)> setSubjectPos,
  std::function<void(std::size_t imageIndex, const glm::ivec3& voxelPos)> setVoxelPos,
  std::function<std::vector<double>(std::size_t imageIndex, bool getOnlyActiveComponent)>
    getImageValuesNN,
  std::function<std::vector<double>(std::size_t imageIndex, bool getOnlyActiveComponent)>
    getImageValuesLinear,
  std::function<std::optional<int64_t>(std::size_t imageIndex)> getSegLabel,
  std::function<std::optional<uuids::uuid>(
    const uuids::uuid& matchingImageUid, const std::string& segDisplayName
  )> createBlankSeg,
  std::function<bool(const uuids::uuid& segUid)> clearSeg,
  std::function<bool(const uuids::uuid& segUid)> removeSeg,
  std::function<
    bool(const uuids::uuid& imageUid, const uuids::uuid& seedSegUid, const SeedSegmentationType&)>
    executeGraphCutsSeg,
  std::function<
    bool(const uuids::uuid& imageUid, const uuids::uuid& seedSegUid, const SeedSegmentationType&)>
    executePoissonSeg,
  std::function<bool(const uuids::uuid& imageUid, bool locked)> setLockManualImageTransformation,
  std::function<void()> paintActiveSegmentationWithActivePolygon
)
{
  m_postEmptyGlfwEvent = postEmptyGlfwEvent;
  m_readjustViewport = readjustViewport;
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
  m_executeGraphCutsSeg = executeGraphCutsSeg;
  m_executePoissonSeg = executePoissonSeg;
  m_setLockManualImageTransformation = setLockManualImageTransformation;
  m_paintActiveSegmentationWithActivePolygon = paintActiveSegmentationWithActivePolygon;
}

void ImGuiWrapper::storeFuture(const uuids::uuid& taskUid, std::future<AsyncTaskDetails> future)
{
  std::lock_guard<std::mutex> lock(m_futuresMutex);

  if (!future.valid())
  {
    spdlog::warn("Future for task {} is not valid", taskUid);
    return;
  }

  m_futures.emplace(taskUid, std::move(future));

  spdlog::debug(
    "Storing future for UI task {}. Total number of UI task futures: {}", taskUid, m_futures.size()
  );
}

void ImGuiWrapper::addTaskToIsosurfaceGpuMeshGenerationQueue(const uuids::uuid& taskUid)
{
  std::lock_guard<std::mutex> lock(m_isosurfaceTaskQueueMutex);

  m_isosurfaceTaskQueueForGpuMeshGeneration.push(taskUid);

  // Post an empty event to notify render thread
  if (m_postEmptyGlfwEvent)
  {
    m_postEmptyGlfwEvent();
  }
}

void ImGuiWrapper::generateIsosurfaceMeshGpuRecords()
{
  std::lock_guard<std::mutex> lock(m_isosurfaceTaskQueueMutex);

  while (!m_isosurfaceTaskQueueForGpuMeshGeneration.empty())
  {
    const uuids::uuid taskUid = m_isosurfaceTaskQueueForGpuMeshGeneration.front();
    m_isosurfaceTaskQueueForGpuMeshGeneration.pop();

    auto it = m_futures.find(taskUid);

    if (std::end(m_futures) == it)
    {
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

    if ( AsyncTasks::IsosurfaceMeshGeneration != value.task ||
            false == value.success ||
            ! value.imageUid || ! value.imageComponent || ! value.objectUid )
    {
      spdlog::error("Failed task {}", taskUid);
      continue;
    }

    spdlog::info("Task {}: Start generating GPU mesh for isosurface {} ", taskUid, *value.objectUid);

    // Get the isosurface associated with this task
    const Isosurface* surface
      = m_appData.isosurface(*value.imageUid, *value.imageComponent, *value.objectUid);

    if (!surface)
    {
      spdlog::error(
        "Null isosurface for isosurface {} of image {}", *value.objectUid, *value.imageUid
      );
      continue;
    }

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
  }
}

/*
Q: How should I handle DPI in my application?
The short answer is: obtain the desired DPI scale, load your fonts resized with that scale (always round down fonts
size to the nearest integer), and scale your Style structure accordingly using style.ScaleAllSizes().

Your application may want to detect DPI change and reload the fonts and reset style between frames.

Your ui code should avoid using hardcoded constants for size and positioning. Prefer to express values as multiple of
reference values such as ImGui::GetFontSize() or ImGui::GetFrameHeight(). So e.g. instead of seeing a hardcoded height of
500 for a given item/window, you may want to use 30*ImGui::GetFontSize() instead.
*/
void ImGuiWrapper::setContentScale(float scale)
{
  if (m_contentScale == scale)
  {
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
  static const std::string cousineFontPath("resources/fonts/Cousine/Cousine-Regular.ttf");
  static const std::string helveticaFontPath("resources/fonts/HelveticaNeue/HelveticaNeue-Light.ttf"
  );
  static const std::string spaceGroteskFontPath(
    "resources/fonts/SpaceGrotesk/SpaceGrotesk-Light.ttf"
  );
  static const std::string sfMonoFontPath("resources/fonts/SFMono/SFMono-Regular.ttf");
  static const std::string sfProFontPath("resources/fonts/SFPro/sf-pro-text-regular.ttf");
  static const std::string forkAwesomeFontPath = std::string("resources/fonts/ForkAwesome/")
                                                 + FONT_ICON_FILE_NAME_FK;

  spdlog::debug("Begin loading fonts");

  ImFontConfig cousineFontConfig;
  const float cousineFontSize = 14.0f;

  myImFormatString(
    cousineFontConfig.Name,
    IM_ARRAYSIZE(cousineFontConfig.Name),
    "%s, %.0fpx",
    "Cousine Regular",
    cousineFontSize
  );

  ImFontConfig helveticaFontConfig;
  const float helveticaFontSize = 16.0f;

  myImFormatString(
    helveticaFontConfig.Name,
    IM_ARRAYSIZE(helveticaFontConfig.Name),
    "%s, %.0fpx",
    "Helvetica Neue Light",
    helveticaFontSize
  );

  ImFontConfig spaceGroteskFontConfig;
  const float spaceGroteskFontSize = 16.0f;

  myImFormatString(
    spaceGroteskFontConfig.Name,
    IM_ARRAYSIZE(spaceGroteskFontConfig.Name),
    "%s, %.0fpx",
    "Space Grotesk Light",
    spaceGroteskFontSize
  );

  ImFontConfig sfMonoFontConfig;
  const float sfMonoFontSize = 14.0f;

  myImFormatString(
    sfMonoFontConfig.Name,
    IM_ARRAYSIZE(sfMonoFontConfig.Name),
    "%s, %.0fpx",
    "SF Mono Regular",
    sfMonoFontSize
  );

  ImFontConfig sfProFontConfig;
  const float sfProFontSize = 16.0f;

  myImFormatString(
    sfProFontConfig.Name,
    IM_ARRAYSIZE(sfProFontConfig.Name),
    "%s, %.0fpx",
    "SF Pro Regular",
    sfProFontSize
  );

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
    forkAwesomeFontSize
  );

  /// @see For details about Fork Awesome fonts: https://forkaweso.me/Fork-Awesome/icons/
  static const ImWchar forkAwesomeIconGlyphRange[] = {ICON_MIN_FK, ICON_MAX_FK, 0};

  // Clear all
  // ImGui::GetIO().Fonts->Clear();
  // m_appData.guiData().m_fonts.clear();

  // Load fonts: If no fonts are loaded, dear imgui will use the default font.
  // You can also load multiple fonts and use ImGui::PushFont()/PopFont() to select them.
  // AddFontFromFileTTF() will return the ImFont* so you can store it if you need to select the
  // font among multiple. If the file cannot be loaded, the function will return NULL.
  // Please handle those errors in your application (e.g. use an assertion, or display an error and quit).
  // The fonts will be rasterized at a given size (w/ oversampling) and stored into a texture when
  // calling ImFontAtlas::Build()/GetTexDataAsXXXX(), which ImGui_ImplXXXX_NewFrame below will call.
  /// @todo use Freetype Rasterizer and Small Font Sizes

  ImFont* cousineFontPtr
    = loadFont(cousineFontPath, cousineFontConfig, m_contentScale * cousineFontSize, nullptr);
  ImFont* fork1Ptr = loadFont(
    forkAwesomeFontPath,
    forkAwesomeFontConfig,
    m_contentScale * forkAwesomeFontSize,
    forkAwesomeIconGlyphRange
  );

  ImFont* helveticaFontPtr
    = loadFont(helveticaFontPath, helveticaFontConfig, m_contentScale * helveticaFontSize, nullptr);
  ImFont* fork2Ptr = loadFont(
    forkAwesomeFontPath,
    forkAwesomeFontConfig,
    m_contentScale * forkAwesomeFontSize,
    forkAwesomeIconGlyphRange
  );

  ImFont* spaceFontPtr = loadFont(
    spaceGroteskFontPath, spaceGroteskFontConfig, m_contentScale * spaceGroteskFontSize, nullptr
  );
  ImFont* fork3Ptr = loadFont(
    forkAwesomeFontPath,
    forkAwesomeFontConfig,
    m_contentScale * forkAwesomeFontSize,
    forkAwesomeIconGlyphRange
  );

  ImFont* sfMonoFontPtr
    = loadFont(sfMonoFontPath, sfMonoFontConfig, m_contentScale * sfMonoFontSize, nullptr);
  ImFont* fork4Ptr = loadFont(
    forkAwesomeFontPath,
    forkAwesomeFontConfig,
    m_contentScale * forkAwesomeFontSize,
    forkAwesomeIconGlyphRange
  );

  ImFont* sfProFontPtr
    = loadFont(sfProFontPath, sfProFontConfig, m_contentScale * sfProFontSize, nullptr);
  ImFont* fork5Ptr = loadFont(
    forkAwesomeFontPath,
    forkAwesomeFontConfig,
    m_contentScale * forkAwesomeFontSize,
    forkAwesomeIconGlyphRange
  );

  if (cousineFontPtr && fork1Ptr)
  {
    m_appData.guiData().m_fonts[cousineFontPath] = cousineFontPtr;
    m_appData.guiData().m_fonts[cousineFontPath + forkAwesomeFontPath] = fork1Ptr;
    spdlog::debug("Loaded font {}", cousineFontPath);
  }
  else
  {
    spdlog::error("Unable to load font {}", forkAwesomeFontPath);
  }

  if (helveticaFontPtr && fork2Ptr)
  {
    m_appData.guiData().m_fonts[helveticaFontPath] = helveticaFontPtr;
    m_appData.guiData().m_fonts[helveticaFontPath + forkAwesomeFontPath] = fork2Ptr;
    spdlog::debug("Loaded font {}", helveticaFontPath);
  }
  else
  {
    spdlog::error("Unable to load font {}", forkAwesomeFontPath);
  }

  if (spaceFontPtr && fork3Ptr)
  {
    m_appData.guiData().m_fonts[spaceGroteskFontPath] = spaceFontPtr;
    m_appData.guiData().m_fonts[spaceGroteskFontPath + forkAwesomeFontPath] = fork3Ptr;
    spdlog::debug("Loaded font {}", spaceGroteskFontPath);
  }
  else
  {
    spdlog::error("Unable to load font {}", forkAwesomeFontPath);
  }

  if (sfMonoFontPtr && fork4Ptr)
  {
    m_appData.guiData().m_fonts[sfMonoFontPath] = sfMonoFontPtr;
    m_appData.guiData().m_fonts[sfMonoFontPath + forkAwesomeFontPath] = fork4Ptr;
    spdlog::debug("Loaded font {}", sfMonoFontPath);
  }
  else
  {
    spdlog::error("Unable to load font {}", forkAwesomeFontPath);
  }

  if (sfProFontPtr && fork5Ptr)
  {
    m_appData.guiData().m_fonts[sfProFontPath] = sfProFontPtr;
    m_appData.guiData().m_fonts[sfProFontPath + forkAwesomeFontPath] = fork5Ptr;
    spdlog::debug("Loaded font {}", sfProFontPath);
  }
  else
  {
    spdlog::error("Unable to load font {}", forkAwesomeFontPath);
  }

  spdlog::debug("Done loading fonts");
}

std::pair<const char*, const char*> ImGuiWrapper::getImageDisplayAndFileNames(std::size_t imageIndex
) const
{
  static const std::string s_empty("<unknown>");

  if (const auto imageUid = m_appData.imageUid(imageIndex))
  {
    if (const Image* image = m_appData.image(*imageUid))
    {
      return {image->settings().displayName().c_str(), image->header().fileName().string().c_str()};
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
  auto getActiveImageIndex = [this]() -> std::size_t
  {
    if (const auto imageUid = m_appData.activeImageUid())
    {
      if (const auto index = m_appData.imageIndex(*imageUid))
      {
        return *index;
      }
    }

    spdlog::warn("No valid active image");
    return 0;
  };

  auto setActiveImageIndex = [this](std::size_t index)
  {
    if (const auto imageUid = m_appData.imageUid(index))
    {
      if (!m_appData.setActiveImageUid(*imageUid))
      {
        spdlog::warn("Cannot set active image to {}", *imageUid);
      }
    }
    else
    {
      spdlog::warn("Cannot set active image to invalid index {}", index);
    }
  };

  auto getImageHasActiveSeg = [this](std::size_t index) -> bool
  {
    if (const auto imageUid = m_appData.imageUid(index))
    {
      return m_appData.isImageBeingSegmented(*imageUid);
    }
    else
    {
      spdlog::warn("Cannot get whether seg is active for invalid image index {}", index);
      return false;
    }
  };

  auto setImageHasActiveSeg = [this](std::size_t index, bool set)
  {
    if (const auto imageUid = m_appData.imageUid(index))
    {
      m_appData.setImageBeingSegmented(*imageUid, set);
    }
    else
    {
      spdlog::warn("Cannot set whether seg is active for invalid image index {}", index);
    }
  };

  /// @todo remove this
  auto getMouseMode = [this]() { return m_appData.state().mouseMode(); };

  auto setMouseMode = [this](MouseMode mouseMode) { m_appData.state().setMouseMode(mouseMode); };

  auto cycleViewLayout = [this](int step) { m_appData.windowData().cycleCurrentLayout(step); };

  /// @todo remove this
  auto getNumImageColorMaps = [this]() -> std::size_t { return m_appData.numImageColorMaps(); };

  auto getImageColorMap = [this](std::size_t cmapIndex) -> ImageColorMap*
  {
    if (const auto cmapUid = m_appData.imageColorMapUid(cmapIndex))
    {
      return m_appData.imageColorMap(*cmapUid);
    }
    return nullptr;
  };

  auto getLabelTable = [this](std::size_t tableIndex) -> ParcellationLabelTable*
  {
    if (const auto tableUid = m_appData.labelTableUid(tableIndex))
    {
      return m_appData.labelTable(*tableUid);
    }
    return nullptr;
  };

  auto getImageIsVisibleSetting = [this](std::size_t imageIndex) -> bool
  {
    if (const auto imageUid = m_appData.imageUid(imageIndex))
    {
      if (const Image* image = m_appData.image(*imageUid))
      {
        return image->settings().visibility();
      }
    }
    return false;
  };

  auto getImageIsActive = [this](std::size_t imageIndex) -> bool
  {
    if (const auto imageUid = m_appData.imageUid(imageIndex))
    {
      const auto activeImageUid = m_appData.activeImageUid();

      return (*imageUid == *activeImageUid);
    }
    return false;
  };

  auto moveImageBackward = [this](const uuids::uuid& imageUid) -> bool
  {
    if (m_appData.moveImageBackwards(imageUid))
    {
      m_appData.windowData().updateImageOrdering(m_appData.imageUidsOrdered());
      return true;
    }
    return false;
  };

  auto moveImageForward = [this](const uuids::uuid& imageUid) -> bool
  {
    if (m_appData.moveImageForwards(imageUid))
    {
      m_appData.windowData().updateImageOrdering(m_appData.imageUidsOrdered());
      return true;
    }
    return false;
  };

  auto moveImageToBack = [this](const uuids::uuid& imageUid) -> bool
  {
    if (m_appData.moveImageToBack(imageUid))
    {
      m_appData.windowData().updateImageOrdering(m_appData.imageUidsOrdered());
      return true;
    }
    return false;
  };

  auto moveImageToFront = [this](const uuids::uuid& imageUid) -> bool
  {
    if (m_appData.moveImageToFront(imageUid))
    {
      m_appData.windowData().updateImageOrdering(m_appData.imageUidsOrdered());
      return true;
    }
    return false;
  };

  auto applyImageSelectionAndRenderModesToAllViews = [this](const uuids::uuid& viewUid)
  {
    m_appData.windowData().applyImageSelectionToAllCurrentViews(viewUid);
    m_appData.windowData().applyViewRenderModeAndProjectionToAllCurrentViews(viewUid);
  };

  auto getViewCameraRotation = [this](const uuids::uuid& viewUid) -> glm::quat
  {
    const View* view = m_appData.windowData().getCurrentView(viewUid);
    if (!view)
      return sk_identityRotation;

    return camera::computeCameraRotationRelativeToWorld(view->camera());
  };

  auto setViewCameraRotation =
    [this](const uuids::uuid& viewUid, const glm::quat& camera_T_world_rotationDelta)
  { m_callbackHandler.doCameraRotate3d(viewUid, camera_T_world_rotationDelta); };

  auto setViewCameraDirection =
    [this](const uuids::uuid& viewUid, const glm::vec3& worldFwdDirection)
  { m_callbackHandler.handleSetViewForwardDirection(viewUid, worldFwdDirection); };

  auto getViewNormal = [this](const uuids::uuid& viewUid)
  {
    View* view = m_appData.windowData().getCurrentView(viewUid);
    if (!view)
      return sk_zeroVec;
    return camera::worldDirection(view->camera(), Directions::View::Back);
  };

  auto getObliqueViewDirections = [this](const uuids::uuid& viewUidToExclude
                                  ) -> std::vector<glm::vec3>
  {
    std::vector<glm::vec3> obliqueViewDirections;

    for (std::size_t i = 0; i < m_appData.windowData().numLayouts(); ++i)
    {
      const Layout* layout = m_appData.windowData().layout(i);
      if (!layout)
        continue;

      for (const auto& view : layout->views())
      {
        if (view.first == viewUidToExclude)
          continue;
        if (!view.second)
          continue;

        if (!camera::looksAlongOrthogonalAxis(view.second->camera()))
        {
          obliqueViewDirections.emplace_back(
            camera::worldDirection(view.second->camera(), Directions::View::Front)
          );
        }
      }
    }

    return obliqueViewDirections;
  };

  ImGui::NewFrame();

  if (m_appData.guiData().m_renderUiWindows)
  {
    renderConfirmCloseAppPopup(m_appData);

    if (m_appData.guiData().m_showImGuiDemoWindow)
    {
      ImGui::ShowDemoWindow(&m_appData.guiData().m_showImGuiDemoWindow);
    }

    if (m_appData.guiData().m_showImPlotDemoWindow)
    {
      ImPlot::ShowDemoWindow(&m_appData.guiData().m_showImPlotDemoWindow);
    }

    renderMainMenuBar(m_appData.guiData());

    if (m_appData.guiData().m_showIsosurfacesWindow)
    {
      renderIsosurfacesWindow(
        m_appData,
        std::bind(&ImGuiWrapper::storeFuture, this, _1, _2),
        std::bind(&ImGuiWrapper::addTaskToIsosurfaceGpuMeshGenerationQueue, this, _1)
      );
    }

    if (m_appData.guiData().m_showSettingsWindow)
    {
      renderSettingsWindow(
        m_appData, getNumImageColorMaps, getImageColorMap, m_updateMetricUniforms, m_recenterAllViews
      );
    }

    using namespace std::placeholders;

    if (m_appData.guiData().m_showInspectionWindow)
    {
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
        getLabelTable
      );
    }

    if (m_appData.guiData().m_showImagePropertiesWindow)
    {
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
        m_recenterAllViews
      );
    }

    if (m_appData.guiData().m_showSegmentationsWindow)
    {
      renderSegmentationPropertiesWindow(
        m_appData,
        getLabelTable,
        m_updateImageUniforms,
        m_updateLabelColorTableTexture,
        m_moveCrosshairsToSegLabelCentroid,
        m_createBlankSeg,
        m_clearSeg,
        m_removeSeg,
        m_recenterAllViews
      );
    }

    if (m_appData.guiData().m_showLandmarksWindow)
    {
      renderLandmarkPropertiesWindow(m_appData, m_recenterAllViews);
    }

    if (m_appData.guiData().m_showAnnotationsWindow)
    {
      renderAnnotationWindow(
        m_appData,
        setViewCameraDirection,
        m_paintActiveSegmentationWithActivePolygon,
        m_recenterAllViews
      );
    }

    if (m_appData.guiData().m_showOpacityBlenderWindow)
    {
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
      setActiveImageIndex
    );

    renderSegToolbar(
      m_appData,
      m_appData.numImages(),
      std::bind(&ImGuiWrapper::getImageDisplayAndFileNames, this, _1),
      getActiveImageIndex,
      setActiveImageIndex,
      getImageHasActiveSeg,
      setImageHasActiveSeg,
      m_readjustViewport,
      m_updateImageUniforms,
      m_executeGraphCutsSeg,
      m_executePoissonSeg
    );

    annotationToolbar(m_paintActiveSegmentationWithActivePolygon);
  }

  Layout& currentLayout = m_appData.windowData().currentLayout();

  const float wholeWindowHeight = static_cast<float>(m_appData.windowData().getWindowSize().y);

  if (m_appData.guiData().m_renderUiOverlays && currentLayout.isLightbox())
  {
    // Per-layout UI controls:

    static constexpr bool sk_recenterCrosshairs = false;
    static constexpr bool sk_doNotRecenterOnCurrentCrosshairsPosition = false;
    static constexpr bool sk_resetObliqueOrientation = false;
    static constexpr bool sk_resetZoom = true;

    const auto mindowFrameBounds = camera::computeMindowFrameBounds(
      currentLayout.windowClipViewport(),
      m_appData.windowData().viewport().getAsVec4(),
      wholeWindowHeight
    );

    renderViewSettingsComboWindow(
      currentLayout.uid(),
      mindowFrameBounds,
      currentLayout.uiControls(),
      true,
      false,

      m_appData.windowData().getContentScaleRatios(),

      m_appData.numImages(),
      [this, &currentLayout](std::size_t index)
      { return currentLayout.isImageRendered(m_appData, index); },
      [this, &currentLayout](std::size_t index, bool visible)
      { currentLayout.setImageRendered(m_appData, index, visible); },

      [this, &currentLayout](std::size_t index)
      { return currentLayout.isImageUsedForMetric(m_appData, index); },
      [this, &currentLayout](std::size_t index, bool visible)
      { currentLayout.setImageUsedForMetric(m_appData, index, visible); },
      std::bind(&ImGuiWrapper::getImageDisplayAndFileNames, this, _1),

      getImageIsVisibleSetting,
      getImageIsActive,

      currentLayout.viewType(),
      currentLayout.renderMode(),
      currentLayout.intensityProjectionMode(),

      [&currentLayout](const ViewType& viewType) { return currentLayout.setViewType(viewType); },
      [&currentLayout](const camera::ViewRenderMode& renderMode)
      { return currentLayout.setRenderMode(renderMode); },
      [&currentLayout](const camera::IntensityProjectionMode& ipMode)
      { return currentLayout.setIntensityProjectionMode(ipMode); },

      [this]()
      {
        m_recenterAllViews(
          sk_recenterCrosshairs,
          sk_doNotRecenterOnCurrentCrosshairsPosition,
          sk_resetObliqueOrientation,
          sk_resetZoom
        );
      },
      nullptr,

      [this]() { return m_appData.renderData().m_intensityProjectionSlabThickness; },
      [this](float thickness)
      { m_appData.renderData().m_intensityProjectionSlabThickness = thickness; },
      [this]() { return m_appData.renderData().m_doMaxExtentIntensityProjection; },
      [this](bool set) { m_appData.renderData().m_doMaxExtentIntensityProjection = set; },

      [this]() { return m_appData.renderData().m_xrayIntensityWindow; },
      [this](float window) { m_appData.renderData().m_xrayIntensityWindow = window; },
      [this]() { return m_appData.renderData().m_xrayIntensityLevel; },
      [this](float level) { m_appData.renderData().m_xrayIntensityLevel = level; },
      [this]() { return m_appData.renderData().m_xrayEnergyKeV; },
      [this](float energy) { m_appData.renderData().setXrayEnergy(energy); }
    );

    renderViewOrientationToolWindow(
      currentLayout.uid(),
      mindowFrameBounds,
      currentLayout.uiControls(),
      true,
      currentLayout.viewType(),
      [&getViewCameraRotation, &currentLayout]()
      { return getViewCameraRotation(currentLayout.uid()); },
      [&setViewCameraRotation, &currentLayout](const glm::quat& q)
      { return setViewCameraRotation(currentLayout.uid(), q); },
      [&setViewCameraDirection, &currentLayout](const glm::vec3& dir)
      { return setViewCameraDirection(currentLayout.uid(), dir); },
      [&getViewNormal, &currentLayout]() { return getViewNormal(currentLayout.uid()); },
      getObliqueViewDirections
    );
  }
  else if (m_appData.guiData().m_renderUiOverlays && !currentLayout.isLightbox())
  {
    // Per-view UI controls:

    for (const auto& viewUid : m_appData.windowData().currentViewUids())
    {
      View* view = m_appData.windowData().getCurrentView(viewUid);
      if (!view)
        return;

      auto setViewType = [view](const ViewType& viewType)
      {
        if (view)
          view->setViewType(viewType);
      };

      auto setRenderMode = [view](const camera::ViewRenderMode& renderMode)
      {
        if (view)
          view->setRenderMode(renderMode);
      };

      auto setIntensityProjectionMode = [view](const camera::IntensityProjectionMode& ipMode)
      {
        if (view)
          view->setIntensityProjectionMode(ipMode);
      };

      auto recenter = [this, &viewUid]() { m_recenterView(viewUid); };

      const auto mindowFrameBounds = camera::computeMindowFrameBounds(
        view->windowClipViewport(), m_appData.windowData().viewport().getAsVec4(), wholeWindowHeight
      );

      renderViewSettingsComboWindow(
        viewUid,
        mindowFrameBounds,
        view->uiControls(),
        false,
        true,

        m_appData.windowData().getContentScaleRatios(),

        m_appData.numImages(),
        [this, view](std::size_t index) { return view->isImageRendered(m_appData, index); },
        [this, view](std::size_t index, bool visible)
        { view->setImageRendered(m_appData, index, visible); },

        [this, view](std::size_t index) { return view->isImageUsedForMetric(m_appData, index); },
        [this, view](std::size_t index, bool visible)
        { view->setImageUsedForMetric(m_appData, index, visible); },

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
        [this](float thickness)
        { m_appData.renderData().m_intensityProjectionSlabThickness = thickness; },
        [this]() { return m_appData.renderData().m_doMaxExtentIntensityProjection; },
        [this](bool set) { m_appData.renderData().m_doMaxExtentIntensityProjection = set; },

        [this]() { return m_appData.renderData().m_xrayIntensityWindow; },
        [this](float window) { m_appData.renderData().m_xrayIntensityWindow = window; },
        [this]() { return m_appData.renderData().m_xrayIntensityLevel; },
        [this](float level) { m_appData.renderData().m_xrayIntensityLevel = level; },
        [this]() { return m_appData.renderData().m_xrayEnergyKeV; },
        [this](float energy) { m_appData.renderData().setXrayEnergy(energy); }
      );

      renderViewOrientationToolWindow(
        viewUid,
        mindowFrameBounds,
        view->uiControls(),
        false,
        view->viewType(),
        [&getViewCameraRotation, &viewUid]() { return getViewCameraRotation(viewUid); },
        [&setViewCameraRotation, &viewUid](const glm::quat& q)
        { return setViewCameraRotation(viewUid, q); },
        [&setViewCameraDirection, &viewUid](const glm::vec3& dir)
        { return setViewCameraDirection(viewUid, dir); },
        [&getViewNormal, &viewUid]() { return getViewNormal(viewUid); },
        getObliqueViewDirections
      );
    }
  }

  ImGui::Render();
  ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

void ImGuiWrapper::annotationToolbar(const std::function<void()> paintActiveAnnotation)
{
  if (!state::isInStateWhereToolbarVisible())
  {
    return;
  }

  if (!ASM::current_state_ptr || !ASM::current_state_ptr->selectedViewUid())
  {
    return;
  }

  // Position the annotation toolbar at the bottom of this view:
  const View* annotationView = m_appData.windowData().getView(
    *ASM::current_state_ptr->selectedViewUid()
  );

  const float wholeWindowHeight = static_cast<float>(m_appData.windowData().getWindowSize().y);

  const auto mindowAnnotViewFrameBounds = camera::computeMindowFrameBounds(
    annotationView->windowClipViewport(),
    m_appData.windowData().viewport().getAsVec4(),
    wholeWindowHeight
  );

  renderAnnotationToolbar(m_appData, mindowAnnotViewFrameBounds, paintActiveAnnotation);
}
