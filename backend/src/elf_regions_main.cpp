#include <exception>
#include <iostream>
#include <string>

#include "yarda/elf_data_regions.hpp"
#include "yarda/elf_data_regions_json.hpp"

namespace
{

void print_usage(std::ostream & output)
{
  output << "Usage: yarda_elf_regions ELF\n";
}

}  // namespace

int main(int argc, char ** argv)
{
  if (argc == 2 &&
      (std::string(argv[1]) == "--help" || std::string(argv[1]) == "-h"))
  {
    print_usage(std::cout);
    return 0;
  }
  if (argc != 2)
  {
    print_usage(std::cerr);
    return 2;
  }
  try
  {
    const auto image = yarda::parse_elf_data_regions(argv[1]);
    std::cout << yarda::elf_data_regions_json(image).dump(2) << '\n';
  }
  catch (const std::exception & error)
  {
    std::cerr << "yarda_elf_regions: " << error.what() << '\n';
    return 1;
  }
  return 0;
}
