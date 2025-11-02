#include <vector>
#include <limits>
#include <cmath>
#include <algorithm>
#include <execution>
#include <numeric>
#include <cstdint>
#include <type_traits>
#include <iostream>
#include <span>

template <typename T>
std::vector<double> compute_quantiles_histogram_broken(
  const std::span<const T> data,
  size_t num_quantiles = 101,
  size_t num_bins = 65536)
{
  if (data.empty())
    return std::vector<double>(num_quantiles, std::numeric_limits<double>::quiet_NaN());

  const size_t N = data.size();
  const double qstep = 100.0 / (num_quantiles - 1);

  // Compute intensity range
  auto [min_it, max_it] = std::minmax_element(data.begin(), data.end());
  const double minv = static_cast<double>(*min_it);
  const double maxv = static_cast<double>(*max_it);
  const double range = std::max(maxv - minv, 1e-12); // prevent divide-by-zero

  // Decide bin count
  if constexpr (std::is_integral_v<T>) {
    // Exact binning for integer types
    num_bins = static_cast<size_t>(maxv - minv + 1);
  }

  // Build histogram
  std::vector<uint64_t> hist(num_bins, 0);

// Parallel accumulation
#pragma omp parallel
  {
    std::vector<uint64_t> local_hist(num_bins, 0);
#pragma omp for nowait
    for (size_t i = 0; i < N; ++i) {
      size_t bin = static_cast<size_t>(
        std::clamp(((static_cast<double>(data[i]) - minv) / range) * (num_bins - 1),
                   0.0, static_cast<double>(num_bins - 1)));
      ++local_hist[bin];
    }

#pragma omp critical
    for (size_t b = 0; b < num_bins; ++b)
      hist[b] += local_hist[b];
  }

  // Compute cumulative histogram
  std::vector<uint64_t> cum(hist.size());
  std::partial_sum(hist.begin(), hist.end(), cum.begin());

  // Compute quantiles
  std::vector<double> quantiles(num_quantiles);
  for (size_t qi = 0; qi < num_quantiles; ++qi) {
    double q = qi * qstep;
    double target = q * (N - 1) / 100.0;

    // Find first bin exceeding target count
    auto it = std::lower_bound(cum.begin(), cum.end(), static_cast<uint64_t>(std::ceil(target)));
    size_t bin = std::distance(cum.begin(), it);

    // Interpolate within bin
    double bin_low = minv + (bin) * (range / (num_bins - 1));
    double bin_high = minv + (bin + 1) * (range / (num_bins - 1));
    uint64_t count_before = (bin > 0) ? cum[bin - 1] : 0;
    uint64_t count_in_bin = hist[bin];
    double f = 0.0;
    if (count_in_bin > 0)
      f = (target - count_before) / static_cast<double>(count_in_bin);

    quantiles[qi] = bin_low + f * (bin_high - bin_low);
  }

  return quantiles;
}

