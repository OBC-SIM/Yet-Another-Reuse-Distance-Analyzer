#pragma once

#include <vector>

#include "yarda/elf/data_regions.hpp"

namespace llvm::object
{
class ELFObjectFileBase;
}

namespace yarda::detail
{

std::vector<ElfDataRegion>
parse_program_regions(const llvm::object::ELFObjectFileBase & object);

}  // namespace yarda::detail
