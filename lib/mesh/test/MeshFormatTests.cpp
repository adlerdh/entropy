#include "mesh/MeshFormat.h"

#include <catch2/catch_test_macros.hpp>

using namespace mesh;

TEST_CASE("Mesh formats are inferred case-insensitively from supported paths")
{
  CHECK(formatFromPath("surface.vtp") == MeshFormat::Vtp);
  CHECK(formatFromPath("surface.VTK") == MeshFormat::LegacyVtk);
  CHECK(formatFromPath("surface.stl") == MeshFormat::Stl);
  CHECK(formatFromPath("surface.ply") == MeshFormat::Ply);
  CHECK(formatFromPath("surface.obj") == MeshFormat::Obj);
  CHECK(formatFromPath("surface.off") == MeshFormat::Off);
  CHECK(formatFromPath("lh.surf.gii") == MeshFormat::Gifti);
  CHECK(formatFromPath("lh.pial") == MeshFormat::FreeSurferBinary);
  CHECK(formatFromPath("rh.white") == MeshFormat::FreeSurferBinary);
  CHECK(formatFromPath("surface.fsb") == MeshFormat::FreeSurferBinary);
  CHECK(formatFromPath("surface.fcv") == MeshFormat::FreeSurferBinary);
  CHECK(formatFromPath("surface.fsa") == MeshFormat::FreeSurferAscii);
  CHECK_FALSE(formatFromPath("surface.txt"));
}
