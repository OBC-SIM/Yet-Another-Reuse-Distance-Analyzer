#include "expectations.hpp"

#include <stdexcept>

namespace yarda::rtems_experiment
{
ResolvedTaskTraceResult expected_sources(const nlohmann::json & row)
{
  ResolvedTaskTrace task;
  task.task_id = "region:16:benchmark_kernel:APE_ANALYZE";
  task.accesses.reserve(row.at("expected_sources").get<std::size_t>());
  const auto emit = [&](const char * name, std::uint64_t index, bool store = false) {
    const auto & symbol = row.at("symbols").at(name);
    const auto offset = index * 8;
    if (offset + 8 > symbol.at("size").get<std::uint64_t>())
      throw std::logic_error("expected access exceeds GNU nm object size");
    task.accesses.push_back({std::string("global::") + name, offset, 8,
      symbol.at("base").get<std::uint64_t>() + offset, AddressBasis::Absolute,
      store ? AccessOperation::Store : AccessOperation::Load, task.accesses.size()});
  };
  const auto kernel = row.at("kernel").get<std::string>();
  const auto m = row.value("m", 0U), n = row.value("n", 0U);
  if (kernel == "atax")
  {
    for (unsigned j = 0; j < n; ++j) emit("y", j, true);
    for (unsigned i = 0; i < m; ++i)
    {
      emit("tmp", i, true);
      for (unsigned j = 0; j < n; ++j)
      {
        emit("tmp", i); emit("A", i * n + j); emit("x", j); emit("tmp", i, true);
      }
      for (unsigned j = 0; j < n; ++j)
      {
        emit("y", j); emit("A", i * n + j); emit("tmp", i); emit("y", j, true);
      }
    }
  }
  else if (kernel == "bicg")
  {
    for (unsigned j = 0; j < m; ++j) emit("s", j, true);
    for (unsigned i = 0; i < n; ++i)
    {
      emit("q", i, true);
      for (unsigned j = 0; j < m; ++j)
      {
        emit("s", j); emit("r", i); emit("A", i * m + j); emit("s", j, true);
        emit("q", i); emit("A", i * m + j); emit("p", j); emit("q", i, true);
      }
    }
  }
  else if (kernel == "mvt")
  {
    for (unsigned i = 0; i < n; ++i)
      for (unsigned j = 0; j < n; ++j)
      {
        emit("x1", i); emit("A", i * n + j); emit("y1", j); emit("x1", i, true);
      }
    for (unsigned i = 0; i < n; ++i)
      for (unsigned j = 0; j < n; ++j)
      {
        emit("x2", i); emit("A", j * n + i); emit("y2", j); emit("x2", i, true);
      }
  }
  else if (kernel == "sweep")
  {
    for (unsigned repeat = 0; repeat < row.at("repeats").get<unsigned>(); ++repeat)
      for (unsigned i = 0; i < row.at("domain").get<unsigned>(); ++i)
      {
        emit("lines", i * 4); emit("lines", i * 4, true);
      }
  }
  else throw std::invalid_argument("unknown RTEMS kernel");
  if (task.accesses.size() != row.at("expected_sources").get<std::size_t>())
    throw std::logic_error("independent source count does not match manifest");
  task.coverage.source_accesses = task.accesses.size();
  task.coverage.resolved_accesses = task.accesses.size();
  ResolvedTaskTraceResult result;
  result.coverage = task.coverage;
  result.tasks.push_back(std::move(task));
  return result;
}
}
