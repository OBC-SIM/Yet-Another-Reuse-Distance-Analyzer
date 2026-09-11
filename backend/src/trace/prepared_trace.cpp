#include "prepared_trace.hpp"

#include <stdexcept>
#include <utility>

#include "loop_iteration.hpp"
#include "prepared_trace_node.hpp"

namespace yarda::detail
{
namespace
{

using Json = nlohmann::json;

void prepare_node(PreparedNode & node, const LoopScope * scope,
                  std::vector<std::int64_t> & values)
{
  if (!std::holds_alternative<std::monostate>(node.payload)) return;
  const auto & raw = *node.source;
  const auto type = raw.value("type", "");
  if (type == "Scalar" || type == "Array")
  {
    PreparedAccess access;
    if (type == "Array")
    {
      const auto indices = raw.find("indices");
      if (indices != raw.end())
      {
        access.indices.reserve(indices->size());
        for (const auto & index : *indices)
          access.indices.emplace_back(index.get<std::string>(), scope);
      }
    }
    node.payload = std::move(access);
    return;
  }
  if (type == "Loop")
  {
    PreparedLoop loop;
    loop.variable = raw.at("var").get<std::string>();
    loop.start = raw.value("start", 0LL);
    const auto bound = raw.at("bound").get<std::int64_t>();
    loop.step = raw.value("step", 1LL);
    loop.count = loop_iteration_count(loop.start, bound, loop.step);
    loop.slot = values.size();
    values.push_back(0);
    node.payload = std::move(loop);
    return;
  }
  throw std::invalid_argument("Unknown LAT node type: " + type);
}

void prepare_body(PreparedLoop & loop, const Json & raw)
{
  if (loop.body_prepared) return;
  const auto body = raw.find("body");
  if (body != raw.end())
  {
    loop.body.reserve(body->size());
    for (const auto & child : *body) loop.body.emplace_back(child);
  }
  loop.body_prepared = true;
}

void execute_node(PreparedNode & node, const LoopScope * scope,
                  std::vector<std::int64_t> & values, ExpansionBudget & budget,
                  const PreparedNodeSink & sink)
{
  prepare_node(node, scope, values);
  if (auto * access = std::get_if<PreparedAccess>(&node.payload))
  {
    sink(*node.source, *access, values);
    return;
  }

  auto & loop = std::get<PreparedLoop>(node.payload);
  budget.consume_loop_iterations(loop.count);
  if (loop.count == 0) return;
  prepare_body(loop, *node.source);
  const LoopScope child_scope{loop.variable, loop.slot, scope};
  auto value = loop.start;
  for (std::uint64_t iteration = 0; iteration < loop.count; ++iteration)
  {
    // Descendant preparation can grow values; never keep a slot reference.
    values[loop.slot] = value;
    for (auto & child : loop.body)
      execute_node(child, &child_scope, values, budget, sink);
    if (iteration + 1 < loop.count &&
        __builtin_add_overflow(value, loop.step, &value))
      throw std::invalid_argument("loop iteration value overflows");
  }
}

}  // namespace

void visit_prepared_trace(const nlohmann::json & node, ExpansionBudget & budget,
                          const PreparedAccessSink & sink)
{
  visit_prepared_accesses(
    node, budget,
    [&](const Json & source, PreparedAccess & access,
        const std::vector<std::int64_t> & slots) {
      access.values.resize(access.indices.size());
      for (std::size_t i = 0; i < access.indices.size(); ++i)
        access.values[i] = access.indices[i].evaluate(slots);
      sink(source, access.values);
    });
}

void visit_prepared_accesses(const nlohmann::json & node,
                             ExpansionBudget & budget,
                             const PreparedNodeSink & sink)
{
  PreparedNode root(node);
  std::vector<std::int64_t> values;
  execute_node(root, nullptr, values, budget, sink);
}

}  // namespace yarda::detail
