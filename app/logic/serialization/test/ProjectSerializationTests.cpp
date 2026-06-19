#include "logic/serialization/ProjectSerialization.h"

#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <fstream>
#include <string>

namespace fs = std::filesystem;

using json = nlohmann::json;

void to_json(json& j, const Annotation&)
{
  j = json::object();
}

void from_json(const json&, std::vector<Annotation>& annotations)
{
  annotations.clear();
}

namespace
{
fs::path uniqueTempProjectDirectory()
{
  const fs::path base = fs::temp_directory_path() / "entropy-project-serialization-tests";
  fs::create_directories(base);

  for (int i = 0; i < 1000; ++i) {
    fs::path candidate = base / (std::string{"case-"} + std::to_string(i));
    std::error_code ec;
    if (fs::create_directories(candidate, ec)) {
      return candidate;
    }
  }

  throw std::runtime_error("Unable to create temporary project serialization test directory");
}

void touchFile(const fs::path& path)
{
  fs::create_directories(path.parent_path());
  std::ofstream out(path);
  out << "test";
}
} // namespace

TEST_CASE("Project serialization preserves DICOM source metadata", "[project][dicom][serialization]")
{
  const fs::path root = uniqueTempProjectDirectory();
  const fs::path dicomRoot = root / "dicom";
  const fs::path slice1 = dicomRoot / "slice-001.dcm";
  const fs::path slice2 = dicomRoot / "slice-002.dcm";
  const fs::path projectFile = root / "project.json";

  touchFile(slice1);
  touchFile(slice2);

  serialize::EntropyProject project;
  project.m_referenceImage.m_imageFileName = slice1;
  project.m_referenceImage.m_dicomSource = serialize::DicomSource{
    .m_rootPath = dicomRoot,
    .m_studyInstanceUid = "1.2.3.study",
    .m_seriesInstanceUid = "1.2.3.series",
    .m_files = {slice1, slice2}};

  REQUIRE(serialize::save(project, projectFile));

  serialize::EntropyProject loaded;
  REQUIRE(serialize::open(loaded, projectFile));
  REQUIRE(loaded.m_referenceImage.m_dicomSource.has_value());

  const auto& source = *loaded.m_referenceImage.m_dicomSource;
  CHECK(loaded.m_referenceImage.m_imageFileName == fs::canonical(slice1));
  CHECK(source.m_rootPath == fs::canonical(dicomRoot));
  CHECK(source.m_studyInstanceUid == "1.2.3.study");
  CHECK(source.m_seriesInstanceUid == "1.2.3.series");
  REQUIRE(source.m_files.size() == 2);
  CHECK(source.m_files.at(0) == fs::canonical(slice1));
  CHECK(source.m_files.at(1) == fs::canonical(slice2));
}

TEST_CASE("Project serialization preserves interface settings", "[project][serialization]")
{
  serialize::EntropyProject project;
  project.m_referenceImage.m_imageFileName = "image.nii.gz";
  project.m_interface.m_showLayoutTabs = false;
  project.m_interface.m_layoutTabPlacement = serialize::ProjectLayoutTabPlacement::Bottom;
  project.m_interface.m_imageValuePrecision = 4;
  project.m_interface.m_coordsPrecision = 5;
  project.m_interface.m_txPrecision = 6;
  project.m_interface.m_percentilePrecision = 7;

  const json root = project;

  REQUIRE(root.contains("interface"));
  CHECK(root.at("interface").at("showLayoutTabs") == false);
  CHECK(root.at("interface").at("layoutTabsPosition") == "bottom");
  CHECK(root.at("interface").at("imageValuePrecision") == 4);
  CHECK(root.at("interface").at("coordinatesPrecision") == 5);
  CHECK(root.at("interface").at("transformPrecision") == 6);
  CHECK(root.at("interface").at("percentilePrecision") == 7);

  const serialize::EntropyProject parsed = root.get<serialize::EntropyProject>();

  CHECK(parsed.m_interface.m_showLayoutTabs == false);
  CHECK(parsed.m_interface.m_layoutTabPlacement == serialize::ProjectLayoutTabPlacement::Bottom);
  CHECK(parsed.m_interface.m_imageValuePrecision == 4);
  CHECK(parsed.m_interface.m_coordsPrecision == 5);
  CHECK(parsed.m_interface.m_txPrecision == 6);
  CHECK(parsed.m_interface.m_percentilePrecision == 7);
}