// Compute num_quantiles quantiles (default 101 for 0..100 inclusive).
// Returns vector<double> of quantile values.
// - data: flattened image values
// - num_quantiles: number of quantiles to produce (e.g., 101 -> 0..100% inclusive).
// - num_bins_hint: suggested histogram bins when coarse-binning floating inputs
//                   (or when integer range too large); default 65536.
template <typename T>
std::vector<double> compute_quantiles_histogram(
  const std::span<const T>& data,
  size_t num_quantiles = 101,
  size_t num_bins_hint = 65536)
{
  using u64 = uint64_t;
  static_assert(std::is_arithmetic_v<T>, "T must be arithmetic");

  if (num_quantiles < 2) num_quantiles = 2;
  if (data.empty()) {
    return std::vector<double>(num_quantiles, std::numeric_limits<double>::quiet_NaN());
  }

  const u64 N = static_cast<u64>(data.size());
  const double qstep = 100.0 / static_cast<double>(num_quantiles - 1);

  // Find min/max
  auto mm = std::minmax_element(data.begin(), data.end());
  const double minv = static_cast<double>(*mm.first);
  const double maxv = static_cast<double>(*mm.second);
  const double range = std::max(maxv - minv, 0.0);

  // Decide bins: exact integer bins when feasible
  size_t num_bins = std::max<size_t>(2, num_bins_hint);
  bool exact_integer_bins = false;
  if constexpr (std::is_integral_v<T>) {
    // integer range length (may be large)
    long long vmin = static_cast<long long>(*mm.first);
    long long vmax = static_cast<long long>(*mm.second);
    unsigned long long int_range_plus1 = static_cast<unsigned long long>(vmax - vmin) + 1ULL;
    if (int_range_plus1 <= num_bins_hint) {
      num_bins = static_cast<size_t>(int_range_plus1);
      exact_integer_bins = true;
    } else {
      // use coarse bins_hint (num_bins already set)
      exact_integer_bins = false;
    }
  } else {
    exact_integer_bins = false;
  }

  // Build histogram (64-bit counters)
  std::vector<u64> hist(num_bins, 0);

  if (exact_integer_bins) {
    // one bin per integer value
    long long base = static_cast<long long>(*mm.first);
    for (u64 i = 0; i < N; ++i) {
      long long v = static_cast<long long>(data[static_cast<size_t>(i)]);
      size_t bin = static_cast<size_t>(v - base);
      ++hist[bin];
    }
  } else {
    // coarse mapping for floats or large integer ranges
    // denom divides range into num_bins equal parts: [min, min+denom), [min+denom, ...), last includes max
    double denom = (range > 0.0) ? (range / static_cast<double>(num_bins)) : 1.0;
    for (u64 i = 0; i < N; ++i) {
      double v = static_cast<double>(data[static_cast<size_t>(i)]);
      size_t bin;
      if (v == maxv) {
        bin = num_bins - 1;
      } else if (denom == 0.0) {
        bin = 0;
      } else {
        double pos = (v - minv) / denom;
        long long ib = static_cast<long long>(std::floor(pos));
        if (ib < 0) ib = 0;
        if (ib >= static_cast<long long>(num_bins)) ib = static_cast<long long>(num_bins - 1);
        bin = static_cast<size_t>(ib);
      }
      ++hist[bin];
    }
  }

  // cumulative histogram
  std::vector<u64> cum(num_bins);
  std::partial_sum(hist.begin(), hist.end(), cum.begin());

  // Prepare output vector
  std::vector<double> quantiles(num_quantiles);

  for (size_t qi = 0; qi < num_quantiles; ++qi) {
    double q = qi * qstep;
    // explicit 100% -> exact max
    if (q >= 100.0) {
      quantiles[qi] = maxv;
      continue;
    }

    // fractional sorted index
    double pos = q * (static_cast<double>(N) - 1.0) / 100.0; // in [0, N-1]
    u64 k = static_cast<u64>(std::floor(pos));                 // integer index floor
    u64 target_count = k + 1; // 1-based count needed (first element has count 1)

    // find bin containing the k-th element: first bin with cum >= target_count
    auto it = std::lower_bound(cum.begin(), cum.end(), target_count);
    size_t bin = std::distance(cum.begin(), it);
    if (bin >= num_bins) bin = num_bins - 1; // defensive

    u64 count_before = (bin == 0) ? 0ULL : cum[bin - 1];
    u64 count_in_bin = hist[bin];

    // compute bin bounds
    double bin_low, bin_high;
    if (exact_integer_bins) {
      long long base = static_cast<long long>(*mm.first);
      bin_low = static_cast<double>(base + static_cast<long long>(bin));
      // next integer (clamp to maxv)
      if (base + static_cast<long long>(bin) < static_cast<long long>(*mm.second))
        bin_high = static_cast<double>(base + static_cast<long long>(bin) + 1LL);
      else
        bin_high = maxv;
    } else {
      double denom = (range > 0.0) ? (range / static_cast<double>(num_bins)) : 0.0;
      bin_low = minv + static_cast<double>(bin) * denom;
      if (bin + 1 < num_bins)
        bin_high = bin_low + denom;
      else
        bin_high = maxv;
    }

    // if bin has members, compute fractional offset into bin
    double frac = 0.0;
    if (count_in_bin > 0) {
      double offset_in_bin = pos - static_cast<double>(count_before);
      // offset_in_bin in [0, count_in_bin) typically, but clamp defensively
      if (offset_in_bin < 0.0) offset_in_bin = 0.0;
      if (offset_in_bin > static_cast<double>(count_in_bin)) offset_in_bin = static_cast<double>(count_in_bin);
      frac = offset_in_bin / static_cast<double>(count_in_bin);
    } else {
      // extremely rare: bin empty although lower_bound returned it.
      // Find nearest non-empty bin forward, then backward.
      size_t f = bin;
      while (f < num_bins && hist[f] == 0) ++f;
      size_t b = (bin == 0) ? num_bins : bin;
      while (b > 0 && hist[b-1] == 0) --b;
      size_t chosen = (f < num_bins) ? f : ((b < num_bins) ? b : bin);
      // use chosen bin (guaranteed in-range)
      bin = chosen;
      count_before = (bin == 0) ? 0ULL : cum[bin - 1];
      count_in_bin = hist[bin];
      if (exact_integer_bins) {
        long long base = static_cast<long long>(*mm.first);
        bin_low = static_cast<double>(base + static_cast<long long>(bin));
        bin_high = (base + static_cast<long long>(bin) < static_cast<long long>(*mm.second)) ?
                     static_cast<double>(base + static_cast<long long>(bin) + 1LL) : maxv;
      } else {
        double denom = (range > 0.0) ? (range / static_cast<double>(num_bins)) : 0.0;
        bin_low = minv + static_cast<double>(bin) * denom;
        bin_high = (bin + 1 < num_bins) ? (bin_low + denom) : maxv;
      }
      double offset_in_bin = pos - static_cast<double>(count_before);
      if (offset_in_bin < 0.0) offset_in_bin = 0.0;
      if (offset_in_bin > static_cast<double>(count_in_bin)) offset_in_bin = static_cast<double>(count_in_bin);
      frac = (count_in_bin > 0) ? (offset_in_bin / static_cast<double>(count_in_bin)) : 0.0;
    }

    double qval = bin_low + frac * (bin_high - bin_low);
    // clamp to valid range
    if (qval < minv) qval = minv;
    if (qval > maxv) qval = maxv;
    quantiles[qi] = qval;
  }

  return quantiles;
}
