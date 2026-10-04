#include "logic/app/Data.h"
#include "logic/app/ProjectSnapshot.h"
#include "logic/app/DeformationArchive.h"
#include "logic/app/ProjectSnapshotSettings.h"
#include "logic/app/ProjectSnapshotComparison.h"
#include "image/ImageWriter.h"
#include "logic/app/RegistrationInputs.h"
#include "logic/app/WarpInversionRequest.h"
#include "logic/app/ImportedMeshWarp.h"
#include "rendering/DistanceMapPolicy.h"
#include "rendering/mesh/MeshPicking.h"
#include "mesh/MeshIO.h"
#include "logic/annotation/Annotation.h"
#include "logic/annotation/AnnotPolygon.tpp"
#include "logic/segmentation/AnnotationSegmentation.h"
#include "rendering/mesh/MeshExtractionJobs.h"
#include "rendering/mesh/MeshSegmentationPolicy.h"
#include "registration/Execution.h"
#include "registration/Artifacts.h"
#include "registration/Json.h"
#include "../../test/support/CompletionGate.h"
#include "../../test/support/AnalyticImages.h"
#include "../../test/support/TempDirectory.h"
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <future>
#include <memory>
#include <nlohmann/json.hpp>

using namespace entropy::test;

TEST_CASE(
  "Registration snapshots the requested vector component and time frame rather than the displayed selection",
  "[workflow][registration][time]")
{
  TempDirectory directory;
  const auto base = translationField(Grid{}, {0, 0, 0});
  constexpr std::size_t count = std::size_t{9} * 11 * 7;
  std::array<std::vector<float>, 3> components;
  for (unsigned c = 0; c < 3; ++c) {
    components[c].resize(count * 2);
    for (unsigned t = 0; t < 2; ++t)
      for (std::size_t i = 0; i < count; ++i)
        components[c][t * count + i] = static_cast<float>(c * 100 + t * 10 + i);
  }
  auto image = Image::fromCopiedData(
    base.header(),
    "time vector",
    Image::ImageRepresentation::Image,
    Image::MultiComponentBufferType::SeparateImages,
    {components[0].data(), components[1].data(), components[2].data()},
    ImageTimeAxis{2, 0, 1, "s"});
  image.settings().setActiveTimePoint(0);
  image.settings().setActiveComponent(2);
  AppData data;
  const auto uid = data.addImage(std::move(image));
  registration::JobSpec job;
  job.dimension = 3;
  job.useCurrentAffineTransformsForInitialization = false;
  job.backend = registration::Backend::FireANTs;
  job.outputDirectory = directory.path() / "component-job";
  job.fixedImage.uid = uuids::to_string(uid);
  job.fixedImage.source = registration::DataSource::LoadedImage;
  job.fixedImage.component = 1;
  job.fixedImage.timePoint = 1;
  job.movingImage = job.fixedImage;
  REQUIRE(registration_inputs::materialize(data, job));
  struct Runner : registration::IProcessRunner
  {
    bool inspected = false;
    registration::ProcessResult run(
      const registration::CommandSpec&,
      const registration::ProcessOptions& options,
      const registration::ProcessCallbacks&) override
    {
      registration::JobSpec lookup;
      lookup.backend = registration::Backend::FireANTs;
      lookup.outputDirectory = options.workingDirectory;
      const auto submitted =
        nlohmann::json::parse(readBytes(registration::artifactPath(lookup, registration::ArtifactRole::JobSpec)))
          .get<registration::JobSpec>();
      Image input(
        submitted.movingImage.fileName,
        Image::ImageRepresentation::Image,
        Image::MultiComponentBufferType::SeparateImages);
      CHECK(input.header().numComponentsPerPixel() == 1);
      CHECK_FALSE(input.isTimeSeries());
      CHECK(input.value<float>(0, 0) == 110.0f);
      CHECK(input.value<float>(0, 1) == 111.0f);
      inspected = true;
      registration::ProcessResult result;
      result.exitCode = 0;
      return result; // Intentionally produces no outputs; completion must fail.
    }
  } runner;
  const auto result = registration::executeJob(job, runner);
  CHECK(runner.inspected);
  CHECK(result.status == registration::JobStatus::Failed);
  CHECK(data.image(uid)->settings().activeComponent() == 2);
  CHECK(data.image(uid)->settings().activeTimePoint() == 0);
}

