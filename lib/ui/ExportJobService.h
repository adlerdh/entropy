#pragma once

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <memory>
#include <optional>
#include <stop_token>
#include <string>
#include <vector>

namespace ui::export_jobs
{
/// Outcome of an export job.
enum class Outcome : std::uint8_t
{
  Running,
  Succeeded,
  Failed,
  Cancelled
};

/// Result returned by a background export task.
struct Result
{
  Outcome outcome = Outcome::Succeeded;               //!< Final job outcome
  std::string message;                                //!< Optional user-facing detail
  std::vector<std::filesystem::path> outputFileNames; //!< Successfully committed outputs

  /// Construct a successful export result.
  static Result success(std::vector<std::filesystem::path> outputs, std::string message = {});

  /// Construct a failed export result.
  static Result failure(std::string message);

  /// Construct a cancelled export result.
  static Result cancelled(std::string message = "Export cancelled.");
};

class JobContext;
using Task = std::function<Result(JobContext&)>;

/// Description and executable work for one export request.
struct Request
{
  std::string description;           //!< Short name displayed in the progress window
  std::filesystem::path destination; //!< Primary destination displayed to the user
  Task task;                         //!< Worker-thread operation
};

/// Thread-safe snapshot used to render export status.
struct Snapshot
{
  Outcome outcome = Outcome::Succeeded;
  bool hasJob = false;
  bool compact = false;
  bool cancellationRequested = false;
  std::string description;
  std::filesystem::path destination;
  std::string phase;
  std::optional<float> progress;
  bool indeterminate = false;
  std::string message;
  std::vector<std::filesystem::path> outputFileNames;
};

namespace detail
{
struct SharedState;
}

/// Worker-side interface for progress reporting and cooperative cancellation.
class JobContext
{
public:
  /// Report the current stage and optional normalized completion fraction.
  void update(std::string phase, std::optional<float> progress = std::nullopt);

  /// Return true after the user or service has requested cancellation.
  [[nodiscard]] bool cancellationRequested() const;

private:
  friend class Service;
  JobContext(std::shared_ptr<detail::SharedState> state, std::stop_token stopToken);

  std::shared_ptr<detail::SharedState> m_state;
  std::stop_token m_stopToken;
};

/// Owns one background export worker and its observable status.
class Service
{
public:
  Service();
  ~Service();

  Service(const Service&) = delete;
  Service& operator=(const Service&) = delete;

  /// Start a job. Returns false if another export is still running or the request is invalid.
  [[nodiscard]] bool submit(Request request);

  /// Request cooperative cancellation of the active export.
  void requestCancel();

  /// Toggle between the full progress window and a compact activity indicator.
  void setCompact(bool compact);

  /// Remove a completed job from the status window.
  void dismiss();

  /// Return a coherent snapshot of the current export state.
  [[nodiscard]] Snapshot snapshot() const;

  /// Wait for an active task to finish. Primarily useful to deterministic clients and tests.
  [[nodiscard]] bool waitForFinished(std::chrono::milliseconds timeout) const;

private:
  std::shared_ptr<detail::SharedState> m_state;
  class Worker;
  std::unique_ptr<Worker> m_worker;
};

/// RAII temporary output that replaces its final destination only when committed.
class StagedOutput
{
public:
  explicit StagedOutput(std::filesystem::path destination);
  ~StagedOutput();

  StagedOutput(StagedOutput&& other) noexcept;
  StagedOutput& operator=(StagedOutput&& other) noexcept;
  StagedOutput(const StagedOutput&) = delete;
  StagedOutput& operator=(const StagedOutput&) = delete;

  /// Temporary path to pass to a codec. It retains the final filename extension.
  [[nodiscard]] const std::filesystem::path& temporaryPath() const;

  /// Intended final output path.
  [[nodiscard]] const std::filesystem::path& destination() const;

  /// Atomically replace the destination with the completed temporary output where supported.
  [[nodiscard]] std::optional<std::string> commit();

private:
  void discard() noexcept;

  std::filesystem::path m_destination;
  std::filesystem::path m_temporaryDirectory;
  std::filesystem::path m_temporaryPath;
  bool m_committed = false;
};
} // namespace ui::export_jobs
