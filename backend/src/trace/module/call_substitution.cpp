#include "call_substitution.hpp"

#include <regex>
#include <stdexcept>
#include <string>

#include "object_metadata.hpp"
#include "trace/index/affine_expression.hpp"
#include "trace/layout/access_layout.hpp"

namespace yarda::detail
{
namespace
{

using Json = nlohmann::json;

const std::regex kIdentifier(R"(\b[A-Za-z_][A-Za-z0-9_]*\b)");

std::string substitute_name(const std::string & name,
                            const CallMapping & mapping)
{
  if (const auto exact = mapping.find(name); exact != mapping.end())
  {
    return exact->second;
  }
  const auto expression = parse_affine_expression(name);
  if (expression)
  {
    AffineExpression result;
    result.constant = expression->constant;
    bool changed = false;
    for (const auto & [variable, coefficient] : expression->terms)
    {
      const auto actual = mapping.find(variable);
      changed |= actual != mapping.end();
      const auto replacement = actual == mapping.end()
                                 ? std::optional<AffineExpression>(
                                     AffineExpression{0, {{variable, 1}}})
                                 : parse_affine_expression(actual->second);
      // Preserve a deferred failure without leaving a capturable formal name.
      if (!replacement ||
          !add_affine_expression(result, *replacement, coefficient))
        return "(" + name + ")";
    }
    if (changed) return format_affine_expression(result);
  }
  return name;
}

std::string substitute_tokens(const std::string & text,
                              const CallMapping & mapping)
{
  std::string output;
  std::sregex_iterator current(text.begin(), text.end(), kIdentifier);
  const std::sregex_iterator end;
  std::size_t copied = 0;
  for (; current != end; ++current)
  {
    output.append(text, copied,
                  static_cast<std::size_t>(current->position()) - copied);
    const auto token = current->str();
    const auto replacement = mapping.find(token);
    output += replacement == mapping.end() ? token : replacement->second;
    copied = static_cast<std::size_t>(current->position() + current->length());
  }
  output.append(text, copied, std::string::npos);
  return output;
}

std::string substitute_text(const std::string & text,
                            const CallMapping & mapping)
{
  std::string output;
  std::size_t position = 0;
  while (position < text.size())
  {
    const auto begin = text.find('[', position);
    const auto end = begin == std::string::npos ? std::string::npos
                                                : text.find(']', begin + 1);
    if (end == std::string::npos) break;
    output +=
      substitute_tokens(text.substr(position, begin - position), mapping);
    output +=
      "[" + substitute_name(text.substr(begin + 1, end - begin - 1), mapping) +
      "]";
    position = end + 1;
  }
  return output + substitute_tokens(text.substr(position), mapping);
}

void validate_row_dimensions(const Json & formal, const Json & actual,
                             std::size_t count)
{
  if (count == 0) return;
  if (!formal.is_array() || !actual.is_array() || formal.size() < count ||
      actual.size() < count)
  {
    throw std::invalid_argument("inline object binding lacks row dimensions");
  }
  for (std::size_t offset = 1; offset <= count; ++offset)
  {
    if (formal[formal.size() - offset] != actual[actual.size() - offset])
    {
      throw std::invalid_argument(
        "inline object binding has incompatible row dimensions");
    }
  }
}

void validate_array_binding(const Json & node, const Json & actual,
                            const Json & objects)
{
  const auto actual_shape = actual.value("shape", Json::array());
  if (has_field_path(node))
  {
    const auto formal_id = node.value("object", "");
    if (!objects.contains(formal_id)) return;
    const auto & formal = objects.at(formal_id);
    for (const auto * key : {"elem_type", "elem_size"})
    {
      if (formal.contains(key) && actual.contains(key) &&
          formal.at(key) != actual.at(key))
        throw std::invalid_argument(
          "inline field binding has incompatible root layout");
    }
    const auto formal_shape = formal.value("shape", Json::array());
    std::size_t root_indices = 0;
    for (const auto & segment : node.at("access_path"))
    {
      if (segment.value("kind", "") != "index") break;
      ++root_indices;
    }
    if (root_indices == 0)
    {
      if (formal_shape != actual_shape)
        throw std::invalid_argument(
          "inline field binding has incompatible root shape");
    }
    else
      validate_row_dimensions(formal_shape, actual_shape, root_indices - 1);
    return;
  }
  if (node.contains("elem_size") && actual.contains("elem_size") &&
      node.at("elem_size") != actual.at("elem_size"))
    throw std::invalid_argument(
      "inline object binding has incompatible element size");
  const auto indices = node.value("indices", Json::array());
  // Missing shape on a known scalar pointer means rank zero. Only legacy
  // nodes without formal object metadata may infer their layout from actuals.
  if (node.contains("shape") || objects.contains(node.value("object", "")))
    validate_row_dimensions(node.value("shape", Json::array()), actual_shape,
                            indices.empty() ? 0 : indices.size() - 1);
}

}  // namespace

nlohmann::json substitute_call_node(nlohmann::json node,
                                    const CallMapping & names,
                                    const CallMapping & object_ids,
                                    const nlohmann::json & objects)
{
  const auto type = node.value("type", "");
  if (type == "Array" || type == "Scalar")
  {
    if (const auto object = object_ids.find(node.value("object", ""));
        object != object_ids.end())
    {
      if (objects.contains(object->second))
      {
        const auto & metadata = objects.at(object->second);
        if (type == "Array")
        {
          try
          {
            validate_array_binding(node, metadata, objects);
          }
          catch (const std::invalid_argument & error)
          {
            throw std::invalid_argument(
              std::string(error.what()) + " (formal object '" + object->first +
              "', actual object '" + object->second + "')");
          }
        }
        apply_object_metadata(node, metadata, true);
      }
      node["object"] = object->second;
    }
  }
  if (type == "Array")
  {
    node["name"] = substitute_text(node.value("name", ""), names);
    for (auto & index : node["indices"])
    {
      index = substitute_name(index.get<std::string>(), names);
    }
    if (node.contains("access_path") && node["access_path"].is_array())
    {
      for (auto & segment : node["access_path"])
      {
        if (segment.value("kind", "") == "index" && segment.contains("value"))
        {
          segment["value"] =
            substitute_name(segment["value"].get<std::string>(), names);
        }
      }
    }
  }
  else if (type == "Scalar")
  {
    node["name"] = substitute_name(node.value("name", ""), names);
  }
  else if (type == "Loop")
  {
    for (auto & child : node["body"])
    {
      child = substitute_call_node(child, names, object_ids, objects);
    }
  }
  else if (type == "Call")
  {
    for (auto & argument : node["args"])
    {
      argument = substitute_name(argument.get<std::string>(), names);
    }
    if (node.contains("arg_objects") && node["arg_objects"].is_array())
    {
      for (auto & object : node["arg_objects"])
      {
        if (const auto actual = object_ids.find(object.get<std::string>());
            actual != object_ids.end())
        {
          object = actual->second;
        }
      }
    }
  }
  return node;
}

}  // namespace yarda::detail