TEST_CASE(
  "Oblique holed annotations preserve labels and mesh holes through export and reload",
  "[workflow][annotation][segmentation][mesh][export]")
{
  TempDirectory directory;
  Grid grid;
  grid.size = {24, 24, 7};
  auto seg = labels(grid);
  const glm::vec3 center{grid.subject({12, 12, 3})};
  const glm::vec3 normal{grid.direction[2]};
  Annotation annotation("oblique donut", glm::vec4{1}, glm::vec4{normal, -glm::dot(normal, center)});
  const auto p = annotation.projectSubjectPointToAnnotationPlane(center);
  annotation.polygon().setAllVertices(
    {{p + glm::vec2{-5, -5}, p + glm::vec2{5, -5}, p + glm::vec2{5, 5}, p + glm::vec2{-5, 5}},
     {p + glm::vec2{-2, -2}, p + glm::vec2{-2, 2}, p + glm::vec2{2, 2}, p + glm::vec2{2, -2}}});
  annotation.setClosed(true);
  fillSegmentationWithPolygon(seg, &annotation, 7, 0, true, [](const auto&, const auto&, const auto&, const auto*) {});
  CHECK(seg.value<int>(0, 12, 12, 3) == 0);
  const auto inventory = rendering::mesh::segmentationLabelInventory(seg, 0);
  REQUIRE(inventory);
  REQUIRE(inventory->contains(7));
  rendering::mesh::MeshGenerationOptions options;
  options.smoothSurface = false;
  const auto request = rendering::mesh::makeScalarGridSegmentationRequest(
    generateRandomUuid(),
    seg.pixelDataRevision(),
    seg.geometryRevision(),
    7,
    0,
    options);
  const auto result =
    rendering::mesh::makeSegmentationExtractionJob(request, inventory->at(7), std::make_shared<Image>(seg))();
  REQUIRE(result.result);
  REQUIRE_FALSE(result.result->mesh.positions.empty());
  CHECK_FALSE(rendering::mesh::pickNearestTriangle(result.result->mesh, {center + 100.0f * normal, -normal}));
  const auto path = directory.path() / "donut.nrrd";
  REQUIRE(static_cast<bool>(image_io::writeImage(seg, path)));
  Image reopened(path, Image::ImageRepresentation::Segmentation, Image::MultiComponentBufferType::SeparateImages);
  for (unsigned z = 0; z < grid.size.z; ++z)
    for (unsigned y = 0; y < grid.size.y; ++y)
      for (unsigned x = 0; x < grid.size.x; ++x)
        CHECK(reopened.value<int>(0, x, y, z) == seg.value<int>(0, x, y, z));
  const auto meshPath = directory.path() / "donut.vtp";
  mesh::MeshRecord record;
  record.geometry.positions = result.result->mesh.positions;
  record.geometry.normals = result.result->mesh.normals;
  record.geometry.triangleIndices = result.result->mesh.indices;
  mesh::MeshIO io;
  REQUIRE(io.write({.path = meshPath, .mesh = &record}).has_value());
  const auto reloaded = io.load({.path = meshPath, .meshUid = "donut", .associatedImageUid = "seg"});
  REQUIRE(reloaded);
  CHECK(reloaded->mesh.geometry.positions == result.result->mesh.positions);
}

