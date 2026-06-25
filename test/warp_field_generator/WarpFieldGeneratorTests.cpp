#include "WarpFieldGenerator.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <filesystem>

namespace
{
warp_field::WarpFieldSpec baseSpec()
{
  warp_field::WarpFieldSpec spec;
  spec.output = std::filesystem::temp_directory_path() / "entropy-warp-field-test.nii.gz";
  spec.spatialDimension = 3;
  spec.vectorDimension = 3;
  spec.size = {5, 5, 5};
  spec.spacing = {1.0, 1.0, 1.0};
  spec.origin = {-2.0, -2.0, -2.0};
  spec.direction = {1.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 1.0};
  return spec;
}
} // namespace

TEST_CASE("Warp field JSON parser builds time-dependent specifications", "[warp-field]")
{
  const std::string json = R"json({
    "output": "field.nii.gz",
    "component_type": "double",
    "spatial_dimension": 3,
    "size": [4, 5, 6],
    "time_points": 7,
    "spacing": [0.5, 0.6, 0.7],
    "operations": [
      {
        "type": "wave",
        "normal": [1, 0, 0],
        "direction": [0, 1, 0],
        "amplitude": 2,
        "time": {"mode": "sine", "frequency": 2}
      }
    ]
  })json";

  const warp_field::WarpFieldSpec spec = warp_field::parseSpecJson(json);

  CHECK(spec.output == "field.nii.gz");
  CHECK(spec.componentType == "double");
  CHECK(spec.vectorDimension == 3);
  CHECK(spec.size == std::vector<std::size_t>{4, 5, 6, 7});
  CHECK(spec.spacing == std::vector<double>{0.5, 0.6, 0.7, 1.0});
  CHECK(spec.origin == std::vector<double>{0.0, 0.0, 0.0, 0.0});
  CHECK(spec.direction.size() == 16);
  CHECK(spec.operations.size() == 1);
}

TEST_CASE("Warp field JSON parser sizes defaults from spatial dimension", "[warp-field]")
{
  const std::string json = R"json({
    "output": "field.nii.gz",
    "spatial_dimension": 2,
    "operations": [
      {"type": "affine", "translation": [1, 2]}
    ]
  })json";

  const warp_field::WarpFieldSpec spec = warp_field::parseSpecJson(json);

  CHECK(spec.vectorDimension == 2);
  CHECK(spec.size == std::vector<std::size_t>{64, 64});
  CHECK(spec.spacing == std::vector<double>{1.0, 1.0});
  CHECK(spec.origin == std::vector<double>{0.0, 0.0});
  CHECK(spec.direction == std::vector<double>{1.0, 0.0, 0.0, 1.0});
}

TEST_CASE("Warp field JSON parser reads composition mode", "[warp-field]")
{
  const std::string json = R"json({
    "output": "field.nii.gz",
    "composition_mode": "composed",
    "operations": [
      {"type": "affine", "translation": [1, 0, 0]}
    ]
  })json";

  const warp_field::WarpFieldSpec spec = warp_field::parseSpecJson(json);

  CHECK(spec.compositionMode == warp_field::CompositionMode::Composed);
}

TEST_CASE("Warp field parser accepts all operation families", "[warp-field]")
{
  const std::vector<std::string> operationSpecs = {
    R"json({"type": "affine"})json",
    R"json({"type": "inverse_affine"})json",
    R"json({"type": "expansion"})json",
    R"json({"type": "contraction"})json",
    R"json({"type": "rotation"})json",
    R"json({"type": "twist"})json",
    R"json({"type": "swirl"})json",
    R"json({"type": "vortex_pair"})json",
    R"json({"type": "source_sink_pair"})json",
    R"json({"type": "wave"})json",
    R"json({"type": "shear"})json",
    R"json({"type": "stretch"})json",
    R"json({"type": "fold"})json",
    R"json({"type": "tear"})json",
    R"json({"type": "smooth_random"})json",
    R"json({"type": "piecewise_affine"})json",
    R"json({"type": "sliding_interface"})json",
    R"json({"type": "divergence_free"})json",
    R"json({"type": "curl_free"})json",
    R"json({"type": "jacobian_expansion"})json",
    R"json({"type": "boundary_constrained"})json",
    R"json({"type": "jump"})json",
    R"json({"type": "white_noise"})json",
    R"json({"type": "control_point", "control_points": [{"point": [0, 0, 0], "displacement": [1, 0, 0]}]})json",
    R"json({"type": "landmark", "landmarks": [{"fixed": [0, 0, 0], "moving": [1, 0, 0]}]})json"};

  for (const std::string& operationSpec : operationSpecs) {
    const std::string json = R"json({
      "output": "field.nii.gz",
      "operations": [)json" + operationSpec + R"json(]
    })json";

    const warp_field::WarpFieldSpec spec = warp_field::parseSpecJson(json);
    CHECK(spec.operations.size() == 1);
  }
}

