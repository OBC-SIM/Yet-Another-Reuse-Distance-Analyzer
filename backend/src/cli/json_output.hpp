#pragma once

#include <string>

namespace yarda::cli
{

/**
 * @brief Write an already serialized document followed by one LF.
 * @param document Borrowed complete JSON text, without its trailing LF.
 * @param path Output file, or empty for the existing mapping stdout path.
 * @return Nothing.
 * @throws std::runtime_error for open, write or close failure.
 */
void write_json_document(const std::string & document,
                         const std::string & path);

} // namespace yarda::cli