TEST_CASE(
  "Swapping equal revision forward warps updates mesh geometry picking and exported vertices",
  "[workflow][mesh][warp][export]")
{
  TempDirectory directory;
  AppData data;
  Grid grid;
  grid.direction = glm::dmat3{1};
  grid.origin = {0, 0, 0};
  grid.spacing = {1, 1, 1};
  const auto uid = data.addImage(scalarRamp(grid));
  data.image(uid)->settings().setWarpEnabled(true);
  data.image(uid)->settings().setWarpStrength(1);
  const auto a = data.addDef(translationField(grid, {1, 0, 0}));
  const auto b = data.addDef(translationField(grid, {-1, 0, 0}));
  REQUIRE(a);
  REQUIRE(b);
  REQUIRE(data.def(*a)->pixelDataRevision() == data.def(*b)->pixelDataRevision());
  mesh::MeshGeometry triangle;
  triangle.positions = {{2, 2, 2}, {4, 2, 2}, {2, 4, 2}};
  triangle.triangleIndices = {0, 1, 2};
  REQUIRE(data.assignForwardWarpUidToImage(uid, *a));
  const auto firstVersion = deformation_warp::warpedGeometryVersion(data, uid, *data.image(uid));
  const auto first = deformation_warp::prepareImportedGeometry(data, uid, triangle);
  REQUIRE(first);
  REQUIRE(data.assignForwardWarpUidToImage(uid, *b));
  CHECK(deformation_warp::warpedGeometryVersion(data, uid, *data.image(uid)) != firstVersion);
  const auto second = deformation_warp::prepareImportedGeometry(data, uid, triangle);
  REQUIRE(second);
  CHECK(first->positions.front() == glm::vec3(3, 2, 2));
  CHECK(second->positions.front() == glm::vec3(1, 2, 2));
  rendering::mesh::MeshData display;
  display.positions = second->positions;
  display.indices = second->triangleIndices;
  const auto hit = rendering::mesh::pickNearestTriangle(display, {{1.5f, 2.5f, 10}, {0, 0, -1}});
  REQUIRE(hit);
  CHECK(hit->worldPosition == glm::vec3(1.5f, 2.5f, 2));
  mesh::MeshRecord record;
  record.geometry = *second;
  const auto path = directory.path() / "warped.vtp";
  mesh::MeshIO io;
  REQUIRE(io.write({.path = path, .mesh = &record}).has_value());
  const auto reloaded = io.load({.path = path, .meshUid = "triangle", .associatedImageUid = uuids::to_string(uid)});
  if (!reloaded) INFO(reloaded.error().message);
  REQUIRE(reloaded);
  CHECK(reloaded->mesh.geometry.positions == display.positions);
  CHECK(triangle.positions.front() == glm::vec3(2, 2, 2));
}

TEST_CASE("Equal histogram time frames cannot consume a pending static distance map", "[workflow][rendering][time]")
{
  Grid grid;
  const auto staticImage = scalarRamp(grid);
  const auto count = static_cast<std::size_t>(grid.size.x) * grid.size.y * grid.size.z;
  std::vector<float> frames(count * 2);
  for (std::size_t i = 0; i < count; ++i) {
    frames[i] = static_cast<float>(i);
    frames[count + i] = static_cast<float>(count - 1 - i);
  }
  auto image = Image::fromCopiedData(
    staticImage.header(),
    "moving content",
    Image::ImageRepresentation::Image,
    Image::MultiComponentBufferType::SeparateImages,
    {frames.data()},
    ImageTimeAxis{2, 0, 1, "s"});
  REQUIRE(rendering::distanceMapEligible(staticImage, true));
  std::future<bool> pending;
  CompletionGate gate;
  pending = std::async(std::launch::async, [pause = gate.waiter()] {
    pause();
    return true;
  });
  REQUIRE(gate.awaitEntry());
  image.settings().setActiveTimePoint(1);
  CHECK_FALSE(rendering::distanceMapEligible(image, true));
  gate.release();
  REQUIRE(pending.get());
  CHECK_FALSE(rendering::distanceMapEligible(image, true));
  image.settings().setActiveTimePoint(0);
  CHECK_FALSE(rendering::distanceMapEligible(image, true));
  CHECK_FALSE(rendering::distanceMapEligible(staticImage, false));
}

TEST_CASE(
  "Generated warp assets and metadata roll back when project publication fails",
  "[workflow][project][warp][filesystem]")
{
  TempDirectory directory;
  AppData data;
  auto referenceImage = scalarRamp();
  referenceImage.header().setFileName(directory.path() / "reference.nrrd");
  REQUIRE(static_cast<bool>(image_io::writeImage(referenceImage, referenceImage.header().fileName())));
  const auto reference = data.addImage(std::move(referenceImage));
  REQUIRE(data.setRefImageUid(reference));
  const auto warp = data.addDef(translationField(Grid{}, {1, 0, 0}));
  REQUIRE(warp);
  REQUIRE(data.assignForwardWarpUidToImage(reference, *warp));
  const auto originalPath = data.def(*warp)->header().fileName();
  const auto projectPath = directory.path() / "project.json";
  std::filesystem::path publishedAsset;
  const auto failed =
    project_snapshot::persistProject(data, projectPath, {}, {}, [&](const auto& snapshot, const auto&) {
      REQUIRE(snapshot.m_referenceImage.m_warpFields.size() == 1);
      publishedAsset = snapshot.m_referenceImage.m_warpFields.front().m_path;
      CHECK(std::filesystem::exists(publishedAsset));
      return false;
    });
  CHECK_FALSE(failed);
  CHECK_FALSE(std::filesystem::exists(publishedAsset));
  CHECK_FALSE(data.def(*warp)->header().existsOnDisk());
  CHECK(data.def(*warp)->header().fileName() == originalPath);
  const auto saved = project_snapshot::persistProject(data, projectPath);
  REQUIRE(saved);
  serialize::EntropyProject loaded;
  REQUIRE(serialize::open(loaded, projectPath));
  REQUIRE(loaded.m_referenceImage.m_warpFields.size() == 1);
  const auto& asset = loaded.m_referenceImage.m_warpFields.front();
  CHECK(asset.m_activeForward);
  Image reopened(asset.m_path, Image::ImageRepresentation::Image, Image::MultiComponentBufferType::SeparateImages);
  CHECK(reopened.value<float>(0, 1, 1, 1) == 1.0f);
  const auto bytes = readBytes(asset.m_path);
  CHECK(project_snapshot::persistProject(data, projectPath).has_value());
  CHECK(readBytes(asset.m_path) == bytes);
}

