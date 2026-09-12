#include "ui/ExportJobService.h"

#include <algorithm>
#include <atomic>
#include <condition_variable>
#include <format>
#include <mutex>
#include <system_error>
#include <thread>
#include <utility>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

namespace ui::export_jobs
{
namespace detail
{
struct SharedState
{
  mutable std::mutex mutex;
  mutable std::condition_variable finished;
  Snapshot snapshot;
};
} // namespace detail

namespace
{
std::atomic<std::uint64_t> s_temporarySequence{0};

bool running(const detail::SharedState& state)
{
  return state.snapshot.hasJob && Outcome::Running == state.snapshot.outcome;
}

std::filesystem::path makeTemporaryDirectory(const std::filesystem::path& destination)
{
  const std::filesystem::path directory = destination.parent_path().empty() ? "." : destination.parent_path();
  for (unsigned int attempt = 0; attempt < 100u; ++attempt) {
    const std::uint64_t sequence = s_temporarySequence.fetch_add(1, std::memory_order_relaxed);
    const auto candidate = directory / std::format(".entropy-export-{}", sequence);
    std::error_code error;
    if (std::filesystem::create_directory(candidate, error)) {
      return candidate;
    }
    if (error && error != std::errc::file_exists) {
      throw std::filesystem::filesystem_error("Could not create a temporary export directory", candidate, error);
    }
  }
  throw std::filesystem::filesystem_error(
    "Could not allocate a temporary export directory",
    destination,
    std::make_error_code(std::errc::file_exists));
}

std::optional<std::string> replaceDestination(
  const std::filesystem::path& temporaryPath,
  const std::filesystem::path& destination)
{
#ifdef _WIN32
  if (
    MoveFileExW(temporaryPath.c_str(), destination.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) !=
    FALSE)
  {
    return std::nullopt;
  }
  return std::system_category().message(static_cast<int>(GetLastError()));
#else
  std::error_code error;
  std::filesystem::rename(temporaryPath, destination, error);
  if (!error) {
    return std::nullopt;
  }
  return error.message();
#endif
}
} // namespace

Result Result::success(std::vector<std::filesystem::path> outputs, std::string message)
{
  return {.outcome = Outcome::Succeeded, .message = std::move(message), .outputFileNames = std::move(outputs)};
}

Result Result::failure(std::string message)
{
  return {.outcome = Outcome::Failed, .message = std::move(message), .outputFileNames = {}};
}

Result Result::cancelled(std::string message)
{
  return {.outcome = Outcome::Cancelled, .message = std::move(message), .outputFileNames = {}};
}

JobContext::JobContext(std::shared_ptr<detail::SharedState> state, const std::stop_token stopToken)
  : m_state{std::move(state)}, m_stopToken{stopToken}
{
}

void JobContext::update(std::string phase, const std::optional<float> progress)
{
  std::scoped_lock lock(m_state->mutex);
  m_state->snapshot.phase = std::move(phase);
  m_state->snapshot.indeterminate = !progress.has_value();
  if (progress) {
    m_state->snapshot.progress = std::clamp(*progress, 0.0f, 1.0f);
  }
}

bool JobContext::cancellationRequested() const
{
  if (m_stopToken.stop_requested()) {
    return true;
  }
  std::scoped_lock lock(m_state->mutex);
  return m_state->snapshot.cancellationRequested;
}

class Service::Worker
{
public:
  std::mutex mutex;
  std::jthread thread;
};

Service::Service() : m_state{std::make_shared<detail::SharedState>()}, m_worker{std::make_unique<Worker>()} {}

Service::~Service()
{
  std::scoped_lock workerLock(m_worker->mutex);
  {
    std::scoped_lock stateLock(m_state->mutex);
    if (running(*m_state)) {
      m_state->snapshot.cancellationRequested = true;
    }
  }
  if (m_worker->thread.joinable()) {
    m_worker->thread.request_stop();
    m_worker->thread.join();
  }
}

bool Service::submit(Request request)
{
  if (!request.task || request.destination.empty()) {
    return false;
  }

  std::scoped_lock workerLock(m_worker->mutex);
  {
    std::scoped_lock lock(m_state->mutex);
    if (running(*m_state)) {
      return false;
    }
  }
  if (m_worker->thread.joinable()) {
    m_worker->thread.join();
  }

  {
    std::scoped_lock lock(m_state->mutex);
    m_state->snapshot = {
      .outcome = Outcome::Running,
      .hasJob = true,
      .compact = false,
      .cancellationRequested = false,
      .description = std::move(request.description),
      .destination = std::move(request.destination),
      .phase = "Preparing export",
      .progress = 0.0f,
      .indeterminate = false,
      .message = {},
      .outputFileNames = {}};
  }

  Task task = std::move(request.task);
  const auto state = m_state;
  m_worker->thread = std::jthread([state, task = std::move(task)](const std::stop_token stopToken) mutable {
    JobContext context{state, stopToken};
    Result result;
    try {
      result = task(context);
      if (context.cancellationRequested() && Outcome::Succeeded == result.outcome) {
        result = Result::cancelled();
      }
      if (Outcome::Running == result.outcome) {
        result = Result::failure("The export task returned an invalid running result.");
      }
    }
    catch (const std::exception& error) {
      result = Result::failure(error.what());
    }
    catch (...) {
      result = Result::failure("An unknown error occurred during export.");
    }

    {
      std::scoped_lock lock(state->mutex);
      state->snapshot.outcome = result.outcome;
      state->snapshot.progress = Outcome::Succeeded == result.outcome ? std::optional{1.0f} : std::nullopt;
      state->snapshot.indeterminate = false;
      state->snapshot.phase.clear();
      state->snapshot.message = std::move(result.message);
      state->snapshot.outputFileNames = std::move(result.outputFileNames);
    }
    state->finished.notify_all();
  });
  return true;
}

void Service::requestCancel()
{
  std::scoped_lock workerLock(m_worker->mutex);
  {
    std::scoped_lock lock(m_state->mutex);
    if (!running(*m_state)) {
      return;
    }
    m_state->snapshot.cancellationRequested = true;
  }
  if (m_worker->thread.joinable()) {
    m_worker->thread.request_stop();
  }
}

void Service::setCompact(const bool compact)
{
  std::scoped_lock lock(m_state->mutex);
  m_state->snapshot.compact = compact;
}

void Service::dismiss()
{
  std::scoped_lock lock(m_state->mutex);
  if (!running(*m_state)) {
    m_state->snapshot = {};
  }
}

Snapshot Service::snapshot() const
{
  std::scoped_lock lock(m_state->mutex);
  return m_state->snapshot;
}

bool Service::waitForFinished(const std::chrono::milliseconds timeout) const
{
  std::unique_lock lock(m_state->mutex);
  return m_state->finished.wait_for(lock, timeout, [this]() { return !running(*m_state); });
}

StagedOutput::StagedOutput(std::filesystem::path destination)
  : m_destination{std::move(destination)}
  , m_temporaryDirectory{makeTemporaryDirectory(m_destination)}
  , m_temporaryPath{m_temporaryDirectory / m_destination.filename()}
{
}

StagedOutput::~StagedOutput()
{
  discard();
}

StagedOutput::StagedOutput(StagedOutput&& other) noexcept
  : m_destination{std::move(other.m_destination)}
  , m_temporaryDirectory{std::move(other.m_temporaryDirectory)}
  , m_temporaryPath{std::move(other.m_temporaryPath)}
  , m_committed{other.m_committed}
{
  other.m_committed = true;
}

StagedOutput& StagedOutput::operator=(StagedOutput&& other) noexcept
{
  if (this != &other) {
    discard();
    m_destination = std::move(other.m_destination);
    m_temporaryDirectory = std::move(other.m_temporaryDirectory);
    m_temporaryPath = std::move(other.m_temporaryPath);
    m_committed = other.m_committed;
    other.m_committed = true;
  }
  return *this;
}

const std::filesystem::path& StagedOutput::temporaryPath() const
{
  return m_temporaryPath;
}

const std::filesystem::path& StagedOutput::destination() const
{
  return m_destination;
}

std::optional<std::string> StagedOutput::commit()
{
  if (m_committed) {
    return std::nullopt;
  }
  std::error_code iterationError;
  std::vector<std::filesystem::path> stagedFiles;
  for (std::filesystem::directory_iterator it{m_temporaryDirectory, iterationError}, end; !iterationError && it != end;
       it.increment(iterationError))
  {
    if (it->is_regular_file()) {
      stagedFiles.push_back(it->path());
    }
  }
  if (iterationError) {
    return "Could not inspect temporary export files: " + iterationError.message();
  }
  if (stagedFiles.empty() || !std::filesystem::exists(m_temporaryPath)) {
    return "The export codec did not create the requested output file.";
  }

  // Publish sidecars first and the requested file last. A reader can therefore never observe a new
  // header that still refers to files which have not been installed yet.
  std::ranges::stable_sort(stagedFiles, [this](const auto& left, const auto& right) {
    return left == m_temporaryPath ? false : right == m_temporaryPath;
  });

  struct CommitEntry
  {
    std::filesystem::path staged;
    std::filesystem::path destination;
    std::filesystem::path backup;
    bool hadOriginal = false;
    bool originalBackedUp = false;
    bool published = false;
  };
  const std::filesystem::path finalDirectory = m_destination.parent_path().empty() ? "." : m_destination.parent_path();
  std::vector<CommitEntry> entries;
  entries.reserve(stagedFiles.size());
  for (std::size_t index = 0; index < stagedFiles.size(); ++index) {
    const auto& stagedFile = stagedFiles[index];
    entries.push_back(
      {.staged = stagedFile,
       .destination = finalDirectory / stagedFile.filename(),
       .backup = m_temporaryDirectory / std::format(".backup-{}-{}", index, stagedFile.filename().string())});
  }

  const auto rollback = [&entries]() {
    std::string rollbackError;
    for (auto it = entries.rbegin(); it != entries.rend(); ++it) {
      std::error_code ignored;
      if (it->published) {
        std::filesystem::remove(it->destination, ignored);
      }
      if (it->originalBackedUp) {
        if (const auto error = replaceDestination(it->backup, it->destination); error && rollbackError.empty()) {
          rollbackError = *error;
        }
      }
    }
    return rollbackError;
  };

  for (auto& entry : entries) {
    std::error_code existsError;
    entry.hadOriginal = std::filesystem::exists(entry.destination, existsError);
    if (existsError) {
      const std::string rollbackError = rollback();
      return "Could not inspect '" + entry.destination.string() + "': " + existsError.message() +
             (rollbackError.empty() ? "" : ". Rollback also failed: " + rollbackError);
    }
    if (entry.hadOriginal) {
      if (const auto error = replaceDestination(entry.destination, entry.backup)) {
        const std::string rollbackError = rollback();
        return "Could not preserve the existing file '" + entry.destination.string() + "': " + *error +
               (rollbackError.empty() ? "" : ". Rollback also failed: " + rollbackError);
      }
      entry.originalBackedUp = true;
    }
    if (const auto error = replaceDestination(entry.staged, entry.destination)) {
      const std::string rollbackError = rollback();
      return "Could not commit '" + entry.destination.string() + "': " + *error +
             (rollbackError.empty() ? "" : ". Rollback also failed: " + rollbackError);
    }
    entry.published = true;
  }
  m_committed = true;
  std::error_code ignored;
  std::filesystem::remove_all(m_temporaryDirectory, ignored);
  return std::nullopt;
}

void StagedOutput::discard() noexcept
{
  if (m_committed || m_temporaryDirectory.empty()) {
    return;
  }
  std::error_code ignored;
  std::filesystem::remove_all(m_temporaryDirectory, ignored);
}
} // namespace ui::export_jobs
