#include "logic/app/DeformationEditController.h"

#include <catch2/catch_test_macros.hpp>

#include <bit>
#include <cstdint>
#include <functional>
#include <memory>
#include <new>
#include <vector>

namespace df = deformation;
namespace edit = deformation_edit;

namespace
{
df::FieldDomain domain()
{
  df::DomainGeometry spec;
  spec.dimension = df::SpatialDimension::Plane;
  spec.size = {3, 3, 1};
  return df::FieldDomain(spec);
}
std::shared_ptr<const df::FieldCheckpoint> identity(const df::FieldDomain& field)
{
  auto checkpoint = std::make_shared<df::FieldCheckpoint>();
  checkpoint->forward.assign(field.sampleCount(), {0, 0, 0, 1});
  checkpoint->inverse.assign(field.sampleCount(), {0, 0, 0, 1});
  return checkpoint;
}
df::QualityReport quality()
{
  df::QualityReport report;
  for (auto* direction : {&report.forward, &report.inverse}) {
    direction->requested = 1;
    direction->evaluated = 1;
    direction->minDeterminant = 1;
    direction->maxDeterminant = 1;
    direction->minSingularValue = 1;
    direction->maxSingularValue = 1;
  }
  report.protectionChecked = true;
  report.convergenceChecked = true;
  report.cellsVerified = true;
  return report;
}
df::BrushDefinition brush()
{
  df::BrushDefinition step;
  step.dimension = df::SpatialDimension::Plane;
  step.motion = df::PushMotion{{0.125, 0, 0}};
  return step;
}
edit::Dependencies dependencies()
{
  edit::Dependencies live;
  live.provenance = {"source", "reference", "baseline", 0};
  live.sourcePixelRevision = 3;
  live.sourceGeometryRevision = 4;
  live.referenceGeometryRevision = 5;
  live.baselineRevision = 6;
  live.maskRevision = 7;
  return live;
}
edit::DeformationEditSession session()
{
  const auto field = domain();
  return edit::DeformationEditSession(
    df::EditHistory(field, field, dependencies().provenance, {1}, identity(field)),
    {});
}

class FakeBackend final : public edit::Backend
{
public:
  enum class Mode
  {
    Accept,
    Reject,
    BadReport,
    AllocationFailure
  };
  Mode mode = Mode::Accept;
  std::function<void()> afterCalculation;
  int calls = 0;

  edit::Result evaluate(const edit::Request& request) override
  {
    ++calls;
    if (mode == Mode::AllocationFailure) throw std::bad_alloc();
    auto next = std::make_shared<df::FieldCheckpoint>(*request.base);
    for (auto& value : next->forward)
      value.x += 0.125f;
    for (auto& value : next->inverse)
      value.x -= 0.125f;
    if (afterCalculation) afterCalculation();
    return {
      next,
      mode == Mode::BadReport ? df::QualityReport{} : quality(),
      {mode == Mode::Reject ? df::CandidateDecision::Reject : df::CandidateDecision::Accept,
       mode == Mode::Reject ? df::QualityReason::Distortion : df::QualityReason::Passed}};
  }
};
} // namespace

TEST_CASE("One stroke publishes all microsteps atomically and undo restores exact snapshots", "[deformation-session]")
{
  auto live = dependencies();
  auto state = session();
  FakeBackend backend;
  edit::DeformationEditController controller(state, backend, [&] { return live; });
  const auto root = state.active();
  REQUIRE(controller.beginStroke());
  for (int i = 0; i < 3; ++i) {
    REQUIRE(controller.appendStep(brush()) == edit::Completion::Provisional);
    REQUIRE(state.active() == root);
    REQUIRE(state.preview());
    REQUIRE(state.preview()->acceptedRevision == root->id());
  }
  REQUIRE(state.preview()->checkpoint->forward.front().x == 0.375f);
  REQUIRE(controller.endStroke());
  const auto accepted = state.active();
  REQUIRE(accepted->id() == df::RevisionId{2});
  REQUIRE(accepted->stroke().size() == 3);
  REQUIRE_FALSE(state.preview());
  REQUIRE(controller.undo());
  REQUIRE(state.active() == root);
  REQUIRE(controller.redo(accepted->id()));
  REQUIRE(state.active() == accepted);
  REQUIRE(
    std::bit_cast<std::uint32_t>(state.active()->checkpoint().forward.front().x) ==
    std::bit_cast<std::uint32_t>(0.375f));
  REQUIRE(backend.calls == 3);
}