TEST_CASE("Deformation bundle publication follows the project transaction", "[workflow][project][deformation]")
{
  TempDirectory directory;
  AppData data;
  auto referenceImage = scalarRamp();
  referenceImage.header().setFileName(directory.path() / "reference.nrrd");
  REQUIRE(static_cast<bool>(image_io::writeImage(referenceImage, referenceImage.header().fileName())));
  const auto reference = data.addImage(std::move(referenceImage));
  REQUIRE(data.setRefImageUid(reference));

  deformation::DomainGeometry geometry;
  geometry.dimension = deformation::SpatialDimension::Plane;
  geometry.size = {3, 3, 1};
  const deformation::FieldDomain domain(geometry);
  auto initial = std::make_shared<deformation::FieldCheckpoint>();
  initial->forward.assign(domain.sampleCount(), {0, 0, 0, 1});
  initial->inverse.assign(domain.sampleCount(), {0, 0, 0, 1});
  const deformation::EditHistory history(
    domain,
    domain,
    {uuids::to_string(reference), uuids::to_string(reference), "identity", 0},
    {1},
    initial);
  const std::vector<project_snapshot::DeformationArchiveSource> sources{{"edit_1", &history}};
  const auto projectFile = directory.path() / "project.json";
  std::filesystem::path abandoned;
  const auto failed = project_snapshot::persistProject(
    data,
    projectFile,
    {},
    {},
    [&](const auto& snapshot, const auto&) {
      REQUIRE(snapshot.m_deformationEdits.size() == 1);
      abandoned = snapshot.m_deformationEdits.front().m_manifestPath;
      REQUIRE(std::filesystem::exists(abandoned));
      return false;
    },
    sources);
  CHECK_FALSE(failed);
  CHECK_FALSE(std::filesystem::exists(abandoned));
  CHECK_FALSE(std::filesystem::exists(projectFile));

  const auto saved = project_snapshot::persistProject(data, projectFile, {}, {}, serialize::save, sources);
  REQUIRE(saved);
  REQUIRE(saved->m_deformationEdits.size() == 1);
  serialize::EntropyProject loaded;
  REQUIRE(serialize::open(loaded, projectFile));
  REQUIRE(loaded.m_deformationEdits.size() == 1);
  const auto restored = project_snapshot::restoreDeformationHistories(loaded, 1024);
  REQUIRE(restored.size() == 1);
  CHECK(restored.at("edit_1").current().id() == history.current().id());
  CHECK(restored.at("edit_1").current().checkpoint().forward == history.current().checkpoint().forward);
}

