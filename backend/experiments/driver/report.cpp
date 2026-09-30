#include "evaluation.hpp"

#include <algorithm>
#include <map>
#include <utility>

namespace yarda::evaluation
{

nlohmann::json distribution(std::vector<std::uint64_t> values)
{
  if (values.empty()) throw std::invalid_argument("no successful samples");
  std::sort(values.begin(), values.end());
  const auto quantile = [&](double fraction) {
    const auto position = (values.size() - 1) * fraction;
    const auto lower = static_cast<std::size_t>(position);
    const auto upper = std::min(lower + 1, values.size() - 1);
    return std::pair<std::uint64_t, double>{values[lower],
      static_cast<double>(values[upper] - values[lower]) * (position - lower)};
  };
  const auto lower = quantile(0.25), median = quantile(0.5), upper = quantile(0.75);
  // Subtract integer origins first so nearby large counters keep their spread.
  const auto iqr = static_cast<double>(upper.first - lower.first) +
    upper.second - lower.second;
  return {{"count", values.size()}, {"min", values.front()},
          {"max", values.back()},
          {"median", static_cast<double>(median.first) + median.second}, {"iqr", iqr}};
}

nlohmann::json summarize_samples(const nlohmann::json & index)
{
  std::map<std::pair<std::string, std::string>, std::vector<nlohmann::json>> groups;
  for (const auto & path : index)
  {
    auto sample = read_json(path.get<std::string>());
    groups[{sample.at("case_id"), sample.at("mode")}].push_back(std::move(sample));
  }
  auto result = nlohmann::json::array();
  for (const auto & [key, samples] : groups)
  {
    nlohmann::json row{{"case_id", key.first}, {"mode", key.second},
                       {"statuses", nlohmann::json::object()}};
    std::map<std::string, std::vector<std::uint64_t>> metrics;
    for (const auto & sample : samples)
    {
      const auto status = sample.at("status").get<std::string>();
      row["statuses"][status] = row["statuses"].value(status, 0U) + 1;
      if (status != "success") continue;
      for (const auto * name : {"analysis_id", "tool_version", "binary_sha256", "source_accesses",
                                "line_references", "tasks"})
      {
        if (row.contains(name) && row.at(name) != sample.at(name))
          throw std::invalid_argument("mixed analysis identities or counts in sample group");
        row[name] = sample.at(name);
      }
      for (const auto * name : {"total_time_ns", "analysis_time_ns",
                                "input_time_ns", "serialize_time_ns",
                                "peak_rss_bytes", "compaction_count",
                                "compaction_time_ns"})
        if (sample.contains(name))
          metrics[name].push_back(sample.at(name).get<std::uint64_t>());
    }
    for (auto & [name, values] : metrics) row[name] = distribution(std::move(values));
    result.push_back(std::move(row));
  }
  return result;
}

nlohmann::json statistics_json(const CsrdStatistics & s)
{
  return {{"touched_sets", s.touched_sets},
          {"active_history_entries", s.active_history_entries},
          {"fenwick_slots", s.fenwick_slots},
          {"allocated_fenwick_elements", s.allocated_fenwick_elements},
          {"hash_buckets", s.hash_buckets}, {"histogram_keys", s.histogram_keys},
          {"compaction_count", s.compaction_count},
          {"compaction_time_ns", s.compaction_time_ns},
          {"maximum_compaction_scratch_bytes", s.maximum_compaction_scratch_bytes}};
}

std::string failure_status(const std::exception & error)
{
  if (dynamic_cast<const std::bad_alloc *>(&error)) return "resource_failure";
  if (dynamic_cast<const ResolutionError *>(&error)) return "unsupported";
  const std::string message = error.what();
  for (const auto * prefix : {"emitted source accesses exceeds ",
                              "emitted line references exceeds ",
                              "loop iteration count exceeds ",
                              "cumulative loop iteration count exceeds "})
    if (message.rfind(prefix, 0) == 0) return "budget_exhaustion";
  if (dynamic_cast<const std::invalid_argument *>(&error)) return "unsupported";
  return "error";
}

} // namespace yarda::evaluation
