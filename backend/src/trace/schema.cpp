#include "yarda/trace/schema.hpp"

#include <stdexcept>

namespace yarda
{
namespace
{

using Json = nlohmann::json;

Json enrich_node(Json node, const Json & objects)
{
  const auto type = node.value("type", "");
  if (type == "Array" || type == "Scalar")
  {
    const auto object_id = node.value("object", "");
    if (!object_id.empty() && objects.contains(object_id))
    {
      const auto & metadata = objects.at(object_id);
      if (type == "Array" && !node.contains("shape") &&
          metadata.contains("shape"))
      {
        node["shape"] = metadata["shape"];
      }
      if (!node.contains("elem_size") && metadata.contains("elem_size"))
      {
        node["elem_size"] = metadata["elem_size"];
      }
    }
  }
  else if (type == "Loop")
  {
    Json body = Json::array();
    for (const auto & child : node.value("body", Json::array()))
    {
      body.push_back(enrich_node(child, objects));
    }
    node["body"] = std::move(body);
  }
  return node;
}

}  // namespace

nlohmann::json normalize_module(const nlohmann::json & raw)
{
  Json functions;
  Json objects = Json::object();
  if (raw.is_array())
  {
    functions = raw;
  }
  else if (raw.is_object() && raw.contains("functions"))
  {
    functions = raw.at("functions");
    if (raw.contains("metadata") && raw["metadata"].is_object())
    {
      objects = raw["metadata"].value("objects", Json::object());
      if (!objects.is_object())
      {
        objects = Json::object();
      }
    }
  }
  else
  {
    throw std::invalid_argument(
      "APE root must be a function list or contain 'functions'");
  }
  if (!functions.is_array())
  {
    throw std::invalid_argument("APE root field 'functions' must be an array");
  }

  Json result = Json::array();
  for (auto function : functions)
  {
    Json body = Json::array();
    for (const auto & node : function.value("body", Json::array()))
    {
      body.push_back(enrich_node(node, objects));
    }
    function["body"] = std::move(body);
    result.push_back(std::move(function));
  }
  return result;
}

}  // namespace yarda