TEST_CASE("Expansion and contraction move points radially", "[warp-field]")
{
  warp_field::WarpFieldSpec spec = baseSpec();
  warp_field::WarpOperation operation;
  operation.type = warp_field::OperationType::Expansion;
  operation.center = {0.0, 0.0, 0.0};
  operation.radii = {10.0, 10.0, 10.0};
  operation.amplitude = 2.0;
  spec.operations = {operation};

  glm::dvec3 displacement = warp_field::evaluateDisplacement(spec, {1.0, 0.0, 0.0}, 0.0);
  CHECK(displacement[0] > 0.0);
  CHECK_THAT(displacement[1], Catch::Matchers::WithinAbs(0.0, 1.0e-12));

  spec.operations[0].type = warp_field::OperationType::Contraction;
  displacement = warp_field::evaluateDisplacement(spec, {1.0, 0.0, 0.0}, 0.0);
  CHECK(displacement[0] < 0.0);
  CHECK_THAT(displacement[1], Catch::Matchers::WithinAbs(0.0, 1.0e-12));
}

TEST_CASE("Affine operations combine translation and linear displacement", "[warp-field]")
{
  warp_field::WarpFieldSpec spec = baseSpec();
  warp_field::WarpOperation operation;
  operation.type = warp_field::OperationType::Affine;
  operation.center = {0.0, 0.0, 0.0};
  operation.translation = {1.0, 2.0, 3.0};
  operation.matrix = {2.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 1.0};
  spec.operations = {operation};

  const glm::dvec3 displacement = warp_field::evaluateDisplacement(spec, {4.0, 5.0, 6.0}, 0.0);

  CHECK_THAT(displacement[0], Catch::Matchers::WithinAbs(5.0, 1.0e-12));
  CHECK_THAT(displacement[1], Catch::Matchers::WithinAbs(2.0, 1.0e-12));
  CHECK_THAT(displacement[2], Catch::Matchers::WithinAbs(3.0, 1.0e-12));
}

TEST_CASE("Composed operations sample later operations at the warped point", "[warp-field]")
{
  warp_field::WarpFieldSpec additive = baseSpec();
  warp_field::WarpOperation translation;
  translation.type = warp_field::OperationType::Affine;
  translation.translation = {1.0, 0.0, 0.0};

  warp_field::WarpOperation scale;
  scale.type = warp_field::OperationType::Affine;
  scale.matrix = {2.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 1.0};

  additive.operations = {translation, scale};

  warp_field::WarpFieldSpec composed = additive;
  composed.compositionMode = warp_field::CompositionMode::Composed;

  const glm::dvec3 point{2.0, 0.0, 0.0};
  const glm::dvec3 additiveDisplacement = warp_field::evaluateDisplacement(additive, point, 0.0);
  const glm::dvec3 composedDisplacement = warp_field::evaluateDisplacement(composed, point, 0.0);

  CHECK_THAT(additiveDisplacement[0], Catch::Matchers::WithinAbs(3.0, 1.0e-12));
  CHECK_THAT(composedDisplacement[0], Catch::Matchers::WithinAbs(4.0, 1.0e-12));
}

