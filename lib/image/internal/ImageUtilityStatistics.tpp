#pragma once

/**
 * @file ImageUtilityStatistics.tpp
 * @brief Template helpers for exact quantiles, online statistics, and T-digest construction.
 */

/**
 * @brief Linearly interpolate between two values.
 */
template<typename T>
double lerp(T a, T b, T t)
{
  return (1 - t) * a + t * b;
}

/**
 * @brief Convert an exact quantile into a value using sorted component data.
 * @tparam T Component value type.
 * @param[in] dataSorted Sorted component values.
 * @param[in] quantile Quantile in [0, 1].
 * @return Interpolated value at \p quantile.
 */
template<typename T>
T convertQuantileToValue(const std::span<const T> dataSorted, double quantile)
{
  const std::size_t N = dataSorted.size();

  if (0 == N) {
    spdlog::error("Sorted data has zero elements");
    throw_debug("Sorted data is empty")
  }
  else if (1 == N) {
    return dataSorted.front();
  }

  constexpr int64_t indexMin = 0;
  const int64_t indexMax = N - 1;

  // Interpolated index corresponding to quantile
  const double index = lerp<double>(-0.5, N - 0.5, quantile);

  const std::size_t indexLeft = std::max(static_cast<int64_t>(std::floor(index)), indexMin);
  const std::size_t indexRight = std::min(static_cast<int64_t>(std::ceil(index)), indexMax);

  const T dataLeft = dataSorted[indexLeft];
  const T dataRight = dataSorted[indexRight];
  return lerp<T>(dataLeft, dataRight, index - static_cast<double>(indexLeft));
}

/**
 * @brief Locate the exact quantile interval containing a value.
 * @tparam T Component value type.
 * @param[in] dataSorted Sorted component values.
 * @param[in] value Value to locate.
 * @return Quantile metadata for \p value.
 */
template<typename T>
QuantileOfValue convertValueToQuantile(const std::span<const T> dataSorted, T value)
{
  const std::size_t N = dataSorted.size();
  if (0 == N) {
    spdlog::error("Sorted data has zero elements");
    throw_debug("Sorted data is empty")
  }

  QuantileOfValue Q{};

  // Find lower and upper bounds
  auto lower = std::lower_bound(dataSorted.begin(), dataSorted.end(), value);
  auto upper = std::upper_bound(dataSorted.begin(), dataSorted.end(), value);

  // Compute indices, clamped to valid range
  std::size_t lowerIndex = std::distance(dataSorted.begin(), lower);
  std::size_t upperIndex = std::distance(dataSorted.begin(), upper);

  // Clamp indices for out-of-range values
  lowerIndex = std::min(lowerIndex, N - 1);
  upperIndex = std::min(upperIndex, N - 1);

  Q.foundValue = (value >= dataSorted.front() && value <= dataSorted.back());
  Q.lowerIndex = lowerIndex;
  Q.upperIndex = upperIndex;
  Q.lowerQuantile = static_cast<double>(lowerIndex) / N;
  Q.upperQuantile = static_cast<double>(upperIndex) / N;
  Q.lowerValue = dataSorted[lowerIndex];
  Q.upperValue = dataSorted[upperIndex];
  return Q;
}

/**
 * @brief Build an approximate T-digest from component values.
 * @tparam T Component value type.
 * @param[in] data Component values.
 * @param[in] numThreads Maximum number of worker threads.
 * @return T-digest summarizing \p data.
 */
