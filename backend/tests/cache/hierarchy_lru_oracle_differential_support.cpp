#include "hierarchy_lru_oracle_differential_support.hpp"

#include <cassert>
#include <random>
#include <sstream>
#include <stdexcept>

#include "hierarchy_lru_oracle_test_support.hpp"

namespace yarda
{
namespace test
{
namespace support
{
namespace
{

void append(MappedTaskTrace & trace, std::uint64_t block,
            const CacheGeometry & geometry, std::uint64_t line_offset,
            AccessOperation operation)
{
  trace.accesses.push_back(make_mapping(
    block, geometry, static_cast<std::uint64_t>(trace.accesses.size()),
    line_offset, operation));
}

void append_witness_prefix(MappedTaskTrace & trace,
                           const CacheGeometry & geometry)
{
  const auto set_count = cache_set_count(geometry);
  const auto quarter = geometry.line_size / 4;
  const auto half = geometry.line_size / 2;
  const auto last = geometry.line_size - 1;

  append(trace, 0, geometry, 0, AccessOperation::Store);
  append(trace, set_count, geometry, 0, AccessOperation::Load);
  append(trace, set_count, geometry, half, AccessOperation::Store);
  append(trace, 0, geometry, quarter, AccessOperation::Load);
  append(trace, 2U * set_count, geometry, 0, AccessOperation::Load);
  append(trace, 3U * set_count, geometry, half, AccessOperation::Store);
  append(trace, 0, geometry, last, AccessOperation::Store);

  append(trace, 4U * set_count, geometry, 0, AccessOperation::Store);
  append(trace, 5U * set_count, geometry, quarter, AccessOperation::Store);
  append(trace, 4U * set_count, geometry, half, AccessOperation::Store);
  append(trace, 6U * set_count, geometry, last, AccessOperation::Store);
  append(trace, 4U * set_count, geometry, quarter, AccessOperation::Store);
  append(trace, 5U * set_count, geometry, half, AccessOperation::Store);
}

char operation_code(AccessOperation operation)
{
  switch (operation)
  {
    case AccessOperation::Unknown:
      return 'U';
    case AccessOperation::Load:
      return 'L';
    case AccessOperation::Store:
      return 'S';
  }
  return '?';
}

void append_geometry(std::ostringstream & output, const char * name,
                     const CacheGeometry & geometry)
{
  output << ' ' << name << "={line=" << geometry.line_size
         << ",lines=" << geometry.line_count
         << ",ways=" << geometry.associativity << '}';
}

}  // namespace

MappedTaskTrace make_seeded_oracle_trace(const SeededOracleTraceSpec & spec)
{
  if (spec.block_domain == 0)
  {
    throw std::invalid_argument(
      "seeded oracle trace requires a positive block domain");
  }
  assert(spec.reference_count >= 13);
  assert(spec.l1_geometry.line_size == spec.llc_geometry.line_size);

  MappedTaskTrace trace;
  trace.task_id = "seed-" + std::to_string(spec.seed);
  trace.accesses.reserve(spec.reference_count);
  append_witness_prefix(trace, spec.l1_geometry);

  std::mt19937 random(spec.seed);
  while (trace.accesses.size() < spec.reference_count)
  {
    const auto block = static_cast<std::uint64_t>(random()) % spec.block_domain;
    const auto offset =
      static_cast<std::uint64_t>(random()) % spec.l1_geometry.line_size;
    const auto operation =
      random() % 2 == 0 ? AccessOperation::Load : AccessOperation::Store;
    append(trace, block, spec.l1_geometry, offset, operation);
  }
  return trace;
}

std::string describe_seeded_oracle_trace(const SeededOracleTraceSpec & spec,
                                         const MappedTaskTrace & trace)
{
  std::ostringstream output;
  output << "seed=" << spec.seed << " length=" << trace.accesses.size()
         << " block_domain=" << spec.block_domain;
  append_geometry(output, "l1", spec.l1_geometry);
  append_geometry(output, "llc", spec.llc_geometry);
  output << " trace=";
  for (const auto & mapping : trace.accesses)
  {
    output << ' ' << mapping.decoded.block_number << '@'
           << mapping.decoded.line_offset << ':'
           << operation_code(mapping.operation);
  }
  return output.str();
}

}  // namespace support
}  // namespace test
}  // namespace yarda
