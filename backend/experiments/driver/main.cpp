#include "evaluation.hpp"
#include "measurements.hpp"

#include <iostream>

#include "cli/artifact_output.hpp"

int main(int argc, char ** argv)
{
  try
  {
    if (argc == 4 && std::string(argv[1]) == "summarize")
    {
      const auto index = yarda::evaluation::read_json(argv[2]);
      const auto result = yarda::evaluation::summarize_samples(index);
      auto inputs = index.get<std::vector<std::string>>();
      inputs.push_back(argv[2]);
      auto text = result.dump(2);
      yarda::cli::publish_json_artifacts(inputs, {{argv[3], std::move(text)}});
      return 0;
    }
    if (argc == 4)
      return yarda::evaluation::measure_case(argv[1], argv[2], argv[3]);
    throw std::invalid_argument(
      "usage: yarda_hierarchy_evaluate CASE.json MODE NEW_OUTPUT\n"
      "       yarda_hierarchy_evaluate summarize INDEX.json OUTPUT.json");
  }
  catch (const std::exception & error)
  {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
