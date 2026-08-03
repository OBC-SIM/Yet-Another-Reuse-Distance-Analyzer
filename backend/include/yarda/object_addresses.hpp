#pragma once

#include <cstdint>
#include <map>
#include <string>

namespace yarda
{

/** @brief Basis used by addresses in an object-address model. */
enum class AddressBasis
{
  Absolute,
  ImageRelative,
};

/** @brief Linked address and byte extent of one storage object. */
struct ObjectAddress
{
  std::uint64_t base = 0;
  std::uint64_t size = 0;
};

/** @brief Canonical LAT object IDs mapped into one linked address space. */
struct ObjectAddressModel
{
  AddressBasis basis = AddressBasis::Absolute;
  std::map<std::string, ObjectAddress> objects;
};

}  // namespace yarda
