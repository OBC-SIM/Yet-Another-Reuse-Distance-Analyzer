#include "yarda/elf/object_addresses.hpp"

#include <stdexcept>
#include <string>

namespace yarda
{

ObjectAddressModel build_elf_object_addresses(const ElfDataRegions & image)
{
  if (image.symbols.empty())
  {
    throw std::invalid_argument("ELF image has no object symbols");
  }
  ObjectAddressModel result;
  result.basis = image.image_relative ? AddressBasis::ImageRelative
                                      : AddressBasis::Absolute;
  for (const auto & symbol : image.symbols)
  {
    const auto object_id = "global::" + symbol.name;
    const ObjectAddress address{symbol.virtual_address, symbol.size};
    const auto [position, inserted] = result.objects.emplace(object_id, address);
    if (!inserted && (position->second.base != address.base ||
                      position->second.size != address.size))
    {
      throw std::invalid_argument("ambiguous ELF object symbol: " +
                                  symbol.name);
    }
  }
  return result;
}

}  // namespace yarda