TEST_CASE(
  "Reopening a project retains the second segmentation as the painting target",
  "[workflow][project][segmentation]")
{
  TempDirectory directory;
  AppData source;
  const auto imageUid = source.addImage(scalarRamp());
  REQUIRE(source.setRefImageUid(imageUid));
  source.addLabelColorTable(16, 256);
  REQUIRE(source.setActiveImageUid(imageUid));
  std::vector<uuids::uuid> segmentations;
  for (unsigned i = 0; i < 2; ++i) {
    const auto path = directory.path() / ("labels-" + std::to_string(i) + ".nrrd");
    auto image = labels();
    REQUIRE(static_cast<bool>(image_io::writeImage(image, path)));
    Image fromDisk(path, Image::ImageRepresentation::Segmentation, Image::MultiComponentBufferType::SeparateImages);
    const auto uid = source.addSeg(std::move(fromDisk));
    REQUIRE(uid);
    REQUIRE(source.assignSegUidToImage(imageUid, *uid));
    segmentations.push_back(*uid);
  }
  REQUIRE(source.assignActiveSegUidToImage(imageUid, segmentations.back()));
  source.image(imageUid)->header().setFileName(directory.path() / "image.nrrd");
  REQUIRE(
    static_cast<bool>(image_io::writeImage(*source.image(imageUid), source.image(imageUid)->header().fileName())));
  const auto saved = project_snapshot::captureProject(source);
  const auto projectPath = directory.path() / "project.json";
  REQUIRE(serialize::save(saved, projectPath));
  serialize::EntropyProject reopened;
  REQUIRE(serialize::open(reopened, projectPath));
  AppData restored;
  const auto restoredImage = restored.addImage(scalarRamp());
  restored.addLabelColorTable(16, 256);
  REQUIRE(restored.setActiveImageUid(restoredImage));
  std::vector<uuids::uuid> restoredSegs;
  for (const auto& record : reopened.m_referenceImage.m_segmentations) {
    Image seg(
      record.m_segFileName,
      Image::ImageRepresentation::Segmentation,
      Image::MultiComponentBufferType::SeparateImages);
    const auto uid = restored.addSeg(std::move(seg));
    REQUIRE(uid);
    REQUIRE(restored.assignSegUidToImage(restoredImage, *uid));
    project_snapshot::restoreSegmentationState(restored, restoredImage, *uid, reopened.m_referenceImage, true);
    restoredSegs.push_back(*uid);
  }
  REQUIRE(restoredSegs.size() == 2);
  REQUIRE(restored.imageToActiveSegUid(restoredImage) == restoredSegs.back());
  REQUIRE(restored.seg(*restored.imageToActiveSegUid(restoredImage))->setValue(0, 1, 2, 3, uint16_t{4}));
  CHECK(restored.seg(restoredSegs.front())->value<int>(0, 1, 2, 3) == 0);
  CHECK(restored.seg(restoredSegs.back())->value<int>(0, 1, 2, 3) == 4);
}

TEST_CASE(
  "A pending warp cannot publish after its reference selection or source revision changes",
  "[workflow][warp][threading]")
{
  const int mutation = GENERATE(0, 1, 2, 3, 4, 5, 6, 7, 8, 9);
  AppData data;
  const auto reference = data.addImage(scalarRamp());
  const auto moving = data.addImage(scalarRamp());
  REQUIRE(data.setRefImageUid(reference));
  const auto source = data.addDef(translationField(Grid{}, {0.1f, 0, 0}));
  REQUIRE(source);
  REQUIRE(data.assignForwardWarpUidToImage(moving, *source));
  const auto* field = data.warpField(*source);
  warp_inversion::RequestState request{
    .imageUid = moving,
    .sourceWarpUid = *source,
    .domainUid = reference,
    .referenceUid = reference,
    .targetWarpUid = data.imageToActiveInverseWarpUid(moving),
    .sourcePixelRevision = field->pixelDataRevision(),
    .sourceGeometryRevision = field->geometryRevision(),
    .domainGeometryRevision = data.image(reference)->geometryRevision(),
    .imageGeometryRevision = data.image(moving)->geometryRevision(),
    .sourceTimePoint = field->settings().activeTimePoint(),
    .sourceTransform = field->transformations().worldDef_T_subject(),
    .imageTransform = data.image(moving)->transformations().worldDef_T_subject(),
    .cancel = std::make_shared<std::atomic_bool>(false)};
  REQUIRE(warp_inversion::canPublish(data, request, true));
  std::future<Image> pending;
  CompletionGate gate; // Releases before pending future joins, even on REQUIRE failure.
  pending = std::async(std::launch::async, [copy = *field, pause = gate.waiter()]() mutable {
    pause();
    return copy;
  });
  REQUIRE(gate.awaitEntry());
  bool latest = true;
  switch (mutation) {
    case 0:
      break;
    case 1:
      REQUIRE(data.setRefImageUid(moving));
      break;
    case 2:
      REQUIRE(data.def(*source)->setValue(0, 1, 1, 1, 42.0f));
      break;
    case 3: {
      const auto replacement = data.addDef(translationField(Grid{}, {-0.1f, 0, 0}));
      REQUIRE(replacement);
      REQUIRE(data.assignForwardWarpUidToImage(moving, *replacement));
      break;
    }
    case 4:
      request.cancel->store(true);
      break;
    case 5:
      latest = false;
      break;
    case 6: {
      auto& tx = data.image(moving)->transformations();
      tx.set_worldDef_T_affine_locked(false);
      tx.set_worldDef_T_affine(shear());
      break;
    }
    case 7:
      REQUIRE(data.removeDef(*source));
      break;
    case 8: {
      auto overrides = data.image(reference)->header().getHeaderOverrides();
      overrides.m_useZeroPixelOrigin = true;
      data.image(reference)->setHeaderOverrides(overrides);
      break;
    }
    case 9: {
      const auto selected = data.addDef(translationField(Grid{}, {2, 0, 0}));
      REQUIRE(selected);
      REQUIRE(data.assignInverseWarpUidToImage(moving, *selected, reference));
      break;
    }
    default:
      FAIL("Unexpected mutation");
  }
  const auto selectedTarget = data.imageToActiveInverseWarpUid(moving);
  gate.release();
  REQUIRE(pending.wait_for(std::chrono::seconds{5}) == std::future_status::ready);
  const auto completed = pending.get();
  CHECK(completed.value<float>(0, 1, 1, 1) == 0.1f);
  CHECK(warp_inversion::canPublish(data, request, latest) == (mutation == 0));
  CHECK(data.imageToActiveInverseWarpUid(moving) == selectedTarget);
}