TEST_CASE("Inverse affine operations invert affine displacement", "[warp-field]")
{
  warp_field::WarpFieldSpec forward = baseSpec();
  warp_field::WarpOperation operation;
  operation.type = warp_field::OperationType::Affine;
  operation.matrix = {2.0, 0.0, 0.0, 0.0, 1.5, 0.0, 0.0, 0.0, 1.0};
  operation.translation = {3.0, -2.0, 0.0};
  forward.operations = {operation};

  warp_field::WarpFieldSpec inverse = baseSpec();
  operation.type = warp_field::OperationType::InverseAffine;
  inverse.operations = {operation};

  const glm::dvec3 point{4.0, 6.0, 0.0};
  const glm::dvec3 forwardDisplacement = warp_field::evaluateDisplacement(forward, point, 0.0);
  const glm::dvec3 movedPoint{
    point[0] + forwardDisplacement[0],
    point[1] + forwardDisplacement[1],
    point[2] + forwardDisplacement[2]};
  const glm::dvec3 inverseDisplacement = warp_field::evaluateDisplacement(inverse, movedPoint, 0.0);

  CHECK_THAT(movedPoint[0] + inverseDisplacement[0], Catch::Matchers::WithinAbs(point[0], 1.0e-12));
  CHECK_THAT(movedPoint[1] + inverseDisplacement[1], Catch::Matchers::WithinAbs(point[1], 1.0e-12));
}

TEST_CASE("Shear displaces along a direction by position along a normal", "[warp-field]")
{
  warp_field::WarpFieldSpec spec = baseSpec();
  warp_field::WarpOperation operation;
  operation.type = warp_field::OperationType::Shear;
  operation.normal = {0.0, 1.0, 0.0};
  operation.direction = {1.0, 0.0, 0.0};
  operation.amplitude = 0.5;
  spec.operations = {operation};

  const glm::dvec3 displacement = warp_field::evaluateDisplacement(spec, {0.0, 4.0, 0.0}, 0.0);

  CHECK_THAT(displacement[0], Catch::Matchers::WithinAbs(2.0, 1.0e-12));
  CHECK_THAT(displacement[1], Catch::Matchers::WithinAbs(0.0, 1.0e-12));
}

TEST_CASE("Control point fields interpolate sparse displacements", "[warp-field]")
{
  warp_field::WarpFieldSpec spec = baseSpec();
  warp_field::WarpOperation operation;
  operation.type = warp_field::OperationType::ControlPoint;
  operation.amplitude = 1.0;
  operation.width = 10.0;
  operation.controlPoints = {{0.0, 0.0, 0.0}};
  operation.displacements = {{3.0, 0.0, 0.0}};
  operation.weights = {10.0};
  spec.operations = {operation};

  const glm::dvec3 displacement = warp_field::evaluateDisplacement(spec, {0.0, 0.0, 0.0}, 0.0);

  CHECK_THAT(displacement[0], Catch::Matchers::WithinAbs(3.0, 1.0e-12));
  CHECK_THAT(displacement[1], Catch::Matchers::WithinAbs(0.0, 1.0e-12));
}

TEST_CASE("Time profiles modulate operation strength", "[warp-field]")
{
  warp_field::WarpFieldSpec spec = baseSpec();
  warp_field::WarpOperation operation;
  operation.type = warp_field::OperationType::Affine;
  operation.translation = {2.0, 0.0, 0.0};
  operation.time.mode = warp_field::TimeMode::Ramp;
  operation.time.amplitude = 1.0;
  spec.operations = {operation};

  const glm::dvec3 early = warp_field::evaluateDisplacement(spec, {0.0, 0.0, 0.0}, 0.0);
  const glm::dvec3 late = warp_field::evaluateDisplacement(spec, {0.0, 0.0, 0.0}, 1.0);

  CHECK_THAT(early[0], Catch::Matchers::WithinAbs(0.0, 1.0e-12));
  CHECK_THAT(late[0], Catch::Matchers::WithinAbs(2.0, 1.0e-12));
}

TEST_CASE("Warp field validation rejects inconsistent specifications", "[warp-field]")
{
  warp_field::WarpFieldSpec spec = baseSpec();
  spec.output.clear();
  spec.operations.push_back({});

  CHECK_THROWS_WITH(warp_field::validateSpec(spec), Catch::Matchers::ContainsSubstring("output image file"));
}

TEST_CASE("Warp field writer creates a small vector image", "[warp-field][itk]")
{
  warp_field::WarpFieldSpec spec = baseSpec();
  spec.output = std::filesystem::temp_directory_path() / "entropy-small-warp-field.nii.gz";
  spec.size = {3, 3, 3};

  warp_field::WarpOperation operation;
  operation.type = warp_field::OperationType::Affine;
  operation.translation = {1.0, 0.0, 0.0};
  spec.operations = {operation};

  warp_field::writeWarpField(spec);

  CHECK(std::filesystem::exists(spec.output));
  std::filesystem::remove(spec.output);
}
