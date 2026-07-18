#include "ui/NativeFileDialogs.h"

#include <spdlog/fmt/std.h>

#include <nfd.hpp>

#include <spdlog/spdlog.h>

namespace fs = std::filesystem;

namespace
{
class NfdInitializer
{
public:
  NfdInitializer() : m_initialized(NFD::Init() == NFD_OKAY)
  {
    if (!m_initialized) {
      spdlog::error("Failed to initialize native file dialogs: {}", NFD::GetError());
      NFD::ClearError();
    }
  }

  ~NfdInitializer()
  {
    if (m_initialized) {
      NFD::Quit();
    }
  }

  [[nodiscard]] bool initialized() const
  {
    return m_initialized;
  }

private:
  bool m_initialized = false;
};

bool ensureNfdInitialized()
{
  static const NfdInitializer initializer;
  return initializer.initialized();
}

fs::path& lastUsedDirectory()
{
  static fs::path directory;
  return directory;
}

std::string dialogDefaultPathString(const fs::path& defaultPath)
{
  const fs::path& dialogPath = defaultPath.empty() ? lastUsedDirectory() : defaultPath;
  return dialogPath.empty() ? std::string{} : dialogPath.string();
}

std::vector<nfdfilteritem_t> toNfdFilters(const std::vector<native_dialog::Filter>& filters)
{
  std::vector<nfdfilteritem_t> nfdFilters;
  nfdFilters.reserve(filters.size());

  for (const auto& filter : filters) {
    nfdFilters.push_back({filter.name.c_str(), filter.extensions.c_str()});
  }

  return nfdFilters;
}

std::optional<fs::path> handleDialogResult(NFD::UniquePath& outPath, nfdresult_t result)
{
  if (NFD_OKAY == result) {
    const fs::path selectedPath{outPath.get()};
    const fs::path selectedDirectory = selectedPath.parent_path();
    if (!selectedDirectory.empty()) {
      lastUsedDirectory() = selectedDirectory;
    }
    return selectedPath;
  }

  if (NFD_CANCEL == result) {
    return std::nullopt;
  }

  spdlog::error("Native file dialog error: {}", NFD::GetError());
  NFD::ClearError();
  return std::nullopt;
}

native_dialog::PathDialogResult handleMultiDialogResultWithStatus(NFD::UniquePathSet& outPaths, nfdresult_t result)
{
  native_dialog::PathDialogResult dialogResult;

  std::vector<fs::path> selectedPaths;

  if (NFD_CANCEL == result) {
    dialogResult.status = native_dialog::PathDialogStatus::Canceled;
    return dialogResult;
  }

  if (NFD_OKAY != result) {
    spdlog::error("Native file dialog error: {}", NFD::GetError());
    NFD::ClearError();
    dialogResult.status = native_dialog::PathDialogStatus::Error;
    return dialogResult;
  }

  nfdpathsetsize_t count = 0;
  if (NFD_OKAY != NFD::PathSet::Count(outPaths.get(), count)) {
    spdlog::error("Native file dialog error: {}", NFD::GetError());
    NFD::ClearError();
    dialogResult.status = native_dialog::PathDialogStatus::Error;
    return dialogResult;
  }

  selectedPaths.reserve(count);
  for (nfdpathsetsize_t i = 0; i < count; ++i) {
    nfdchar_t* rawPath = nullptr;
    if (NFD_OKAY != NFD::PathSet::GetPath(outPaths.get(), i, rawPath) || !rawPath) {
      spdlog::error("Native file dialog error: {}", NFD::GetError());
      NFD::ClearError();
      dialogResult.status = native_dialog::PathDialogStatus::Error;
      return dialogResult;
    }

    NFD::UniquePathSetPath path(rawPath);
    selectedPaths.emplace_back(path.get());
  }

  if (!selectedPaths.empty()) {
    const fs::path selectedDirectory = selectedPaths.front().parent_path();
    if (!selectedDirectory.empty()) {
      lastUsedDirectory() = selectedDirectory;
    }
  }

  dialogResult.status =
    selectedPaths.empty() ? native_dialog::PathDialogStatus::Canceled : native_dialog::PathDialogStatus::Selected;
  dialogResult.paths = std::move(selectedPaths);
  return dialogResult;
}

std::vector<fs::path> handleMultiDialogResult(NFD::UniquePathSet& outPaths, nfdresult_t result)
{
  return handleMultiDialogResultWithStatus(outPaths, result).paths;
}
} // namespace

