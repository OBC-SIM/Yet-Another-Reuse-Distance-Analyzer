#include "json_output.hpp"

#include <fstream>
#include <iostream>
#include <stdexcept>

namespace yarda::cli
{

void write_json_document(const std::string & document, const std::string & path)
{
  if (path.empty())
  {
    std::cout << document << '\n';
    std::cout.flush();
    if (!std::cout)
    {
      throw std::runtime_error("failed to write JSON to stdout");
    }
    return;
  }
  std::ofstream output(path);
  if (!output)
  {
    throw std::runtime_error("cannot open export path: " + path);
  }
  output << document << '\n';
  output.close();
  if (!output)
  {
    throw std::runtime_error("failed to write export path: " + path);
  }
}

} // namespace yarda::cli
