#include "mesh/MeshFormat.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <string>

namespace mesh
{
namespace
{
std::string lower(std::string value)
{
  std::ranges::transform(value, value.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  return value;
}
} // namespace

std::string_view formatName(MeshFormat format) noexcept
{
  switch (format) {
    case MeshFormat::Vtp:
      return "VTK XML PolyData";
    case MeshFormat::LegacyVtk:
      return "Legacy VTK PolyData";
    case MeshFormat::Stl:
      return "STL";
    case MeshFormat::Ply:
      return "PLY";
    case MeshFormat::Obj:
      return "Wavefront OBJ";
    case MeshFormat::Off:
      return "Object File Format";
    case MeshFormat::Gifti:
      return "GIFTI surface";
    case MeshFormat::FreeSurferBinary:
      return "FreeSurfer binary surface";
    case MeshFormat::FreeSurferAscii:
      return "FreeSurfer ASCII surface";
  }
  return "Unknown mesh";
}

std::vector<std::string_view> formatExtensions(MeshFormat format)
{
  switch (format) {
    case MeshFormat::Vtp:
      return {".vtp"};
    case MeshFormat::LegacyVtk:
      return {".vtk"};
    case MeshFormat::Stl:
      return {".stl"};
    case MeshFormat::Ply:
      return {".ply"};
    case MeshFormat::Obj:
      return {".obj"};
    case MeshFormat::Off:
      return {".off"};
    case MeshFormat::Gifti:
      return {".gii", ".surf.gii"};
    case MeshFormat::FreeSurferBinary:
      return {".fsb", ".fcv", ".pial", ".white", ".inflated", ".sphere", ".orig", ".smoothwm"};
    case MeshFormat::FreeSurferAscii:
      return {".fsa", ".asc"};
  }
  return {};
}

bool isRecognizedFreeSurferSurfacePath(const std::filesystem::path& path)
{
  static constexpr std::array<std::string_view, 7>
    sk_suffixes{".pial", ".white", ".inflated", ".sphere", ".orig", ".smoothwm", ".surf"};
  const std::string name = lower(path.filename().string());
  return std::ranges::any_of(sk_suffixes, [&name](std::string_view suffix) { return name.ends_with(suffix); });
}

std::optional<MeshFormat> formatFromPath(const std::filesystem::path& path)
{
  const std::string name = lower(path.filename().string());
  if (name.ends_with(".surf.gii") || name.ends_with(".gii")) return MeshFormat::Gifti;
  if (name.ends_with(".vtp")) return MeshFormat::Vtp;
  if (name.ends_with(".vtk")) return MeshFormat::LegacyVtk;
  if (name.ends_with(".stl")) return MeshFormat::Stl;
  if (name.ends_with(".ply")) return MeshFormat::Ply;
  if (name.ends_with(".obj")) return MeshFormat::Obj;
  if (name.ends_with(".off")) return MeshFormat::Off;
  if (name.ends_with(".fsa") || name.ends_with(".asc")) return MeshFormat::FreeSurferAscii;
  if (name.ends_with(".fsb") || name.ends_with(".fcv") || isRecognizedFreeSurferSurfacePath(path)) {
    return MeshFormat::FreeSurferBinary;
  }
  return std::nullopt;
}

} // namespace mesh