TEST_CASE(
  "Registration freezes edited segmentation pixels while a later export stays dirty",
  "[workflow][registration][segmentation][export]")
{
  TempDirectory directory;
  AppData data;
  const auto fixed = data.addImage(scalarRamp());
  const auto moving = data.addImage(scalarRamp());
  const auto seg = data.addSeg(labels());
  REQUIRE(seg);
  data.addLabelColorTable(16, 256);
  REQUIRE(data.setActiveImageUid(moving));
  REQUIRE(data.assignSegUidToImage(moving, *seg));
  REQUIRE(data.seg(*seg)->setValue(0, 2, 3, 1, uint16_t{7}));
  registration::JobSpec job;
  job.useCurrentAffineTransformsForInitialization = false;
  job.dimension = 3;
  job.outputDirectory = directory.path() / "owned-job";
  job.fixedImage.uid = uuids::to_string(fixed);
  job.fixedImage.source = registration::DataSource::LoadedImage;
  job.movingImage.uid = uuids::to_string(moving);
  job.movingImage.source = registration::DataSource::LoadedImage;
  job.movingMask.uid = uuids::to_string(*seg);
  job.movingMask.source = registration::DataSource::Segmentation;
  REQUIRE(registration_inputs::materialize(data, job));
  const auto frozenBytes = readBytes(job.movingMask.fileName);
  CHECK_FALSE(registration_inputs::materialize(data, job)); // Cannot reuse a job's workspace.
  CHECK(readBytes(job.movingMask.fileName) == frozenBytes);
  const auto exportRevision = data.seg(*seg)->pixelDataRevision();
  const Image exportCopy = *data.seg(*seg);
  REQUIRE(data.seg(*seg)->setValue(0, 2, 3, 1, uint16_t{9}));
  const auto exported = directory.path() / "labels.nrrd";
  REQUIRE(static_cast<bool>(image_io::writeImage(exportCopy, exported)));
  REQUIRE(data.recordSegmentationExport(*seg, exported, exportRevision));
  CHECK(data.segmentationHasUnsavedVoxelChanges(*seg));
  CHECK(readBytes(job.movingMask.fileName) == frozenBytes);
  Image registrationMask(
    job.movingMask.fileName,
    Image::ImageRepresentation::Segmentation,
    Image::MultiComponentBufferType::SeparateImages);
  Image exportedMask(
    exported,
    Image::ImageRepresentation::Segmentation,
    Image::MultiComponentBufferType::SeparateImages);
  CHECK(registrationMask.value<int>(0, 2, 3, 1) == 7);
  CHECK(exportedMask.value<int>(0, 2, 3, 1) == 7);
  CHECK(data.seg(*seg)->value<int>(0, 2, 3, 1) == 9);
  CHECK(job.ownedInputFiles.size() == 3);
}

