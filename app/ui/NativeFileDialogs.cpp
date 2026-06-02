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

  bool initialized() const
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
  return {{"Medical images", "nii,nii.gz,gz,nrrd,nhdr,mha,mhd,dcm,img,hdr"}};
}

std::vector<Filter> projectFilters()
{
  return {{"Entropy projects", "json"}};
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