template<typename T>
tdigest::TDigest buildTDigest(
  const std::span<const T>& data,
  unsigned int numThreads = std::thread::hardware_concurrency())
{
  using TD = tdigest::TDigest;

  const auto N = data.size();
  if (0 == N) {
    return TD{};
  }

  // Adjust compression and batchSize heuristics for performance.
  // -memory use (bytes) ≈ 30 × 15 × compression ≈ 450 × compression
  // -compression of 1000 takes about 450 kB, gives 99.9% quantile error of 1e-4,
  //  with high-fidelity tails
  const double compression = std::clamp(200.0 * std::cbrt(N / 1.0e6), 200.0, 1000.0);
  const auto batchSize = std::clamp<std::size_t>(static_cast<std::size_t>(N / (numThreads * 1000)), 10'000, 1'000'000);

  // launch one digest per thread
  std::vector<std::unique_ptr<TD>> localDigests;
  localDigests.reserve(numThreads);

  for (unsigned int i = 0; i < numThreads; ++i) {
    localDigests.emplace_back(std::make_unique<TD>(compression));
  }

  std::vector<std::thread> threads;
  const std::size_t chunkSize = (N + numThreads - 1) / numThreads;

  for (unsigned int t = 0; t < numThreads; ++t) {
    auto& td = localDigests[t];

    threads.emplace_back([&, t]() {
      const auto start = t * chunkSize;
      const auto end = std::min(start + chunkSize, N);

      // Process in batches to improve cache and reduce compress overhead
      for (std::size_t i = start; i < end; i += batchSize) {
        const auto e = std::min(i + batchSize, end);
        for (std::size_t j = i; j < e; ++j) {
          td->add(data[j]);
        }
      }

      td->compress(); // finalize local digest
    });
  }

  for (auto& thread : threads) {
    thread.join();
  }

  // merge results
  TD globalDigest(compression);
  for (auto& td : localDigests) {
    globalDigest.merge(td.get());
  }

  globalDigest.compress();
  globalDigest.cdf(0.0); // Force it to process

  return globalDigest;
}

/**
 * @brief Compute online descriptive statistics from unsorted component values.
 * @tparam T Component value type.
 * @param[in] data Component values.
 * @return Online statistics for \p data.
 */
template<typename T>
OnlineStats computeStatsOnUnsortedBuffers(const std::span<const T> data)
{
  spdlog::debug("Computing statistics on unsorted image intensities");

  OnlineStats s;
  const auto N = data.size();

  if (0 == N) {
    spdlog::error("Image contains no data on which to compute statistics");
    return s;
  }

  s.count = N;
  s.min = s.max = data[0];
  s.sum = 0.0L;
  s.mean = static_cast<long double>(data[0]);
  long double ssd = 0.0L; // sum of squares of differences from the mean

  // Welford's online algorithm
  for (std::size_t i = 0; i < N; ++i) {
    const auto x = static_cast<long double>(data[i]);

    if (x < s.min) {
      s.min = x;
    }

    if (x > s.max) {
      s.max = x;
    }

    s.sum += x;

    const auto delta = x - s.mean;
    s.mean += delta / (i + 1);
    ssd += delta * (x - s.mean);
  }

  s.variance = (N > 1) ? ssd / (N - 1) : 0.0L;
  s.stdev = std::sqrt(s.variance);
  return s;
}

/**
 * @brief Compute descriptive statistics and exact percentile table from sorted values.
 * @tparam T Component value type.
 * @param[in] dataSorted Sorted component values.
 * @return Component statistics including percentiles.
 */
template<typename T>
ComponentStats computeStatsOnSortedBuffers(const std::span<const T> dataSorted)
{
  OnlineStats os;
  os.min = static_cast<double>(dataSorted.front());
  os.max = static_cast<double>(dataSorted.back());

  const std::size_t N = dataSorted.size();
  os.sum = std::accumulate(std::begin(dataSorted), std::end(dataSorted), 0.0);
  os.mean = os.sum / N;

  std::vector<double> diff(N);
  std::transform(std::begin(dataSorted), std::end(dataSorted), std::begin(diff), [&os](double x) {
    return x - os.mean;
  });

  const double squaredSum = std::inner_product(std::begin(diff), std::end(diff), std::begin(diff), 0.0);
  os.variance = squaredSum / N;
  os.stdev = std::sqrt(os.variance);

  ComponentStats compStats;
  compStats.onlineStats = os;

  for (std::size_t i = 0; i <= 100; ++i) {
    const double quantile = static_cast<double>(i) / 100.0;
    const T value = convertQuantileToValue(dataSorted, quantile);
    compStats.quantiles[i] = static_cast<double>(value);
  }

  return compStats;
}

/**
 * @brief Compute exact first, second, and third quartiles by partial sorting.
 * @tparam T Component value type.
 * @param[in] data Component values; copied by value so it can be reordered.
 * @return Tuple containing Q1, median, and Q3.
 */
template<typename T>
std::tuple<T, T, T> compute_exact_quartiles(std::vector<T> data)
{
  const size_t n = data.size();
  if (n == 0) {
    return {T{}, T{}, T{}};
  }

  size_t q1_idx = n / 4;
  size_t q2_idx = n / 2;
  size_t q3_idx = 3 * n / 4;

  std::nth_element(data.begin(), data.begin() + q1_idx, data.end());
  T q1 = data[q1_idx];

  std::nth_element(data.begin(), data.begin() + q2_idx, data.end());
  T q2 = data[q2_idx];

  std::nth_element(data.begin(), data.begin() + q3_idx, data.end());
  T q3 = data[q3_idx];

  return {q1, q2, q3};
}