TEST_CASE(
  "Disabled stored transforms survive application snapshot save reopen and enable",
  "[workflow][project][transform]")
{
  TempDirectory directory;
  AppData data;
  const auto referenceUid = data.addImage(scalarRamp());
  REQUIRE(data.setRefImageUid(referenceUid));
  auto moving = scalarRamp();
  const auto imagePath = directory.path() / "moving.nrrd";
  REQUIRE(static_cast<bool>(image_io::writeImage(moving, imagePath)));
  moving.header().setFileName(imagePath);
  auto& tx = moving.transformations();
  tx.set_worldDef_T_affine_locked(false);
  tx.set_worldDef_T_affine(shear());
  tx.set_affine_T_subject(glm::translate(glm::mat4{1}, glm::vec3{2, 4, 6}));
  tx.set_enable_worldDef_T_affine(false);
  tx.set_enable_affine_T_subject(false);
  auto overrides = moving.header().getHeaderOverrides();
  overrides.m_useZeroPixelOrigin = true;
  moving.setHeaderOverrides(overrides);
  const auto movingUid = data.addImage(std::move(moving));
  const auto saved = project_snapshot::captureProject(data);
  REQUIRE(saved.m_additionalImages.size() == 1);
  const auto path = directory.path() / "project.json";
  REQUIRE(serialize::save(saved, path));
  serialize::EntropyProject reopened;
  REQUIRE(serialize::open(reopened, path));
  const auto& record = reopened.m_additionalImages.front();
  Image loaded(imagePath, Image::ImageRepresentation::Image, Image::MultiComponentBufferType::SeparateImages);
  project_snapshot::restoreImageState(loaded, record, false, [](const auto&, const auto&, const auto&) {
    FAIL("Unexpected load error");
  });
  CHECK(loaded.transformations().worldDef_T_subject() == glm::mat4{1});
  CHECK(loaded.header().getHeaderOverrides().m_useZeroPixelOrigin);
  const auto& original = data.image(movingUid)->transformations();
  CHECK(loaded.transformations().stored_worldDef_T_affine() == original.stored_worldDef_T_affine());
  CHECK(loaded.transformations().stored_affine_T_subject() == original.stored_affine_T_subject());
  loaded.transformations().set_worldDef_T_affine_locked(false);
  loaded.transformations().set_enable_worldDef_T_affine(true);
  loaded.transformations().set_enable_affine_T_subject(true);
  const auto expected = original.stored_worldDef_T_affine() * original.stored_affine_T_subject();
  for (int c = 0; c < 4; ++c)
    for (int r = 0; r < 4; ++r)
      CHECK(loaded.transformations().worldDef_T_subject()[c][r] == Catch::Approx(expected[c][r]).margin(1e-5));
  data.image(movingUid)->transformations().set_enable_affine_T_subject(true);
  CHECK_FALSE(project_snapshot::equivalent(saved, project_snapshot::captureProject(data)));
}

TEST_CASE(
  "Analytic spatial image and vector fields preserve physical samples across export reload",
  "[workflow][image][warp]")
{
  TempDirectory directory;
  const Grid grid;
  auto image = scalarRamp(grid);
  SECTION("scalar ramp") {}
  SECTION("constant translation")
  {
    image = translationField(grid, {1, -2, 0.5});
  }
  SECTION("nonlinear bending")
  {
    image = bendingField(grid);
  }
  const auto path = directory.path() / "analytic.nrrd";
  REQUIRE(static_cast<bool>(image_io::writeImage(image, path)));
  Image loaded(path, Image::ImageRepresentation::Image, Image::MultiComponentBufferType::SeparateImages);
  for (unsigned z = 0; z < grid.size.z; ++z)
    for (unsigned y = 0; y < grid.size.y; ++y)
      for (unsigned x = 0; x < grid.size.x; ++x) {
        const auto physical = grid.subject({x, y, z});
        const glm::vec3 restored = loaded.transformations().subject_T_pixel() * glm::vec4{x, y, z, 1};
        for (int axis = 0; axis < 3; ++axis)
          CHECK(restored[axis] == Catch::Approx(physical[axis]).margin(1e-5));
        for (unsigned c = 0; c < image.header().numComponentsPerPixel(); ++c)
          CHECK(loaded.value<float>(c, x, y, z) == image.value<float>(c, x, y, z));
      }
}
