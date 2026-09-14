#include "evaluation.hpp"

#include "yarda/trace/task_access_stream.hpp"

namespace yarda::evaluation
{
namespace
{
struct Expected
{
  const char * object;
  std::uint64_t offset;
  std::uint64_t base;
  AccessOperation operation;
};

Expected expected_source(const nlohmann::json & row, std::uint64_t ordinal)
{
  const auto load = AccessOperation::Load, store = AccessOperation::Store;
  if (row.at("fixture") == "fixed_domain.c" ||
      row.at("fixture") == "growing_domain.c")
  {
    const auto domain = row.at("domain").get<std::uint64_t>();
    return {"global::lines", ((ordinal / 2) % domain) * 32, 0x600000,
            ordinal % 2 == 0 ? load : store};
  }
  if (row.at("fixture") != "atax_region.c")
    throw std::invalid_argument("no independent source expectation for fixture");
  const auto n = row.at("n").get<std::uint64_t>();
  if (ordinal < n) return {"global::y", ordinal * 8, 0xa00000, store};
  const auto relative = ordinal - n;
  const auto i = relative / (8 * n + 1), phase = relative % (8 * n + 1);
  if (phase == 0) return {"global::tmp", i * 8, 0xc00000, store};
  const auto step = phase - 1;
  const auto j = (step % (4 * n)) / 4, lane = step % 4;
  if (lane == 0) return {"global::A", (i * n + j) * 8, 0x600000, load};
  if (step < 4 * n)
  {
    if (lane == 1) return {"global::x", j * 8, 0x800000, load};
    return {"global::tmp", i * 8, 0xc00000, lane == 2 ? load : store};
  }
  if (lane == 1) return {"global::tmp", i * 8, 0xc00000, load};
  return {"global::y", j * 8, 0xa00000, lane == 2 ? load : store};
}
} // namespace

void verify_sources(const Inputs & inputs, const nlohmann::json & row)
{
  std::uint64_t seen = 0, tasks = 0;
  const auto expected_count = row.at("expected_sources").get<std::uint64_t>();
  const TaskAccessSink sink{
    [&](const std::string &, std::uint64_t excluded) {
      if (++tasks != 1 || excluded != 0)
        throw std::logic_error("unexpected task or excluded call in fixture");
    },
    [&](const std::string &, const ResolvedAccess & actual) {
      if (seen >= expected_count) throw std::logic_error("extra fixture source");
      const auto expected = expected_source(row, seen);
      if (actual.source_access_ordinal != seen || actual.object_id != expected.object ||
          actual.object_byte_offset != expected.offset || actual.access_size != 8 ||
          actual.linked_byte_address != expected.base + expected.offset ||
          actual.address_basis != AddressBasis::Absolute ||
          actual.operation != expected.operation)
        throw std::logic_error("independent fixture source mismatch at " +
                                std::to_string(seen));
      ++seen;
    },
    [](const std::string &, const TraceCoverage &) {}};
  const auto options = work_options(row);
  TraceEmissionBudget budget(options.emission_limits);
  stream_resolved_task_accesses(inputs.raw, inputs.objects, sink, budget,
                                options.loop_limits);
  if (seen != expected_count || tasks != 1)
    throw std::logic_error("incomplete fixture source sequence");
}

} // namespace yarda::evaluation