namespace native_dialog
{
std::optional<fs::path> openFile(const std::vector<Filter>& filters, const fs::path& defaultPath)
{
  if (!ensureNfdInitialized()) {
    return std::nullopt;
  }

  const auto nfdFilters = toNfdFilters(filters);
  const std::string defaultPathString = dialogDefaultPathString(defaultPath);
  NFD::UniquePath outPath;

  const nfdresult_t result = NFD::OpenDialog(
    outPath,
    nfdFilters.empty() ? nullptr : nfdFilters.data(),
    static_cast<nfdfiltersize_t>(nfdFilters.size()),
    defaultPathString.empty() ? nullptr : defaultPathString.c_str());

  return handleDialogResult(outPath, result);
}

std::vector<fs::path> openFiles(const std::vector<Filter>& filters, const fs::path& defaultPath)
{
  if (!ensureNfdInitialized()) {
    return {};
  }

  const auto nfdFilters = toNfdFilters(filters);
  const std::string defaultPathString = dialogDefaultPathString(defaultPath);
  NFD::UniquePathSet outPaths;

  const nfdresult_t result = NFD::OpenDialogMultiple(
    outPaths,
    nfdFilters.empty() ? nullptr : nfdFilters.data(),
    static_cast<nfdfiltersize_t>(nfdFilters.size()),
    defaultPathString.empty() ? nullptr : defaultPathString.c_str());

  return handleMultiDialogResult(outPaths, result);
}

std::optional<fs::path> pickFolder(const fs::path& defaultPath)
{
  if (!ensureNfdInitialized()) {
    return std::nullopt;
  }

  const std::string defaultPathString = dialogDefaultPathString(defaultPath);
  NFD::UniquePath outPath;

  const nfdresult_t result = NFD::PickFolder(outPath, defaultPathString.empty() ? nullptr : defaultPathString.c_str());

  return handleDialogResult(outPath, result);
}

std::vector<fs::path> pickFolders(const fs::path& defaultPath)
{
  return pickFoldersWithStatus(defaultPath).paths;
}

PathDialogResult pickFoldersWithStatus(const fs::path& defaultPath)
{
  if (!ensureNfdInitialized()) {
    return {.status = PathDialogStatus::Unavailable, .paths = {}};
  }

  const std::string defaultPathString = dialogDefaultPathString(defaultPath);
  NFD::UniquePathSet outPaths;

  const nfdresult_t result =
    NFD::PickFolderMultiple(outPaths, defaultPathString.empty() ? nullptr : defaultPathString.c_str());

  return handleMultiDialogResultWithStatus(outPaths, result);
}

std::optional<fs::path>
saveFile(const std::vector<Filter>& filters, const fs::path& defaultPath, const std::string& defaultName)
{
  if (!ensureNfdInitialized()) {
    return std::nullopt;
  }

  const auto nfdFilters = toNfdFilters(filters);
  const std::string defaultPathString = dialogDefaultPathString(defaultPath);
  NFD::UniquePath outPath;

  const nfdresult_t result = NFD::SaveDialog(
    outPath,
    nfdFilters.empty() ? nullptr : nfdFilters.data(),
    static_cast<nfdfiltersize_t>(nfdFilters.size()),
    defaultPathString.empty() ? nullptr : defaultPathString.c_str(),
    defaultName.empty() ? nullptr : defaultName.c_str());

  return handleDialogResult(outPath, result);
}

std::vector<Filter> imageFilters()
{
  return {
    {"Images", "nii,nii.gz,gz,nrrd,nhdr,mha,mhd,dcm,img,hdr,jpg,jpeg,jpe,png,tif,tiff,bmp,dib"},
    {"Medical images", "nii,nii.gz,gz,nrrd,nhdr,mha,mhd,dcm,img,hdr"},
    {"Standard 2D images", "jpg,jpeg,jpe,png,tif,tiff,bmp,dib"}};
}

std::vector<Filter> medicalImageExportFilters()
{
  return {{"Medical images", "nii,nii.gz,nrrd,nhdr,mha,mhd,img,hdr"}};
}

std::vector<Filter> projectFilters()
{
  return {{"Entropy projects", "json"}};
}

std::vector<Filter> layoutFilters()
{
  return {{"Entropy layouts", "json"}};
}

std::vector<Filter> transformFilters()
{
  return {{"Transform files", "txt,mat,tfm"}};
}

std::vector<Filter> segmentationFilters()
{
  return imageFilters();
}

std::vector<Filter> landmarkFilters()
{
  return {{"Landmark CSV", "csv"}};
}

std::vector<Filter> annotationFilters()
{
  return {{"Annotation JSON", "json"}};
}
} // namespace native_dialog