TEST_CASE(
  "Undo branches without replay and rejected or failed strokes preserve accepted values",
  "[deformation-session]")
{
  auto live = dependencies();
  auto state = session();
  FakeBackend backend;
  edit::DeformationEditController controller(state, backend, [&] { return live; });
  REQUIRE(controller.beginStroke());
  REQUIRE(controller.appendStep(brush()) == edit::Completion::Provisional);
  REQUIRE(controller.endStroke());
  const auto first = state.active();
  REQUIRE(controller.undo());
  REQUIRE(controller.beginStroke());
  REQUIRE(controller.appendStep(brush()) == edit::Completion::Provisional);
  REQUIRE(controller.appendStep(brush()) == edit::Completion::Provisional);
  REQUIRE(controller.endStroke());
  const auto branch = state.active();
  REQUIRE((state.history().children({1}) == std::vector<df::RevisionId>{first->id(), branch->id()}));
  REQUIRE(controller.undo());
  REQUIRE(controller.redo(first->id()));
  REQUIRE(state.active() == first);

  backend.mode = FakeBackend::Mode::Reject;
  REQUIRE(controller.beginStroke());
  REQUIRE(controller.appendStep(brush()) == edit::Completion::Rejected);
  REQUIRE_FALSE(state.strokeActive());
  REQUIRE(state.active() == first);
  backend.mode = FakeBackend::Mode::BadReport;
  REQUIRE(controller.beginStroke());
  REQUIRE(controller.appendStep(brush()) == edit::Completion::Rejected);
  REQUIRE(state.active() == first);
  backend.mode = FakeBackend::Mode::AllocationFailure;
  REQUIRE(controller.beginStroke());
  REQUIRE_THROWS_AS(controller.appendStep(brush()), std::bad_alloc);
  REQUIRE_FALSE(state.strokeActive());
  REQUIRE(state.active() == first);
}

TEST_CASE("Cancellation deletion frame changes and stale tickets cannot publish", "[deformation-session]")
{
  auto live = dependencies();
  auto state = session();
  FakeBackend backend;
  edit::DeformationEditController controller(state, backend, [&] { return live; });
  const auto root = state.active();

  REQUIRE(state.beginStroke(live));
  const auto pending = state.prepareStep(brush(), live);
  REQUIRE(pending);
  REQUIRE_FALSE(state.prepareStep(brush(), live));
  state.cancelStroke();
  REQUIRE(pending->cancel->load());
  REQUIRE(state.complete(pending, backend.evaluate(*pending), live) == edit::Completion::Stale);
  REQUIRE(state.active() == root);

  REQUIRE(state.beginStroke(live));
  const auto frameRequest = state.prepareStep(brush(), live);
  live.provenance.frame = 1;
  REQUIRE(state.complete(frameRequest, backend.evaluate(*frameRequest), live) == edit::Completion::Stale);
  REQUIRE_FALSE(state.endStroke(live));
  REQUIRE(state.active() == root);
  live = dependencies();

  REQUIRE(state.beginStroke(live));
  const auto deletedRequest = state.prepareStep(brush(), live);
  live.sourcePresent = false;
  REQUIRE(state.complete(deletedRequest, backend.evaluate(*deletedRequest), live) == edit::Completion::Stale);
  REQUIRE(state.active() == root);
  live = dependencies();

  REQUIRE(controller.beginStroke());
  REQUIRE(controller.appendStep(brush()) == edit::Completion::Provisional);
  controller.cancelStroke();
  REQUIRE_FALSE(controller.endStroke());
  REQUIRE(state.active() == root);

  backend.afterCalculation = [&] {
    ++live.maskRevision;
  };
  REQUIRE(controller.beginStroke());
  REQUIRE(controller.appendStep(brush()) == edit::Completion::Stale);
  REQUIRE(state.active() == root);
}

TEST_CASE("Every captured image and baseline dependency invalidates pending work", "[deformation-session]")
{
  const std::vector<std::function<void(edit::Dependencies&)>> changes{
    [](auto& live) { live.sourcePresent = false; },
    [](auto& live) { live.referencePresent = false; },
    [](auto& live) { live.baselinePresent = false; },
    [](auto& live) { live.provenance.sourceImageId = "other"; },
    [](auto& live) { live.provenance.referenceImageId = "other"; },
    [](auto& live) { live.provenance.baselineId = "other"; },
    [](auto& live) { ++live.provenance.frame; },
    [](auto& live) { ++live.sourcePixelRevision; },
    [](auto& live) { ++live.sourceGeometryRevision; },
    [](auto& live) { ++live.referenceGeometryRevision; },
    [](auto& live) { ++live.baselineRevision; },
    [](auto& live) {
      ++live.maskRevision;
    }};
  FakeBackend backend;
  for (const auto& change : changes) {
    auto live = dependencies();
    auto state = session();
    const auto root = state.active();
    REQUIRE(state.beginStroke(live));
    const auto request = state.prepareStep(brush(), live);
    REQUIRE(request);
    change(live);
    REQUIRE(state.complete(request, backend.evaluate(*request), live) == edit::Completion::Stale);
    REQUIRE_FALSE(state.strokeActive());
    REQUIRE(state.active() == root);
  }
}
